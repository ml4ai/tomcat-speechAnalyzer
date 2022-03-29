#include "JsonBuilder.h"
#include "GlobalMosquittoListener.h"
#include "Mosquitto.h"
#include "arguments.h"
#include "base64.h"

#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>

#include <chrono>
#include <deque>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>
#include <smileapi/SMILEapi.h>
#include <string>
#include <thread>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

using google::cloud::speech::v1::StreamingRecognizeResponse;
using google::cloud::speech::v1::WordInfo;
using namespace std;

JsonBuilder::JsonBuilder() {}

JsonBuilder::~JsonBuilder() {}

void JsonBuilder::Initialize() {
    // Setup connection with external mosquitto broker
    this->mosquitto_client.connect(
        args.mqtt_host, args.mqtt_port, 1000, 1000, 1000);
    
    // Setup connection with internal mosquitto broker
    if(!this->args.disable_opensmile){
   	 this->mosquitto_client_internal.connect(this->args.mqtt_host_internal, this->args.mqtt_port_internal, 1000, 1000, 1000);
    }
    
    // Set the start time for the stream
    this->stream_start_time =
        boost::posix_time::microsec_clock::universal_time();
    this->stream_start_time_vosk =
        boost::posix_time::microsec_clock::universal_time();

}

void JsonBuilder::Shutdown() {
    // Close connection with external  mosquitto broker
    this->mosquitto_client.close();
    
    // Close connection with internal  mosquitto broker
    if(!this->args.disable_opensmile){
    	this->mosquitto_client_internal.close();
    }
}

// Data for handling google asr messages
void JsonBuilder::process_asr_message(StreamingRecognizeResponse response,
                                      string id) {
    nlohmann::json message;
    message["header"] = create_common_header("observation");
    message["msg"] = create_common_msg("asr:transcription");

    // Check results and alternatives size
    if (response.results_size() == 0 ||
        response.results(0).alternatives_size() == 0) {
        return;
    }

    message["data"]["text"] = response.results(0).alternatives(0).transcript();
    message["data"]["is_final"] = response.results(0).is_final();

    // Check that text isn't empty if is_final is true
    if (response.results(0).alternatives(0).words_size() == 0 &&
        message["data"]["is_final"]) {
        return;
    }

    message["data"]["asr_system"] = "google";
    message["data"]["participant_id"] = this->participant_id;
    message["data"]["id"] = id;
    message["data"]["utterance_id"] = id;

    if (!message["data"]["is_final"]) {

        // Handle is_initial field
        if (this->is_initial) {
            message["data"]["is_initial"] = true;
            this->utterance_start_timestamp =
                boost::posix_time::to_iso_extended_string(
                    boost::posix_time::microsec_clock::universal_time()) +
                "Z";
            this->is_initial = false;
        }
        else {
            // Stop processing if only publishig first intermediate
            return;
            message["data"]["is_initial"] = false;
        }

        // Add start timestamp
        message["data"]["start_timestamp"] = this->utterance_start_timestamp;

        // Publish message
        this->mosquitto_client.publish("agent/asr/intermediate",
                                       message.dump());
    }
    else {
        // Add transcription alternatvie
        vector<nlohmann::json> alternatives;
        auto result = response.results(0);
        for (int i = 0; i < result.alternatives_size(); i++) {
            auto alternative = result.alternatives(i);
            alternatives.push_back(nlohmann::json::object(
                {{"text", alternative.transcript()},
                 {"confidence", alternative.confidence()}}));
        }
        message["data"]["alternatives"] = alternatives;

	// Generate word messages
	vector<nlohmann::json> word_messages;
	auto utt = result.alternatives(0);
	for(WordInfo word : utt.words()){
		int64_t start_seconds = word.start_time().seconds();
		int32_t start_nanos = word.start_time().nanos();
        	int64_t end_seconds = word.end_time().seconds();
       		int32_t end_nanos = word.end_time().nanos();
		double start_time = this->sync_time + start_seconds + (start_nanos / 1000000000.0);
		double end_time = this->sync_time + end_seconds + (end_nanos / 1000000000.0);

		nlohmann::json word_message;
		word_message["word"] = word.word();	
		word_message["start_time"] = start_time;
		word_message["end_time"] = end_time;
		word_messages.push_back(word_message);	
	}
	message["data"]["features"]["word_messages"] = word_messages;

        // Calculate timestamps
        WordInfo f = utt.words(0);
        WordInfo l = utt.words(utt.words().size() - 1);

        int64_t start_seconds = f.start_time().seconds();
        int32_t start_nanos = f.start_time().nanos();
        int64_t end_seconds = l.start_time().seconds();
        int32_t end_nanos = l.start_time().nanos();
        boost::posix_time::ptime start_timestamp =
            this->stream_start_time +
            boost::posix_time::seconds(start_seconds) +
            boost::posix_time::nanoseconds(start_nanos);
        boost::posix_time::ptime end_timestamp =
            this->stream_start_time + boost::posix_time::seconds(end_seconds) +
            boost::posix_time::nanoseconds(end_nanos);

        message["data"]["start_timestamp"] =
            boost::posix_time::to_iso_extended_string(start_timestamp) + "Z";
        message["data"]["end_timestamp"] =
            boost::posix_time::to_iso_extended_string(end_timestamp) + "Z";

        // Publish message
        this->mosquitto_client.publish("agent/asr/final", message.dump());
        
	// Publish message internally
	if(!this->args.disable_opensmile){
		this->mosquitto_client_internal.publish("agent/asr/final", message.dump());
	}

        this->is_initial = true;
    }
}

// Data for handling vosk asr messages
void JsonBuilder::process_asr_message_vosk(std::string response) {
    try {
        nlohmann::json message;
        message["header"] = create_common_header("observation");
        message["msg"] = create_common_msg("asr:transcription");

        message["data"]["asr_system"] = "vosk";
        message["data"]["participant_id"] = this->participant_id;
        message["data"]["id"] =
            boost::uuids::to_string(boost::uuids::random_generator()());
        nlohmann::json response_message = nlohmann::json::parse(response);

        // Handle additional features for intermediate messages
        if (response_message.contains("partial")) {
            message["data"]["is_final"] = false;
            message["data"]["text"] = response_message["partial"];

            // Don't publish dead air messages
            string text = message["data"]["text"];
            if (text.compare("the") == 0) {
                return;
            }

            // Handle is_initial field
            if (this->is_initial) {
                message["data"]["is_initial"] = true;
                this->utterance_start_timestamp =
                    boost::posix_time::to_iso_extended_string(
                        boost::posix_time::microsec_clock::universal_time()) +
                    "Z";
                this->is_initial = false;
            }
            else {
                message["data"]["is_initial"] = false;
            }

            // Add start timestamp
            message["data"]["start_timestamp"] =
                this->utterance_start_timestamp;

            // Publish messsage
            this->mosquitto_client.publish("agent/asr/intermediate",
                                           message.dump());
        }
        else if (response_message.contains("alternatives")) {
            vector<nlohmann::json> alternatives =
                response_message["alternatives"];
            vector<nlohmann::json> words = alternatives[0]["result"];

            message["data"]["is_final"] = true;
            message["data"]["text"] =
                response_message["alternatives"][0]["text"];

            // Handle timestamps
            double start_offset = words[0]["start"];
            double end_offset = words[words.size() - 1]["end"];
            boost::posix_time::ptime start_timestamp =
                this->stream_start_time_vosk +
                boost::posix_time::seconds((int)start_offset);
            boost::posix_time::ptime end_timestamp =
                this->stream_start_time_vosk +
                boost::posix_time::seconds((int)end_offset);
            message["data"]["start_time"] =
                boost::posix_time::to_iso_extended_string(start_timestamp) +
                "Z";
            message["data"]["end_time"] =
                boost::posix_time::to_iso_extended_string(end_timestamp) + "Z";

            // Add transcription alternatives
            for (int i = 0; i < alternatives.size(); i++) {
                alternatives[i].erase("result");
            }
            message["data"]["alternatives"] = alternatives;

            // Publish message
            this->mosquitto_client.publish("agent/asr/final", message.dump());
            // Set is_initial to true
            this->is_initial = true;
        }
        else {
            return;
        }
    }
    catch (std::exception const& e) {
        BOOST_LOG_TRIVIAL(error)
            << "Error processing Vosk message: " << e.what();
        BOOST_LOG_TRIVIAL(error) << "The Vosk message was: " << response;
    }
}


void JsonBuilder::update_sync_time(double sync_time) {
    this->sync_time = sync_time;
    this->stream_start_time =
        boost::posix_time::microsec_clock::universal_time();
}


// Methods for creating common message types
nlohmann::json JsonBuilder::create_common_header(string message_type) {
    nlohmann::json header;
    string timestamp =
        boost::posix_time::to_iso_extended_string(
            boost::posix_time::microsec_clock::universal_time()) +
        "Z";

    header["timestamp"] = timestamp;
    header["message_type"] = message_type;
    header["version"] = "0.1";

    return header;
}

nlohmann::json JsonBuilder::create_common_msg(std::string sub_type) {
    nlohmann::json message;
    string timestamp =
        boost::posix_time::to_iso_extended_string(
            boost::posix_time::microsec_clock::universal_time()) +
        "Z";

    message["timestamp"] = timestamp;
    message["experiment_id"] = GLOBAL_LISTENER.experiment_id;
    message["trial_id"] = GLOBAL_LISTENER.trial_id;
    message["version"] = "4.0.0";
    message["source"] = "tomcat_speech_analyzer";
    message["sub_type"] = sub_type;

    return message;
}


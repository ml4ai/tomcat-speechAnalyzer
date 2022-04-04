#include "JsonBuilder.h"
#include "GlobalMosquittoListener.h"
#include "Mosquitto.h"
#include "arguments.h"
#include "base64.h"

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

using namespace std;

JsonBuilder::JsonBuilder() {}

JsonBuilder::~JsonBuilder() {}

void JsonBuilder::Initialize() {
    // Setup connection with mosquitto broker
    this->mosquitto_client.connect(
        args.mqtt_host, args.mqtt_port, 1000, 1000, 1000);
 
    // Set the start time for the stream
    this->stream_start_time =
        boost::posix_time::microsec_clock::universal_time();
    this->stream_start_time_vosk =
        boost::posix_time::microsec_clock::universal_time();

    // Initialize postgres connection
    this->postgres.initialize();
}

void JsonBuilder::Shutdown() {
    // Close connection with mosquitto broker
    this->mosquitto_client.close();

    // Close postgres connection
    this->postgres.shutdown();
}

void JsonBuilder::process_message(string message) {
    string temp(message);
    temp.erase(remove(temp.begin(), temp.end(), ' '), temp.end());
    if (tmeta) {
        if (temp.find("lld") != string::npos) {
            this->postgres.publish_chunk(this->opensmile_message);
            this->opensmile_message["data"]["participant_id"] =
                this->participant_id;
            this->opensmile_message["header"] =
                create_common_header("observation");
            this->opensmile_message["msg"] = create_common_msg("openSMILE");
            tmeta = false;
        }
        if (tmeta) {
            auto equals_index = temp.find('=');
            string field = temp.substr(0, equals_index);
            double value = atof(temp.substr(equals_index + 1).c_str());
            this->opensmile_message["data"]["tmeta"][field] = value;
        }
    }

    if (temp.find("lld") != string::npos) {
        std::string lld = temp.substr(4);
        auto equals_index = lld.find('=');
        string field = lld.substr(0, equals_index);
        double value = atof(lld.substr(equals_index + 1).c_str());

        // Replace '[' and ']' characters
        size_t open = field.find("[");
        size_t close = field.find("]");

        if (open != string::npos && close != string::npos) {
            field.replace(open, 1, "(");
            field.replace(close, 1, ")");
        }

        this->opensmile_message["data"]["features"]["lld"][field] = value;
    }

    if (temp.find("tmeta:") != string::npos) {
        tmeta = true;
    }
}

void JsonBuilder::process_sentiment_message(nlohmann::json m){
	nlohmann::json message = m;
	
	// Generate aligned features
	vector<nlohmann::json> word_messages;
	for(nlohmann::json word_message : m["data"]["features"]["word_messages"]){
		double start_time = word_message["start_time"];
		double end_time = word_message["end_time"];

		vector<nlohmann::json> history = this->postgres.features_between(start_time, end_time, m["data"]["participant_id"], m["msg"]["trial_id"]);
		nlohmann::json features_output;
		if(history.size() == 0){
			features_output = nullptr;
			word_message["features"] = nullptr;
		}
		else{
			for (auto& it : history[0].items()){
				features_output[it.key()] = vector<double>();
			}
			 // Load the features output from the history entries
            		for (auto entry : history) {
                		for (auto& it : history[0].items()) {
                    			features_output[it.key()].push_back(entry[it.key()]);
                		}
            		}
			word_message["features"] = features_output.dump();
		}
		word_messages.push_back(word_message);
	}
	message["data"]["features"]["word_messages"] = word_messages;

	// Send to MMC Server
	message["data"]["word_messages"] = message["data"]["features"]["word_messages"];
	string features = message.dump();
	string mmc = this->process_mmc_message(message.dump());
	try{
		message["data"]["sentiment"] = nlohmann::json::parse(mmc);
	}
	catch(std::exception e){
		std::cout << "Unable to process response from mmc server" << std::endl;
		return;
	}

	// Format and publish sentiment message
	nlohmann::json sentiment;
	sentiment["header"] = this->create_common_header("observation");
	sentiment["msg"] = this->create_common_msg("speech_analyzer:sentiment");
	sentiment["data"]["utterance_id"] = message["data"]["utterance_id"];
	sentiment["data"]["sentiment"]["emotions"] = message["data"]["sentiment"]["emotions"];
	sentiment["data"]["sentiment"]["penultimate_emotions"] = message["data"]["sentiment"]["penultimate_emotions"];
	this->mosquitto_client.publish("agent/speech_analyzer/sentiment", sentiment.dump());
	
	// Format and publish personality message
	nlohmann::json personality;
	personality["header"] = this->create_common_header("observation");
	personality["msg"] = this->create_common_msg("speech_analyzer:personality");
	personality["data"]["utterance_id"] = message["data"]["utterance_id"];
	sentiment["data"]["personality"]["traits"] = message["data"]["sentiment"]["traits"];
	sentiment["data"]["personality"]["penultimate_traits"] = message["data"]["sentiment"]["penultimate_traits"];
	this->mosquitto_client.publish("agent/speech_analyzer/personality", personality.dump());



}

// Data for handling word/feature alignment messages
string JsonBuilder::process_mmc_message(string message) {
    try {
        string host = "mmc";
        string port = "8001";
        string target = "/encode";
        int version = 11;

        net::io_context ioc;

        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);

        // Look up the domain name
        auto const results = resolver.resolve(host, port);

        // Make the connection on the IP address we get from a lookup
        stream.connect(results);

        // Set up an HTTP GET request message
        http::request<http::string_body> req{http::verb::get, target, version};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.body() = message;
        req.prepare_payload();

        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::string_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);

        // Write the message to standard out

        // Gracefully close the socket
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if (ec && ec != beast::errc::not_connected)
            throw beast::system_error{ec};

        return res.body().data();
    }
    catch (std::exception const& e) {
        BOOST_LOG_TRIVIAL(error) << e.what();
    }
    return "";
}

void JsonBuilder::process_audio_chunk_message(vector<char> chunk, string id) {
    // Encode audio chunk
    int encoded_data_length = Base64encode_len(chunk.size());
    char output[encoded_data_length];
    Base64encode(output, &chunk[0], chunk.size());
    string encoded(output);

    // Create message
    nlohmann::json message;
    message["header"] = create_common_header("observation");
    message["msg"] = create_common_msg("audio_chunk");
    message["data"]["chunk"] = encoded;
    message["data"]["id"] = id;
    this->mosquitto_client.publish("audio/chunk", message.dump());
}

void JsonBuilder::process_audio_chunk_metadata_message(vector<char> chunk,
                                                       string id) {
    // Check if in trial
    if (!GLOBAL_LISTENER.in_trial) {
        return;
    }
    nlohmann::json message;
    message["header"] = create_common_header("metadata");
    message["msg"] = create_common_msg("audio");
    message["data"]["size"] = chunk.size();
    message["data"]["format"] = "int16";
    message["data"]["id"] = id;
    message["data"]["participant_id"] = this->participant_id;
    this->mosquitto_client.publish("agent/asr/metadata", message.dump());
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

void JsonBuilder::strip_mmc_message(nlohmann::json& message) {
    // Remove buggy speaker field
    message["data"]["sentiment"].erase("speaker");
    
    // Remove features fields
    message["data"].erase("features");
    message["data"].erase("word_messages");

    // Remove ASR Fields 
    message["data"].erase("alternatives");
    message["data"].erase("asr_system");
    message["data"].erase("is_final");
    message["data"].erase("text");
    message["data"].erase("start_timestamp");
    message["data"].erase("end_timestamp");
}

void JsonBuilder::strip_features_message(nlohmann::json& message) {
}

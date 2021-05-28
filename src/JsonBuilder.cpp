#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <regex>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "JsonBuilder.h"
#include "Mosquitto.h"
#include "GlobalMosquittoListener.h"
#include "arguments.h"
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include "version.h"
#include "base64.h"
#include <nlohmann/json.hpp>
#include <smileapi/SMILEapi.h>

using google::cloud::speech::v1::StreamingRecognizeResponse;
using google::cloud::speech::v1::WordInfo;
using namespace std;

JsonBuilder::JsonBuilder() {
    // Setup connection with mosquitto broker
    this->mosquitto_client.connect(
        args.mqtt_host, args.mqtt_port, 1000, 1000, 1000);
    this->listener_client.connect(
        args.mqtt_host, args.mqtt_port, 1000, 1000, 1000);

    // Listen for trial id and experiment id
    this->listener_client.subscribe("trial");
    this->listener_client.subscribe("experiment");
    this->listener_client.set_max_seconds_without_messages(
        2147483647); // Max Long value
    this->listener_client_thread = thread([this] { listener_client.loop(); });

   // Set the start time for the stream
   this->stream_start_time = boost::posix_time::microsec_clock::universal_time(); 
}

JsonBuilder::~JsonBuilder() {
    // Close connection with mosquitto broker
    this->mosquitto_client.close();
    this->listener_client.close();
    this->listener_client_thread.join();
}

void JsonBuilder::process_message(smilelogmsg_t message) {
    string temp(message.text);
    temp.erase(remove(temp.begin(), temp.end(), ' '), temp.end());
    if (tmeta) {
        if (temp.find("lld") != string::npos) {
            this->opensmile_history.push_back(this->opensmile_message);
            this->opensmile_message["header"] = create_common_header("observation");
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
        auto dot_index = temp.find('.');
        auto equals_index = temp.find('=');
        string field = temp.substr(dot_index + 1, equals_index - dot_index - 1);
	double value = atof(temp.substr(equals_index + 1).c_str());

	// Replace '[' and ']' characters 
	size_t open = field.find("[");
	size_t close = field.find("]");

	if(open!=string::npos && close!=string::npos){	
		field.replace(open, 1, "(");
		field.replace(close, 1, ")");
	}

	this->opensmile_message["data"]["features"]["lld"][field] = value;
        if (!count(
                this->feature_list.begin(), this->feature_list.end(), field)) {
            feature_list.push_back(field);
        }
    }

    if (temp.find("tmeta:") != string::npos) {
        tmeta = true;
    }
}

// Data for handling google asr messages
void JsonBuilder::process_asr_message(StreamingRecognizeResponse response,
                                      string id) {

    nlohmann::json message;
    message["header"] = create_common_header("observation");
    message["msg"] = create_common_msg("asr");

    message["data"]["text"] = response.results(0).alternatives(0).transcript();
    message["data"]["is_final"] = response.results(0).is_final();
    message["data"]["asr_system"] = "google";
    message["data"]["participant_id"] = this->participant_id;
    message["data"]["id"] = id;

    // Add transcription alternatvies
    vector<nlohmann::json> alternatives;
    auto result = response.results(0);
    for (int i = 0; i < result.alternatives_size(); i++) {
        auto alternative = result.alternatives(i);
        alternatives.push_back(
            nlohmann::json::object({{"text", alternative.transcript()},
                                    {"confidence", alternative.confidence()}}));
    }
    message["data"]["alternatives"] = alternatives;
    
    // Calculate timestamps
    if(response.results(0).is_final()){
	    auto utt = result.alternatives(0);
	    WordInfo f = utt.words(0);
	    WordInfo l = utt.words(utt.words().size()-1); 
	    
	    int64_t start_seconds = f.start_time().seconds();
	    int32_t start_nanos = f.start_time().nanos();
	    int64_t end_seconds = l.start_time().seconds();
	    int32_t end_nanos = l.start_time().nanos();
	    
	    boost::posix_time::ptime start_timestamp = this->stream_start_time + boost::posix_time::seconds(start_seconds) + boost::posix_time::nanoseconds(start_nanos);
	    boost::posix_time::ptime end_timestamp = this->stream_start_time + boost::posix_time::seconds(end_seconds) + boost::posix_time::nanoseconds(end_nanos);

	    message["data"]["start_timestamp"] = boost::posix_time::to_iso_extended_string(start_timestamp) + "Z";
	    message["data"]["end_timestamp"] = boost::posix_time::to_iso_extended_string(end_timestamp) + "Z";
    }
    // Publish message
    if (message["data"]["is_final"]) {
        this->mosquitto_client.publish("agent/asr/final", message.dump());
    }
    else {
        this->mosquitto_client.publish("agent/asr/intermediate",
                                       message.dump());
    }
}

// Data for handling word/feature alignment messages
void JsonBuilder::process_alignment_message(StreamingRecognizeResponse response,
                                            string id) {
    

    nlohmann::json message;
    message["header"] = create_common_header("observation");
    message["msg"] = create_common_msg("alignment");

    auto result = response.results(0);
    for (int i = 0; i < result.alternatives_size(); i++) {
        auto alternative = result.alternatives(i);
        for (WordInfo word : alternative.words()) {
            int64_t start_seconds = word.start_time().seconds();
            int32_t start_nanos = word.start_time().nanos();
            int64_t end_seconds = word.end_time().seconds();
            int32_t end_nanos = word.end_time().nanos();

            double start_time =
                this->sync_time + start_seconds + (start_nanos / 1000000000.0);
            double end_time =
                this->sync_time + end_seconds + (end_nanos / 1000000000.0);
            string current_word = word.word();
            // Get extracted features message history
            vector<nlohmann::json> history =
                this->features_between(start_time, end_time);
            // Initialize the features output by creating a vector for each
            // feature
            nlohmann::json features_output;
            if (history.size() == 0) {
                features_output = nullptr;
            }
            else {
                for (auto& it : history[0].items()) {
                    features_output[it.key()] = vector<double>();
                }
                // Load the features output from the history entries
                for (auto entry : history) {
                    for (auto& it : history[0].items()) {
                        features_output[it.key()].push_back(entry[it.key()]);
                    }
                }
            }
            message["data"]["word"] = current_word;
            message["data"]["start_time"] = start_time;
            message["data"]["end_time"] = end_time;
            message["data"]["features"] = features_output;
            message["data"]["id"] = id;
            message["data"]["time_interval"] = 0.01;
	    this->mosquitto_client.publish("word/feature", message.dump());
        }
    }
}

void JsonBuilder::process_audio_chunk_message(vector<char> chunk, string id){
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

void JsonBuilder::process_audio_chunk_metadata_message(vector<char> chunk, string id){
    // Check if in trial
    if (!GLOBAL_LISTENER.in_trial){
	return;
    }
	nlohmann::json message;
    	message["header"] = create_common_header("metadata");
   	message["msg"] = create_common_msg("audio");
	message["data"]["size"] = chunk.size();
	message["data"]["format"] = "int16";
	message["data"]["id"] = id;	
	this->mosquitto_client.publish("metadata/audio", message.dump());	
}

void JsonBuilder::update_sync_time(double sync_time) {
    this->sync_time = sync_time;
   this->stream_start_time = boost::posix_time::microsec_clock::universal_time(); 
}

vector<nlohmann::json> JsonBuilder::features_between(double start_time,
                                                     double end_time) {
    vector<nlohmann::json> out;
    for (int i = 0; i < opensmile_history.size(); i++) {
        float time = opensmile_history[i]["data"]["tmeta"]["time"];
        if (time > start_time && time < end_time) {
            out.push_back(opensmile_history[i]["data"]["features"]["lld"]);
        }
    }
    return out;
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
    message["version"] = "0.1";
    message["source"] = "tomcat_speech_analyzer";
    message["sub_type"] = sub_type;

    return message;
}

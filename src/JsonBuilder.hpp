#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <string>
#include "version.h"
#include "Mosquitto.h"
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include "SMILEapi.h"
using google::cloud::speech::v1::WordInfo;
using google::cloud::speech::v1::StreamingRecognizeResponse;

class JsonBuilder{
        
	//////////////////////////////////////////////////////////////////////////////////////////
	public:
	JsonBuilder(){
		//Setup connection with mosquitto broker
		this->mosquitto_client.connect("mosquitto", 1883, 1000, 1000, 1000);
	/*	this->listener_client.connect("127.0.0.1", 5556, 1000, 1000, 1000);
		//Listen for trial id and experiment id
		this->listener_client.subscribe("trial");
		this->listener_client.subscribe("experiment");
		this->listener_client_thread = std::thread( [this] { listener_client.loop(); } );
		*/
	}
	
	~JsonBuilder(){
		//Close connectino with mosquitto broker
		this->mosquitto_client.close();
		//this->listener_client.close();
		//this->listener_client_thread.join();
	}
	//////////////////////////////////////////////////////////////////////////////////////////
	
	void process_message(smilelogmsg_t message){
                std::string temp(message.text);
                temp.erase(std::remove(temp.begin(), temp.end(), ' '), temp.end());
                if(tmeta){
                        if(temp.find("lld") != std::string::npos){
				this->mosquitto_client.publish("agent/uaz/speechAnalyzer/vocalicFeatures", opensmile_message.dump());
				this->opensmile_history.push_back(this->opensmile_message);
                                this->opensmile_message["header"] = create_common_header();
				this->opensmile_message["msg"] = create_common_msg();
                                tmeta = false;
                        }
                        if(tmeta){
                                auto equals_index = temp.find('=');
                                std::string field = temp.substr(0, equals_index);
                                double value = std::atof(temp.substr(equals_index+1).c_str());	
                                this->opensmile_message["data"]["tmeta"][field] = value;
                        }
                }

                if(temp.find("lld") != std::string::npos){
                        auto dot_index = temp.find('.');
                        auto equals_index = temp.find('=');
                        std::string field = temp.substr(dot_index+1, equals_index-dot_index-1);
                        double value = std::atof(temp.substr(equals_index+1).c_str());

                        this->opensmile_message["data"]["features"]["lld"][field] = value;
			if(!std::count(this->feature_list.begin(), this->feature_list.end(), field)){
				feature_list.push_back(field);	
			}
                }

                if(temp.find("tmeta:") != std::string::npos){
                        tmeta = true;
                }
        }

	
        //Data for handling google asr messages
        void process_asr_message(StreamingRecognizeResponse response, std::string id){
                nlohmann::json message;
                message["header"] = create_common_header();
                message["msg"] = create_common_msg();

                message["data"]["text"] = response.results(0).alternatives(0).transcript();
                message["data"]["is_final"] = response.results(0).is_final();
                message["data"]["asr_system"] = "google";
                message["data"]["participant_id"] = nullptr;
		message["data"]["id"] = id;
                
		// Add transcription alternatvies
                std::vector<nlohmann::json> alternatives;
                auto result = response.results(0);
                for(int i=0;i<result.alternatives_size();i++){
                        auto alternative = result.alternatives(i);
                        alternatives.push_back(nlohmann::json::object({{"text", alternative.transcript()},{"confidence", alternative.confidence()}}));
                }
                message["data"]["alternatives"] = alternatives;
                this->mosquitto_client.publish("agent/asr", message.dump());
	}

        //Data for handling word/feature alignment messages
        void process_alignment_message(StreamingRecognizeResponse response, std::string id){
		nlohmann::json message;
                message["header"] = create_common_header();
                message["msg"] = create_common_msg();
		
		auto result = response.results(0);
            	for(int i=0; i<result.alternatives_size(); i++){
                	auto alternative = result.alternatives(i);
			for(WordInfo word : alternative.words()){
				int64_t start_seconds = word.start_time().seconds();
				int32_t start_nanos = word.start_time().nanos();
				int64_t end_seconds = word.end_time().seconds();
				int32_t end_nanos = word.end_time().nanos();

				double start_time = this->sync_time + start_seconds + (start_nanos/1000000000.0);
				double end_time = this->sync_time + end_seconds + (end_nanos/1000000000.0);
				std::string current_word = word.word();
				// Get extracted features message history	
				std::vector<nlohmann::json> history = this->features_between(start_time, end_time);
				// Initialize the features output by creating a vector for each feature
				nlohmann::json features_output;
				if(history.size() == 0){
					features_output = nullptr;
				}
				else{
					for(auto& it : history[0].items()){
						features_output[it.key()] = std::vector<double>();
					} 
					// Load the features output from the history entries
					for(auto entry : history){
						for(auto& it : history[0].items()){
							features_output[it.key()].push_back(entry[it.key()]);
						}
					}
				}
				message["data"]["word"] = current_word;
				message["data"]["start_time"] = start_time;
				message["data"]["end_time"] = end_time;
				message["data"]["features"] = features_output;
				message["data"]["configuration"] = std::string(GIT_COMMIT);
				message["data"]["id"] = id;
				message["data"]["time_interval"] = 0.01;
				this->mosquitto_client.publish("word/feature", message.dump());
			}
            	}
        }

	void update_sync_time(double sync_time){
		this->sync_time = sync_time;
	}
	//////////////////////////////////////////////////////////////////////////////////////////
        private:		
	Mosquitto mosquitto_client;
	//TrialListenerClient listener_client;
	//std::thread listener_client_thread;
	
        //Data for handling opensmile messages
        nlohmann::json opensmile_message;
	std::vector<std::string> feature_list;
	bool tmeta = false;
	double sync_time = 0.0;
        std::vector<nlohmann::json> opensmile_history;
        std::vector<nlohmann::json> features_between(double start_time, double end_time){
		std::vector<nlohmann::json> out;
                for(int i=0;i<opensmile_history.size();i++){
                        float time = opensmile_history[i]["data"]["tmeta"]["time"];
                        if(time > start_time && time < end_time){
                                out.push_back(opensmile_history[i]["data"]["features"]["lld"]);
                        }
                }

                return out;
        }
	

	//Methods for creating common message types	
        nlohmann::json create_common_header(){
		nlohmann::json header;
                std::string timestamp = boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time()) + "Z";

                header["timestamp"] = timestamp;
                header["message_type"] = "observation";
                header["version"] = "0.1";

		return header;
        }

	nlohmann::json create_common_msg(){
		nlohmann::json message;
                std::string timestamp = boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time()) + "Z";

                message["timestamp"] = timestamp;
                message["experiment_id"] = nullptr;// listener_client.experiment_id;
                message["trial_id"] = nullptr; //listener_client.trial_id;
                message["version"] = "0.1";
                message["source"] = "tomcat_speech_analyzer";
                message["sub_type"] = "speech_analysis";
		
		return message;
	}

};

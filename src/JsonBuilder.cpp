#include <smileapi/SMILEapi.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include "Mosquitto.h"
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
using google::cloud::speech::v1::StreamingRecognizeResponse;
class JsonBuilder{
        
	//////////////////////////////////////////////////////////////////////////////////////////
	public:
	JsonBuilder(){
		//Setup connection with mosquitto broker
		this->mosquitto_client.connect("127.0.0.1", 5556, 1000, 1000, 1000);
	}	
	~JsonBuilder(){
		//Close connectino with mosquitto broker
		this->mosquitto_client.close();
	}
        
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
                }

                if(temp.find("tmeta:") != std::string::npos){
                        tmeta = true;
                }
        }

	
        //Data for handling google asr messages
        void process_asr_message(StreamingRecognizeResponse response){
                nlohmann::json message;
                message["header"] = create_common_header();
                message["msg"] = create_common_msg();

                message["data"]["text"] = response.results(0).alternatives(0).transcript();
                message["data"]["is_final"] = response.results(0).is_final();
                message["data"]["asr_system"] = "google";
                message["data"]["participant_id"] = nullptr;

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
        void process_alignment_message(StreamingRecognizeResponse response){
		nlohmann::json message;
                message["header"] = create_common_header();
                message["msg"] = create_common_msg();
        }

	//////////////////////////////////////////////////////////////////////////////////////////
        private:		
	Mosquitto mosquitto_client;

        //Data for handling opensmile messages
        nlohmann::json opensmile_message;
        std::vector<nlohmann::json> opensmile_history;
        std::vector<nlohmann::json> features_between(float start_time, float end_time){
                std::vector<nlohmann::json> out;
                for(int i=0;i<opensmile_history.size();i++){
                        float time = opensmile_history[i]["data"]["tmeta"]["time"];
                        if(time > start_time && time < end_time){
                                out.push_back(opensmile_history[i]["data"]["features"]);
                        }
                }

                return out;
        }


	
	//General class data
	bool tmeta = false;
	
        static nlohmann::json create_common_header(){
		nlohmann::json header;
                std::string timestamp = boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time()) + "Z";

                header["timestamp"] = timestamp;
                header["message_type"] = "observation";
                header["version"] = "0.1";

		return header;
        }

	static nlohmann::json create_common_msg(){
		nlohmann::json message;
                std::string timestamp = boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time()) + "Z";

                message["timestamp"] = timestamp;
                message["experiment_id"] = nullptr;
                message["trial_id"] = nullptr;
                message["version"] = "0.1";
                message["source"] = "tomcat_speech_analyzer";
                message["sub_type"] = "speech_analysis";
		
		return message;
	}

};

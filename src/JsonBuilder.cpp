#include <boost/date_time/posix_time/posix_time.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include "SMILEapi.h"
class JsonBuilder{

        public:
        JsonBuilder(){
                this->j["header"] = {};
                this->j["msg"] = {};
                this->j["data"] = {};
                this->j["data"]["features"]["lld"] = {};
                this->j["data"]["tmeta"] = {};

        }
        void process_message(smilelogmsg_t message){
                std::string temp(message.text);
                temp.erase(std::remove(temp.begin(), temp.end(), ' '), temp.end());
                if(tmeta){
                        if(temp.find("lld") != std::string::npos){
				//this->history.push_back(j);
                                std::cout << j << std::endl;
                                tmeta = false;
                                create_header();
                        }
                        if(tmeta){
                                auto equals_index = temp.find('=');
                                std::string field = temp.substr(0, equals_index);
                                double value = std::atof(temp.substr(equals_index+1).c_str());

                                this->j["data"]["tmeta"][field] = value;
                        }
                }

                if(temp.find("lld") != std::string::npos){
                        auto dot_index = temp.find('.');
                        auto equals_index = temp.find('=');
                        std::string field = temp.substr(dot_index+1, equals_index-dot_index-1);
                        double value = std::atof(temp.substr(equals_index+1).c_str());

                        this->j["data"]["features"]["lld"][field] = value;
                }

                if(temp.find("tmeta:") != std::string::npos){
                        tmeta = true;
                }
        }

        private:
	bool tmeta = false;
        nlohmann::json j;
	
        void create_header(){
                std::string timestamp = boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time()) + "Z";

                j["header"]["timestamp"] = timestamp;
                j["header"]["message_type"] = "observation";
                j["header"]["version"] = "0.1";

                j["msg"]["timestamp"] = timestamp;
                j["msg"]["experiment_id"] = nullptr;
                j["msg"]["trial_id"] = nullptr;
                j["msg"]["version"] = "0.1";
                j["msg"]["source"] = "tomcat_speech_analyzer";
                j["msg"]["sub_type"] = "speech_analysis";
        }

	// Keep track of previous extractions
	std::vector<nlohmann::json> history;
	int history_index = 0;
	std::vector<nlohmann::json> features_between(float start_time, float end_time){
		std::vector<nlohmann::json> out;
		for(int i=0;i<history.size();i++){
			float time = history[i]["data"]["tmeta"]["time"];
			if(time > start_time && time < end_time){
				out.push_back(history[i]);
			}
		}

		return out;
	}

};

#include "SMILEapi.h"

#include "json.hpp"

#include <string>
#include <cstring>
#include <iostream>
class JsonBuilder{

	public:
	JsonBuilder(){
		this->j["lld"] = {};
		this->j["tmeta"] = {};

	}

	void process_message(smilelogmsg_t message){
		std::string temp(message.text);
		temp.erase(std::remove(temp.begin(), temp.end(), ' '), temp.end()); 
		if(tmeta){
			if(temp.find("lld") != std::string::npos){
				std::cout << j << std::endl;
				tmeta = false;
			}
			if(tmeta){
				auto equals_index = temp.find('=');
				std::string field = temp.substr(0, equals_index);
				double value = std::atof(temp.substr(equals_index+1).c_str());

				this->j["tmeta"][field] = value;
			}
		}

		if(temp.find("lld") != std::string::npos){
			auto dot_index = temp.find('.');
			auto equals_index = temp.find('=');
			std::string field = temp.substr(dot_index+1, equals_index-dot_index-1);
			double value = std::atof(temp.substr(equals_index+1).c_str());

			this->j["lld"][field] = value;
		}
		
		if(temp.find("tmeta:") != std::string::npos){
			tmeta = true;
		}	
	}
	
	private:
	bool tmeta = false;
	nlohmann::json j;

};

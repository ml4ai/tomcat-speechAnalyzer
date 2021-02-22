#include "SMILEapi.h"

#include "json.hpp"

#include <regex>
#include <string>
#include <thread>
#include <cerrno>  //errno
#include <cstring> //strerror
#include <iostream>
#include <vector>
#include <fstream>
const size_t MAX_NUM_SAMPLES = 512;

void parse_audio_data(smileobj_t*);
void log_callback(smileobj_t*, smilelogmsg_t, void*);

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

int main() {

    //JsonBuilder object which will be passed to openSMILE log callback
    JsonBuilder builder;

    //Initialize and start opensmile
    smileobj_t* handle;

    handle = smile_new();
    smile_initialize(handle, "conf/is09-13/IS13_ComParE.conf", 0, NULL, 1, 0, 0, 0);
    smile_set_log_callback(handle, &log_callback, &builder);
    std::thread thread_object(smile_run, handle);
    std::thread thread_object2(parse_audio_data, handle);

    thread_object.join();
    thread_object2.join();
    return 0;
}

void parse_audio_data(smileobj_t* handle){
    try {
        std::freopen(nullptr, "rb", stdin); // reopen stdin in binary mode

        if (std::ferror(stdin)) {
            throw(std::runtime_error(std::strerror(errno)));
        }
	
	float chunk[MAX_NUM_SAMPLES];
        std::size_t length;
        while ((length = std::fread(chunk, sizeof(float), MAX_NUM_SAMPLES, stdin)) > 0) {
		if(length < MAX_NUM_SAMPLES){
			memset(&chunk[length],0,(MAX_NUM_SAMPLES-length)*sizeof(float));
		}
		while(true){
			smileres_t result = smile_extaudiosource_write_data(handle, "externalAudioSource", (void*)chunk, length*sizeof(float));
			if(result == SMILE_SUCCESS){
				break;
			}
		}
	}
    }
    catch (std::exception const& e) {
        std::cerr << e.what() << std::endl;
    }
    
    smile_extaudiosource_set_external_eoi(handle, "externalAudioSource");

}

void log_callback(smileobj_t* smileobj, smilelogmsg_t message, void* param){

	JsonBuilder *builder = (JsonBuilder*)(param);
	builder->process_message(message);
}

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

int main() {
    //Initialize and start opensmile
    smileobj_t* handle;

    handle = smile_new();
    smile_initialize(handle, "conf/external_audio_test.conf", 0, NULL, 1, 0, 0, 0);
    smile_set_log_callback(handle, &log_callback, nullptr);
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
	std::string in(message.text);
	std::regex rgx("energy.pcm_LOGenergy = (.*)");
	std::smatch matches;
	if(std::regex_search(in, matches, rgx)){
		nlohmann::json j;
		j["features"]["pcm_LOGenergy"] = matches[1].str();
		std::cout <<  j  << std::endl;	
	}
}

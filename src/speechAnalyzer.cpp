#include "SMILEapi.h"
#include <thread>
#include <cerrno>  //errno
#include <cstring> //strerror
#include <iostream>
#include <vector>

const size_t MAX_CHUNK_SIZE = 2048;


int main() {

    //Initialize and start opensmile
    smileobj_t* handle;
    smileopt_t options[1];

    options[0].name = "O";
    options[0].value = "opensmile.energy.csv";

    handle = smile_new();
    smile_initialize(handle, "external_audio_test.conf", 1, options, 1, 0, 1, 0);
    std::thread thread_object(smile_run, handle);

    try {
        std::freopen(nullptr, "rb", stdin); // reopen stdin in binary mode

        if (std::ferror(stdin)) {
            throw(std::runtime_error(std::strerror(errno)));
        }
	
	float chunk[MAX_CHUNK_SIZE];
        std::size_t length;
        while ((length = std::fread(chunk, sizeof(float), MAX_CHUNK_SIZE, stdin)) > 0) {
		if(length < MAX_CHUNK_SIZE){
			memset(&chunk[length],0,MAX_CHUNK_SIZE-length);
		}
		while(true){
			smileres_t result = smile_extaudiosource_write_data(handle, "externalAudioSource", (void*)chunk, MAX_CHUNK_SIZE);
			if(result == SMILE_SUCCESS){
				break;
			}
		}
	}
    }
    catch (std::exception const& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    smile_extaudiosource_set_external_eoi(handle, "externalAudioSource");
    thread_object.join();
    return EXIT_SUCCESS;
}


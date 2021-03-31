#include "SMILEapi.h"
#include <fstream>
#include <iostream>
#include <thread>

int main(){

	//Extract data from audio.raw
	float data[2048];
	std::ifstream file("audio.raw", std::ios::in | std::ios::binary);
	int i=0;
	while(file.read(reinterpret_cast<char*>(&data[i]), sizeof(float)) && i<2048){
		i++;
	}

	//Set up opensmile options
	smileobj_t* handle;
	smileopt_t options[2];

	options[0].name = "I";
	options[0].value = "opensmile.wav";
	options[1].name = "O";
	options[1].value = "opensmile.energy.csv";

	//Run SMILExtract
        handle = smile_new();
	smile_initialize(handle, "demo1_energy.conf", 2, options, 2, 0, 1, 0);
	
	std::thread thread_object(smile_run, handle);
	std::cout << smile_extaudiosource_write_data(handle, "externalAudioSource", (void*)data, 2048) << std::endl;//iloveu
	smile_extaudiosource_set_external_eoi(handle, "externalAudioSource");
	thread_object.join();	
	
	return 0;
}



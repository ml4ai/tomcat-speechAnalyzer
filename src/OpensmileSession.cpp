#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <thread>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <boost/log/trivial.hpp>

#include "GlobalMosquittoListener.h"
#include "base64.h"
#include "JsonBuilder.h"
#include "util.h"
#include <smileapi/SMILEapi.h>

#include "OpensmileSession.h"

OpensmileSession::OpensmileSession(string participant_id, string mqtt_host_internal, int mqtt_port_internal) { 
    this->participant_id = participant_id;
    this->mqtt_host_internal = mqtt_host_internal;
    this->mqtt_port_internal = mqtt_port_internal;
    this->pid = fork();
    if(!pid){
	    this->Initialize();
	    this->Loop();
    }
}

OpensmileSession::~OpensmileSession(){
	// If child process shutdown
	if(this->running){
		this->Shutdown();
	}

	// If parent process kill child
	kill(this->pid, SIGTERM);
}

void OpensmileSession::Initialize(){
    this->running = true;
    
    // Initialize JsonBuilder
    BOOST_LOG_TRIVIAL(info) << "Initializing JsonBuilder for: " << this->participant_id;
    this->builder = new JsonBuilder();
    this->builder->participant_id = this->participant_id;
    this->builder->Initialize();
    
    // Initialize Opensmile session
    BOOST_LOG_TRIVIAL(info) << "Initializing Opensmile Session for: " << this->participant_id;
    this->handle = smile_new();
    smileopt_t* options = NULL;
    smile_initialize(this->handle,
                     "conf/is09-13/IS13_ComParE.conf",
                     0,
                     options,
                     1,
                     0, // Debug
                     0, // Console Output
                     0);
    smile_set_log_callback(this->handle, &log_callback, this->builder);
    this->opensmile_thread = std::thread(smile_run, this->handle);
    

    // Connect to broker
        BOOST_LOG_TRIVIAL(info) << "Initializing Mosquitto connection for: " << this->participant_id;
        this->connect(this->mqtt_host_internal, this->mqtt_port_internal, 1000, 1000, 1000);
        this->subscribe(this->participant_id);	
        this->set_max_seconds_without_messages(10000);
        this->listener_thread = thread([this] { this->loop(); });

}

void OpensmileSession::Shutdown(){
	this->running = false;

	// Close listening session
	this->close();
    	
	// Close Opensmile Session
	smile_extaudiosource_set_external_eoi(this->handle, "externalAudioSource");
    	smile_free(this->handle);
	this->opensmile_thread.join();

	// Close process
	exit(0);
}

void OpensmileSession::Loop(){
	chrono::milliseconds duration(1);
	while(true){
		this_thread::yield();
        	this_thread::sleep_for(duration);
	}
}

void OpensmileSession::PublishChunk(vector<float> float_chunk){
	while(true) {
		smileres_t result =
			smile_extaudiosource_write_data(this->handle,
							"externalAudioSource",
							(void*)&float_chunk[0],
							4096 * sizeof(float));
		if (result == SMILE_SUCCESS) {
			break;
		}	
	}
}

void OpensmileSession::on_message(const std::string& topic,const std::string& message){
	nlohmann::json m = nlohmann::json::parse(message);

	// Decode base64 chunk
	string coded_src = m["chunk"];
	int encoded_data_length = Base64decode_len(coded_src.c_str());
	vector<char> decoded(encoded_data_length);
	Base64decode(&decoded[0], coded_src.c_str());
	vector<float> float_chunk(decoded.size()/sizeof(float));
	memcpy(&float_chunk[0], &decoded[0], decoded.size());
	
	this->PublishChunk(float_chunk);
}

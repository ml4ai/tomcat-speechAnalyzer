// STDLIB
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Third party
#include <boost/log/trivial.hpp>
#include <smileapi/SMILEapi.h>

// Local
#include "base64.h"
#include "util.h"
#include "OpensmileProcessor.h"
#include "OpensmileSession.h"

using namespace std;

OpensmileSession::OpensmileSession(string participant_id,
                                   string mqtt_host_internal,
                                   int mqtt_port_internal,
				   string trial_id,
				   string experiment_id) {
    this->participant_id = participant_id;
    this->trial_id = trial_id;
    this->experiment_id = experiment_id;
    
    this->mqtt_host_internal = mqtt_host_internal;
    this->mqtt_port_internal = mqtt_port_internal;
    
    pid = fork();
    if (!pid) {
	    // Connect to broker
	    connect(
		mqtt_host_internal, mqtt_port_internal, 1000, 1000, 1000);
	    subscribe(participant_id);
	    set_max_seconds_without_messages(10000);
	    listener_thread = thread([this] { this->loop(); });
	   
	    // Create processor for processing Opensmile log messages  
	    processor = new OpensmileProcessor(participant_id, trial_id, experiment_id);
       
	   // Run Opensmile session 
	    handle = smile_new();
	    smileopt_t* options = NULL;
	    smile_initialize(handle,
			     "conf/is09-13/IS13_ComParE.conf",
			     0,
			     options,
			     1,
			     0, // Debug
			     0, // Console Output
			     0);
	    smile_set_log_callback(handle, &log_callback, processor);
	    smile_run(handle);
    }
}

OpensmileSession::~OpensmileSession(){
    if(pid){
    	kill(pid, SIGTERM);
    }
}

void OpensmileSession::PublishChunk(const vector<float>& float_chunk) {
    while (true) {
        smileres_t result =
            smile_extaudiosource_write_data(handle,
                                            "externalAudioSource",
                                            (void*)&float_chunk[0],
                                            float_chunk.size()*sizeof(float));
        if (result == SMILE_SUCCESS) {
            break;
        }
    }
}

void OpensmileSession::on_message(const std::string& topic,
                                  const std::string& message) {
    nlohmann::json m = nlohmann::json::parse(message);

    // Decode base64 chunk
    string coded_src = m["chunk"];
    int encoded_data_length = Base64decode_len(coded_src.c_str());
    vector<char> decoded(encoded_data_length); // Chunk size 16386  byte (16384 for audio + 2 for /0)s
    Base64decode(&decoded[0], coded_src.c_str());
    vector<float> float_chunk((decoded.size()-2) / sizeof(float));
    memcpy(&float_chunk[0], &decoded[0], decoded.size()-2);

    PublishChunk(float_chunk);
}

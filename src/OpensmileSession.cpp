#include <iostream>
#include <mutex>
#include <string.h>
#include <thread>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <boost/log/trivial.hpp>

#include <smileapi/SMILEapi.h>
#include <mqtt/client.h>

#include "GlobalMosquittoListener.h"
#include "JsonBuilder.h"
#include "base64.h"
#include "util.h"


#include "OpensmileSession.h"


OpensmileSession::OpensmileSession(string participant_id,
                                   string mqtt_host_internal,
                                   int mqtt_port_internal) {
    this->participant_id = participant_id;
    this->mqtt_host_internal = mqtt_host_internal;
    this->mqtt_port_internal = mqtt_port_internal;
    this->pid = fork();
    if (!pid) {
        this->Initialize();
        this->Loop();
    }
}

OpensmileSession::~OpensmileSession() {
    // If child process shutdown
    if (this->running) {
        this->Shutdown();
    }

    // If parent process kill child
    kill(this->pid, SIGTERM);
}

void OpensmileSession::Initialize() {
    this->running = true;

    // Initialize JsonBuilder
    BOOST_LOG_TRIVIAL(info)
        << "Initializing JsonBuilder for: " << this->participant_id;
    this->builder = new JsonBuilder();
    this->builder->participant_id = this->participant_id;
    this->builder->Initialize();

    // Initialize Opensmile session
    BOOST_LOG_TRIVIAL(info)
        << "Initializing Opensmile Session for: " << this->participant_id;
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

    // Initialize paho mqtt Mosquitto client
    BOOST_LOG_TRIVIAL(info)
        << "Initializing Mosquitto connection for: " << this->participant_id;
    string client_id = this->participant_id + "_SPEECH_ANALYZER";
    string server_address = "tcp://" + this->mqtt_host_internal + ":" + to_string(this->mqtt_port_internal);
    this->mqtt_client = new mqtt::client(server_address, client_id);
    
    mqtt::connect_options connOpts;
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);
    
    this->mqtt_client->connect(connOpts);
    this->mqtt_client->subscribe(this->participant_id, 2);
    
}

void OpensmileSession::Shutdown() {
    this->running = false;

    // Close listening session
    this->mqtt_client->stop_consuming();
    this->mqtt_client->disconnect();

    // Close Opensmile Session
    smile_extaudiosource_set_external_eoi(this->handle, "externalAudioSource");
    smile_free(this->handle);
    this->opensmile_thread.join();

    // Close process
    exit(0);
}

void OpensmileSession::Loop() {
    BOOST_LOG_TRIVIAL(info)
        << "Begin processing audio chunks for: " << this->participant_id;
    while (true) {
	// Recieve message from broker
	auto msg = this->mqtt_client->consume_message();
	auto payload = msg->get_payload();

	// Payload is always string, so needs to be copied to float vector
	vector<float> float_chunk(payload.size());
	memcpy(&float_chunk[0], payload.data(), payload.size()/sizeof(float));
	this->PublishChunk(float_chunk);
    }
}

void OpensmileSession::PublishChunk(vector<float> float_chunk) {
    while (true) {
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


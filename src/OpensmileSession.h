#pragma once

// STDLIB
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Third Party
#include <smileapi/SMILEapi.h>

// Local
#include "Mosquitto.h"
#include "OpensmileProcessor.h"

class OpensmileSession : Mosquitto {
  public:
    OpensmileSession(std::string participant_id, std::string mqtt_host_internal,
                     int mqtt_port_internal, std::string trial_id, std::string experiment_id);
    ~OpensmileSession();  
  private:
    void PublishChunk(const std::vector<float>& float_chunk);
    void on_message(const std::string& topic,
                    const std::string& message) override;

    int pid;

    // Trial data
    std::string participant_id;
    std::string trial_id;
    std::string experiment_id;

    // MQTT data
    std::string mqtt_host_internal;
    int mqtt_port_internal;
    std::thread listener_thread;


    // Opensmile data
    OpensmileProcessor* processor;
    smileobj_t* handle;
    std::thread opensmile_thread;
};

#pragma once

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "JsonBuilder.h"
#include <smileapi/SMILEapi.h>


class OpensmileSession : Mosquitto {
  public:
    OpensmileSession(std::string participant_id, std::string mqtt_host_internal, int mqtt_port_internal);
    ~OpensmileSession();

    

  private:
    void Initialize();
    void Shutdown();
    void Loop();
    void on_message(const std::string& topic,const std::string& message) override;
    
    std::string mqtt_host_internal;
    int mqtt_port_internal;
    std::thread listener_thread;

    std::string participant_id;
    std::mutex mutex;
    std::queue<std::vector<float>> queue;

    JsonBuilder *builder;
    smileobj_t* handle;
    std::thread opensmile_thread;
};

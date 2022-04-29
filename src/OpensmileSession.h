#pragma once

#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mqtt/client.h>
#include <smileapi/SMILEapi.h>

#include "JsonBuilder.h"

class OpensmileSession{
  public:
    OpensmileSession(std::string participant_id, std::string mqtt_host_internal,
                     int mqtt_port_internal);
    ~OpensmileSession();

  private:
    void Initialize();
    void Shutdown();
    void Loop();
    void PublishChunk(std::vector<float> float_chunk);

    bool running = false;
    int pid;

    std::string mqtt_host_internal;
    int mqtt_port_internal;
    mqtt::client *mqtt_client;
    std::thread listener_thread;

    std::string participant_id;
    std::mutex mutex;

    JsonBuilder* builder;
    smileobj_t* handle;
    std::thread opensmile_thread;
};

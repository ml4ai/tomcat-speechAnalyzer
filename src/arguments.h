#pragma once
#include <string>

struct Arguments {
    std::string mode = "websocket";

    // Websocket options
    std::string ws_host = "0.0.0.0";
    int ws_port = 8888;

    // Mosquitto options
    std::string mqtt_host = "mosquitto";
    int mqtt_port = 1883;

    // Disable systems
    bool disable_asr = false;
    bool disable_opensmile = true;
    bool disable_audio_writing = false;
};

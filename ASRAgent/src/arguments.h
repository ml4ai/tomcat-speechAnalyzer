#pragma once
#include <string>

struct Arguments {
    std::string mode;

    // Websocket options
    std::string ws_host;
    int ws_port;

    // Mosquitto options
    std::string mqtt_host;
    int mqtt_port;
    std::string mqtt_host_internal;
    int mqtt_port_internal;

    // Audio options
    int sample_rate;

    // Disable systems
    bool disable_asr_google;
    bool disable_asr_vosk;
    bool disable_opensmile;
    bool disable_audio_writing;

    // Publishing options
    bool intermediate_first_only;
};

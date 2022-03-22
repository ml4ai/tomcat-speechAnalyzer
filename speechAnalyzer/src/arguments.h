#pragma once
#include <string>

struct Arguments {
    // Mosquitto options
    std::string mqtt_host;
    int mqtt_port;
    std::string mqtt_host_internal;
    int mqtt_port_internal;

};

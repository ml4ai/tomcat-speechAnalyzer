#pragma once

// STDLIB
#include <string>
#include <thread>
#include <vector>

// Third party
#include <nlohmann/json.hpp>

// Local
#include "Mosquitto.h"
#include "DBWrapper.h"

class ASRProcessor {
  public:
    std::string participant_id;

    ASRProcessor(std::string mqtt_host, std::string mqtt_port);

    void ProcessASRMessage(nlohmann::json m);



  private:
    std::string ProcessMMCMessage(std::string message);
    
    // Mosquitto client objects
    std::string mqtt_host;
    std::string mqtt_port;
    Mosquitto mosquitto_client;

    // DBWrapper object
    unique_ptr<DBWrapper> postgres;

    // Functions for creating cmomon message types
    nlohmann::json create_common_header(std::string message_type);
    nlohmann::json create_common_msg(std::string sub_type);
};

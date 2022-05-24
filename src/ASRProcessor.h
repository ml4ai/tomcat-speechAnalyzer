#pragma once

// STDLIB
#include <string>
#include <thread>
#include <vector>
#include <memory>

// Third party
#include <nlohmann/json.hpp>

// Local
#include "Mosquitto.h"
#include "DBWrapper.h"

class ASRProcessor {
  public:
    std::string participant_id;

    ASRProcessor(std::string mqtt_host, int mqtt_port);

    void ProcessASRMessage(nlohmann::json m);



  private:
    std::string ProcessMMCMessage(std::string message);

    // Mosquitto client objects
    std::string mqtt_host;
    int mqtt_port;
    Mosquitto mosquitto_client;

    // DBWrapper object
    std::unique_ptr<DBWrapper> postgres;

    // Functions for creating cmomon message types
    nlohmann::json create_common_header(std::string message_type, std::string trial_id, std::string experiment_id);
    nlohmann::json create_common_msg(std::string sub_type, std::string trial_id, std::string experiment_id);
};

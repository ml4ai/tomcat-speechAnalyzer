#pragma once

// STDLIB
#include <string>
#include <thread>
#include <vector>
#include <memory>

// Third Party
#include <nlohmann/json.hpp>

// Local
#include "DBWrapper.h"
#include "ASRProcessor.h"
#include "Mosquitto.h"
#include "OpensmileSession.h"

class Manager : public Mosquitto {

  public:
    Manager(std::string mqtt_host, int mqtt_port,
                 std::string mqtt_host_internal, int mqtt_port_internal);

  protected:
    void on_message(const std::string& topic,
                    const std::string& message) override;

  private:
    void InitializeParticipants(std::vector<std::string> participants, std::string trial_id, std::string experiment_id);
    void ClearParticipants();
    void ProcessASRMessage(nlohmann::json m);

    // MQTT options
    std::string mqtt_host;
    int mqtt_port;
    std::thread listener_thread;
    std::string mqtt_host_internal;
    int mqtt_port_internal;

    std::unique_ptr<ASRProcessor> processor;
    std::unique_ptr<DBWrapper> postgres;
    
    std::vector<OpensmileSession*> participant_sessions;
   
};

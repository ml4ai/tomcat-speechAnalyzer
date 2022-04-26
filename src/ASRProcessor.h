#pragma once

#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "JsonBuilder.h"
#include "Mosquitto.h"
#include "OpensmileSession.h"
#include "arguments.h"

class ASRProcessor : public Mosquitto {

  public:
    std::string trial_id = "00000000-0000-0000-0000-000000000000";
    std::string experiment_id = "00000000-0000-0000-0000-000000000000";

    ASRProcessor(std::string mqtt_host, int mqtt_port,
                 std::string mqtt_host_internal, int mqtt_port_internal);
    ~ASRProcessor();

    bool running = true;

  protected:
    void on_message(const std::string& topic,
                    const std::string& message) override;

  private:
    void Initialize();
    void InitializeParticipants(std::vector<std::string> participants);
    void ClearParticipants();
    void Shutdown();
    void ProcessASRMessage(nlohmann::json m);

    // MQTT options
    std::string mqtt_host;
    int mqtt_port;
    std::string mqtt_host_internal;
    int mqtt_port_internal;

    JsonBuilder* builder;
    std::vector<OpensmileSession*> participant_sessions;
    std::thread listener_thread;
};

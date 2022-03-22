#pragma once 

#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "arguments.h"
#include "Mosquitto.h"

class ASRProcessor : public Mosquitto {

  public:
    std::string trial_id = "00000000-0000-0000-0000-000000000000";
    std::string experiment_id = "00000000-0000-0000-0000-000000000000";

    ASRProcessor(Arguments args);
    ~ASRProcessor();


   
  protected:
    void on_message(const std::string& topic,
                    const std::string& message) override;

  private:
    void Initialize();
    void InitializeParticipants(std::vector<std::string> participants);
    void Shutdown();
    void ProcessASRMessage(nlohmann::json m);
    Arguments args;
    //std::vector<OpensmileListener*> participant_sessions;
    std::thread listener_thread;
};

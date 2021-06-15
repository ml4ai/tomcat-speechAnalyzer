#pragma once

#include <string>

#include "Mosquitto.h"

class TrialListener : public Mosquitto {
  public:
    std::string trial_id = "00000000-0000-0000-0000-000000000000";
    std::string experiment_id = "00000000-0000-0000-0000-000000000000";

    std::string playername = "";

    bool in_trial = false;

  protected:
    void on_message(const std::string& topic,
                    const std::string& message) override;
};

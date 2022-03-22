#include "TrialListener.h"

#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

using namespace std;

void TrialListener::on_message(const string& topic, const string& message) {

    nlohmann::json m = nlohmann::json::parse(message);
    if (m["msg"].contains("trial_id")) {
        this->trial_id = m["msg"]["trial_id"];
    }
    if (m["msg"].contains("experiment_id")) {
        this->experiment_id = m["msg"]["experiment_id"];
    }

    // Check if trial has started
    std::string sub_type = m["msg"]["sub_type"];
    if (sub_type.compare("start") == 0) {
        this->in_trial = true;
    }

    // Check if trial has stopped
    if (sub_type.compare("stop") == 0) {
        this->in_trial = false;
    }
}

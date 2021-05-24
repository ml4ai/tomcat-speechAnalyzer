#include "TrialListener.h"

#include <iostream>
#include <thread>
#include <nlohmann/json.hpp>

using namespace std;

void TrialListener::on_message(const string& topic, const string& message) {
    nlohmann::json m = nlohmann::json::parse(message);
    if (m["msg"].contains("trial_id")) {
        this->trial_id = m["msg"]["trial_id"];
    }
    if (m["msg"].contains("experiment_id")) {
        this->experiment_id = m["msg"]["experiment_id"];
    }
    if (m["data"].contains("client_info")) {
/*	for(nlohmann::json client : m["data"]["client_info"]){
		if(client["participantid"].compare(this->playername) == 0){ 
			this->participant_id = client["participantid"];
		}
	}*/
    }
}

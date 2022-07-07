// STDLIB
#include <string>
#include <thread>
#include <vector>
#include <memory>

// Third Party
#include <boost/log/trivial.hpp>
#include <nlohmann/json.hpp>

// Local
#include "ASRProcessor.h"
#include "OpensmileSession.h"
#include "Manager.h"

using namespace std;

Manager::Manager(string mqtt_host, int mqtt_port,
                           string mqtt_host_internal, int mqtt_port_internal) {
    this->mqtt_host = mqtt_host;
    this->mqtt_port = mqtt_port;
    this->mqtt_host_internal = mqtt_host_internal;
    this->mqtt_port_internal = mqtt_port_internal;

    // Initialize JsonBuilder
    processor = make_unique<ASRProcessor>(mqtt_host, mqtt_port);

    // Initialize Database connection to clear trial
    postgres = make_unique<DBWrapper>(1); // Only one connection needed at a time for trial clearing

    // Make connection to external mqtt server
    connect(mqtt_host, mqtt_port, 1000, 1000, 1000);
    subscribe("trial");
    subscribe("agent/asr/final");
    set_max_seconds_without_messages(100000000);
    listener_thread = thread([this] { this->loop(); });
}

void Manager::InitializeParticipants(vector<string> participants, string trial_id, string experiment_id) {
    for (int i = 0; i < participants.size(); i++) {
	// Create OpensmileListener
        participant_sessions.push_back(
            new OpensmileSession(participants[i],
                                 mqtt_host_internal,
                                 mqtt_port_internal,
				 trial_id,
				 experiment_id));
    }
}

void Manager::ClearParticipants() {

    // Free pointers
    for (auto p : this->participant_sessions) {
    	delete p;
    }

    // Clear vector
    participant_sessions.clear();
}

void Manager::on_message(const std::string& topic,
                              const std::string& message) {
    nlohmann::json m = nlohmann::json::parse(message);
    if (topic.compare("trial") == 0) {
        string sub_type = m["msg"]["sub_type"];
	string trial_id = m["msg"]["trial_id"];
	string experiment_id = m["msg"]["experiment_id"];
        if (sub_type.compare("start") == 0) {
            BOOST_LOG_TRIVIAL(info) << "Recieved trial start message, creating "
                                       "Opensmile sessions...";

            // Set client info
            vector<string> participants;
            nlohmann::json client_info = m["data"]["client_info"];
            for (nlohmann::json client : client_info) {
                participants.push_back(client["playername"]); //Switch back to playername
            }

	    // Clear trial in database
	    postgres->ClearTrial(trial_id);

            // Initialize participant session
            InitializeParticipants(participants, trial_id, experiment_id);
        }
        else if (sub_type.compare("stop") == 0) {
            BOOST_LOG_TRIVIAL(info) << "Recieved trial stop message, shutting "
                                       "down Opensmile sessions...";

            // Clear participant sessions
            this->ClearParticipants();

            BOOST_LOG_TRIVIAL(info) << "Ready for next trial";
        }
    }
    else if (topic.compare("agent/asr/final") == 0) {
        BOOST_LOG_TRIVIAL(info) << "Recieved ASR message, processing sentiment/personality data...";
        // Process sentiment message in seperate thread
        std::thread io = std::thread(
            [this, m] { this->processor->ProcessASRMessage(m); });
        io.detach();
    }
}

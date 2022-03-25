#include <string>
#include <vector>
#include <thread>

#include <nlohmann/json.hpp>

#include "OpensmileListener.h"
#include "arguments.h"
#include "ASRProcessor.h"

using namespace std;

ASRProcessor::ASRProcessor(string mqtt_host, int mqtt_port, string mqtt_host_internal, int mqtt_port_internal){
	this->mqtt_host = mqtt_host;
	this->mqtt_port = mqtt_port;
	this->mqtt_host_internal = mqtt_host_internal;
	this->mqtt_port_internal = mqtt_port_internal;

	this->Initialize();
}

ASRProcessor::~ASRProcessor(){
	this->Shutdown();
}

void ASRProcessor::Initialize(){
	// Initialize JsonBuilder
	this->builder = new JsonBuilder();
	this->builder->Initialize();

	// Make connection to external mqtt server
	this->connect(this->mqtt_host, this->mqtt_port, 1000, 1000, 1000);
	this->subscribe("trial");
	this->subscribe("agent/asr/final");
	this->set_max_seconds_without_messages(10000);
	this->listener_thread = thread([this] { this->loop(); });
}

void ASRProcessor::Shutdown(){

}

void ASRProcessor::InitializeParticipants(vector<string> participants){
	for(string participant : participants){
		// Create OpensmileListener 
		this->participant_sessions.push_back(new OpensmileListener(this->mqtt_host_internal, this->mqtt_port_internal, participant, this->socket_port));

		// Update socket port
		this->socket_port++;
	}
}


void ASRProcessor::on_message(const std::string& topic,const std::string& message){
	nlohmann::json m = nlohmann::json::parse(message);
	if(topic.compare("trial") == 0){
	       string sub_type = m["msg"]["sub_type"];
	       if( sub_type.compare("start") == 0){
		       std::cout << "RECIEVED TRIAL START MESSAGE" << std::endl;
			// Set trial info
			this->trial_id = m["msg"]["trial_id"];
			this->experiment_id = m["msg"]["experiment_id"];

			// Set client info
			vector<string> participants;
			nlohmann::json client_info = m["data"]["client_info"];
			for(nlohmann::json client : client_info){
				participants.push_back(client["participant_id"]);
			}

			// Initialize Participants
			this->InitializeParticipants(participants);
		}
	       else if( sub_type.compare("stop") == 0){
		       std::cout << "RECIEVED TRIAL STOP MESSAGE" << std::endl;
	       }
	}
}

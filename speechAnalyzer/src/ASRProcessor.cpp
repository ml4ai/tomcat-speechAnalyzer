#include <string>
#include <vector>
#include <thread>

#include <nlohmann/json.hpp>

#include "arguments.h"
#include "ASRProcessor.h"

using namespace std;

ASRProcessor::ASRProcessor(Arguments args){
	this->args = args;

	this->Initialize();
}

ASRProcessor::~ASRProcessor(){
	this->Shutdown();
}

void ASRProcessor::Initialize(){
	this->connect(this->args.mqtt_host, this->args.mqtt_port, 1000, 1000, 1000);
	this->subscribe("trial");
	this->subscribe("agent/asr/final");
	this->set_max_seconds_without_messages(10000);
	this->listener_thread = thread([this] { this->loop(); });
}

void ASRProcessor::Shutdown(){

}

void ASRProcessor::InitializeParticipants(vector<string> participants){
	for(string participant : participants){
		//this->participant_sessions.push_back(new OpensmileListener(this->args, participant));
	}
}

void ASRProcessor::ProcessASRMessage(nlohmann::json message){

}

void ASRProcessor::on_message(const std::string& topic,const std::string& message){
	nlohmann::json m = nlohmann::json::parse(message);
	if(topic.compare("trial") == 0){
	       string sub_type = m["msg"]["sub_type"];
	       if( sub_type.compare("start") == 0){
			// Set trial info
			this->trial_id = m["msg"]["trial_id"];
			this->experiment_id = m["msg"]["experiment_id"];

			// Set client info
			vector<string> participants;
			nlohmann::json client_info = m["data"]["client_info"];
			for(nlohmann::json client : client_info){
				participants.push_back(client["playername"]);
			}

			// Initialize Participants
			this->InitializeParticipants(participants);
		}
	       else if( sub_type.compare("stop") == 0){

	       }
	}
	else if(topic.compare("agent/asr/final")){
		this->ProcessASRMessage(m);
	}
}

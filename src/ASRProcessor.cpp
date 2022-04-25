#include <string>
#include <vector>
#include <thread>

#include <boost/log/trivial.hpp>
#include <nlohmann/json.hpp>

#include "OpensmileSession.h"
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
	this->ClearParticipants();
}

void ASRProcessor::InitializeParticipants(vector<string> participants){
	for(string participant : participants){
		// Create OpensmileListener 
		this->participant_sessions.push_back(new OpensmileSession(participant, this->mqtt_host_internal, this->mqtt_port_internal));

	}
}

void ASRProcessor::ClearParticipants(){
	// Free pointers
	for(auto p : this->participant_sessions){
		delete p;
	}

	// Clear vector
	this->participant_sessions.clear();
}

void ASRProcessor::on_message(const std::string& topic,const std::string& message){
	nlohmann::json m = nlohmann::json::parse(message);
	if(topic.compare("trial") == 0){
	       string sub_type = m["msg"]["sub_type"];
	       if( sub_type.compare("start") == 0){
		       BOOST_LOG_TRIVIAL(info) << "Recieved trial start message, creating Opensmile sessions...";
			// Set trial info
			this->trial_id = m["msg"]["trial_id"];
			this->experiment_id = m["msg"]["experiment_id"];

			// Set client info
			vector<string> participants;
			nlohmann::json client_info = m["data"]["client_info"];
			for(nlohmann::json client : client_info){
				participants.push_back(client["playername"]);
			}

			// Initialize participant session
			this->InitializeParticipants(participants);
		}
	       else if( sub_type.compare("stop") == 0){
		       BOOST_LOG_TRIVIAL(info) << "Recieved trial stop message, shutting down Opensmile sessions...";
	       	       
		       // Clear participant sessions
		       this->ClearParticipants();

		       BOOST_LOG_TRIVIAL(info) << "Ready for next trial";
	       }
	}
	else if(topic.compare("agent/asr/final") == 0){
		// Process sentiment message in seperate thread
		std::thread io = std::thread([this, m] {this->builder->process_sentiment_message(m);});
		io.detach();
	}
}

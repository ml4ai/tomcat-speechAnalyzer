#include <thread>
#include <string>
#include <vector>

#include "arguments.h"
#include "base64.h"
#include "OpensmileListener.h"

using namespace std;

OpensmileListener::OpensmileListener(string mqtt_host_internal, int mqtt_port_internal, string participant_id, int socket_port){
	this->mqtt_host_internal = mqtt_host_internal;
	this->mqtt_port_internal = mqtt_port_internal;

	this->participant_id = participant_id;
	this->socket_port = socket_port;

	this->Initialize();
}

OpensmileListener::~OpensmileListener(){
	this->Shutdown();
}

void OpensmileListener::Initialize(){
	std::cout << "INITIALIZING JSON BUILDER FOR: " << this->participant_id << std::endl;
	// Initialize JsonBuilder
	this->builder = new JsonBuilder();
	this->builder->participant_id = this->participant_id;
	this->builder->Initialize();

	std::cout << "INITIALIZING OPENSMILE SESSION FOR: " << this->participant_id << std::endl;
	// Initialize Opensmile Session
	this->session = new OpensmileSession(this->socket_port, this->builder);

	std::cout << "INITIALIZING MOSQUITTO CONNECTION FOR: " << this->participant_id << std::endl;
	// Connect to broker
	this->connect(this->mqtt_host_internal, this->mqtt_port_internal, 1000, 1000, 1000);
	this->subscribe(this->participant_id);
	this->subscribe("agent/asr/final");
	this->set_max_seconds_without_messages(10000);
        this->listener_thread = thread([this] { this->loop(); });
}

void OpensmileListener::Shutdown(){

}

void OpensmileListener::on_message(const std::string& topic,const std::string& message){
	nlohmann::json m = nlohmann::json::parse(message);
   	
       	if(topic.compare("agent/asr/final") == 0){
		this->builder->process_sentiment_message(m);
	}
	else{	
		// Decode base64 chunk
		string coded_src = m["chunk"];
		int encoded_data_length = Base64decode_len(coded_src.c_str());
		vector<char> decoded(encoded_data_length);
		Base64decode(&decoded[0], coded_src.c_str());
		vector<float> float_chunk(decoded.size()/sizeof(float)); 
		memcpy(&float_chunk[0], &decoded[0], decoded.size());
			
		// Send chunk
		this->session->send_chunk(float_chunk);	
	}
}

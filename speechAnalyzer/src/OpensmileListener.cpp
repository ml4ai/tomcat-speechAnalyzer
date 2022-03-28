#include <thread>
#include <string>
#include <vector>

#include <boost/log/trivial.hpp>

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
	// Initialize JsonBuilder
	BOOST_LOG_TRIVIAL(info) << "Initializing JsonBuilder for: " << this->participant_id;
	this->builder = new JsonBuilder();
	this->builder->participant_id = this->participant_id;
	this->builder->Initialize();

	// Initialize Opensmile Session
	BOOST_LOG_TRIVIAL(info) << "Initializing Opensmile Session for: " << this->participant_id;
	this->session = new OpensmileSession(this->socket_port, this->builder);

	// Connect to broker
	BOOST_LOG_TRIVIAL(info) << "Initializing Mosquitto connection for: " << this->participant_id;
	this->connect(this->mqtt_host_internal, this->mqtt_port_internal, 1000, 1000, 1000);
	this->subscribe(this->participant_id);
	this->subscribe("agent/asr/final");
	this->set_max_seconds_without_messages(10000);
        this->listener_thread = thread([this] { this->loop(); });
}

void OpensmileListener::Shutdown(){
	// Shutdown JsonBuilder
	BOOST_LOG_TRIVIAL(info) << "Shutting down JsonBuilder for: " << this->participant_id;
	this->builder->Shutdown();

	// Shutdown Opensmile Session
	BOOST_LOG_TRIVIAL(info) << "Shutting down Opensmile Session for: " << this->participant_id;
	this->session->send_eoi();

	// Shutdown listening thread
	BOOST_LOG_TRIVIAL(info) << "Shutting down Mosquitto connection for: " << this->participant_id;
	this->close();
    	this->listener_thread.join();	
}

void OpensmileListener::on_message(const std::string& topic,const std::string& message){
	nlohmann::json m = nlohmann::json::parse(message);
   	
       	if(topic.compare("agent/asr/final") == 0){
		// Check if associated with this participant
		string participant = m["data"]["participant_id"].get<string>();
		if(this->participant_id.compare(participant) == 0){
			this->builder->process_sentiment_message(m);
		}
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

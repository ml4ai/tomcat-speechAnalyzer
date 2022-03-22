#include "arguments.h"

#include "OpensmileListener.h"

using namespace std;

OpensmileListener::OpensmileListener(Arguments args, string participant_id){
	this->participant_id = participant_id;

	this->port = this->socket_port;
	this->socket_port++;

	this->Initialize();
}

OpensmileListener::~OpensmileListener(){
	this->Shutdown();
}

void OpensmileListener::Initialize(){
	// Initialize JsonBuilder
	this->builder = new JsonBuilder();
	this->builder->participant_id = this->participant_id;
	this->builder->Initialize();

	// Initialize Opensmile Session
	this->session = new OpensmileSession(this->port, this->builder);

	// Connect to broker
	this->connect(this->args.mqtt_host_internal, this->args.mqtt_port_internal, 1000, 1000, 1000);
	this->subscribe(this->participant_id);
	this->set_max_seconds_without_messages(10000);
        this->listener_thread = thread([this] { this->loop(); });
}

void OpensmileListener::Shutdown(){

}

void OpensmileListener::on_message(const std::string& topic,const std::string& message){
    	nlohmann::json m = nlohmann::json::parse(message);
    
	// Decode base64 chunk
	string coded_src = m["chunk"];
	int encoded_data_length = Base64decode_len(coded_src.c_str());
	vector<char> decoded(encoded_data_length);
	Base64decode(&decoded[0], coded_src.c_str());
	vector<float> float_chunk(decoded.size()/sizeof(float)); 
	memcpy(&float_chunk[0], &decoded[0], decoded.size())
		
	// Send chunk
	this->session->send_chunk(float_chunk);	
}

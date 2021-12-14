#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <iterator>

#include <boost/algorithm/string.hpp>
#include <libpq-fe.h>
#include <nlohmann/json.hpp>

#include "GlobalMosquittoListener.h"
#include "DBWrapper.h"

using namespace std;

const vector<char> DBWrapper::INVALID_COLUMN_CHARACTERS = {'+','-','(',')','\n', '.'}; 

DBWrapper::DBWrapper(){
}

DBWrapper::~DBWrapper(){
}

void DBWrapper::initialize(){
	// Create connection string
	string connection_string = "host=" + this->host + " port=" + this->port + " dbname=" + this->db + " user=" + this->user + " password= " + this->pass;
	// Create connection
	this->conn = PQconnectdb(connection_string.c_str());
	if (PQstatus(conn) != CONNECTION_OK)
        {
		std::cout<< PQerrorMessage(conn) << std::endl;
        }
}
void DBWrapper::shutdown(){
	// End connection
	PQfinish(this->conn);
}
void DBWrapper::publish_chunk(nlohmann::json message){
	// Generate columns and values
	vector<string> columns;
	vector<double> values;
	for(auto element : message["data"]["features"]["lld"].items()){
		columns.push_back(this->format_to_db_string(element.key()));
		values.push_back(element.value());		
	}

	// Get timestamp
	this->timestamp = message["data"]["tmeta"]["time"];

	// Convert columns to string format
	ostringstream oss;
	for(string element : columns){
		oss << element << ",";
	}
	oss << "participant, " << "timestamp";
	string column_string = oss.str();
	oss.str("");

	// Convert values to string format
	for(double element : values){
		oss << to_string(element) << ",";
	}
	oss << message["data"]["participant_id"]  << "," << message["data"]["tmeta"]["time"];
	string value_string = oss.str();

	// Generate sql query
	string query = "INSERT INTO features (" +
					     column_string +
					     ") VALUES (" +
					     value_string +
					     ")";

	boost::replace_all(query, "\"", "\'");
	// Send query
	PGresult *result = PQexec(this->conn, query.c_str());
	
}

string DBWrapper::format_to_db_string(std::string in){
	boost::replace_all(in, ")", "");
	for(char c : this->INVALID_COLUMN_CHARACTERS){
		boost::replace_all(in, string(1,c), "_");
	}
	return in;
}

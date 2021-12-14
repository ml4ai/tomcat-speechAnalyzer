#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <iterator>
#include <map>
#include <stdlib.h>


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

vector<nlohmann::json> DBWrapper::features_between(double start_time, double end_time){
	// Get features from database
	std::string query = "SELECT * FROM features WHERE timestamp >= " + to_string(start_time) + " and timestamp <= " + to_string(end_time);
	PGresult *result = PQexec(this->conn, query.c_str());

	// Turn features into json object
	vector<nlohmann::json> out; 
	for( int i=0; i<PQntuples(result); i++){
		nlohmann::json message;
		for(int j=0; j<PQnfields(result); j++){
			string field = this->column_map[PQfname(result, j)];
			if(field.compare("timestamp") == 0 || field.compare("participant") == 0){
				continue;
			}
			double value = atof(PQgetvalue(result, i, j));
			message[field] = value;		
		}
		out.push_back(message);
	}

	// Clear result
	PQclear(result);

	return out;	
}

string DBWrapper::format_to_db_string(std::string in){
	// Check if value already in map
	if(this->column_map.find(in) != this->column_map.end()){
		return this->column_map[in];	
	}
	
	string original = string(in);
	boost::replace_all(in, ")", "");
	for(char c : this->INVALID_COLUMN_CHARACTERS){
		boost::replace_all(in, string(1,c), "_");
	}
	this->column_map[in] = original;
	this->column_map[original] = in;

	return in;
}

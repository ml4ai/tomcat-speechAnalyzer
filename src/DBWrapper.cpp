#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <iterator>
#include <map>
#include <stdlib.h>


#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

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
	// Create UUID for client
	this->client_id = boost::uuids::to_string(boost::uuids::random_generator()());

	// Create connection string
	this->connection_string = "host=" + this->host + " port=" + this->port + " dbname=" + this->db + " user=" + this->user + " password= " + this->pass;
}
void DBWrapper::shutdown(){
}
void DBWrapper::publish_chunk(nlohmann::json message){	
	PGconn *conn;
	PGresult *result;
	
	// Create connection
	conn = PQconnectdb(this->connection_string.c_str());
	if (PQstatus(conn) != CONNECTION_OK)
        {
		std::cout << "Connection error: " << std::endl;
		std::cout<< PQerrorMessage(conn) << std::endl;
        }
	
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
	oss << "participant, " << "timestamp, " << "client_id";
	string column_string = oss.str();
	oss.str("");

	// Convert values to string format
	for(double element : values){
		oss << to_string(element) << ",";
	}
	oss << message["data"]["participant_id"]  << "," << message["data"]["tmeta"]["time"] << "," << "\'" << this->client_id << "\'";
	string value_string = oss.str();

	// Generate sql query
	string query = "INSERT INTO features (" +
					     column_string +
					     ") VALUES (" +
					     value_string +
					     ")";

	boost::replace_all(query, "\"", "\'");
	// Send query
	result = PQexec(conn, query.c_str());
	if(result == NULL){
		std::cout << "Execution error: " << std::endl;
		std::cout << PQerrorMessage(conn) << std::endl;
	}
	// Clear result
        PQclear(result);
	
	// End connection
	PQfinish(conn);
}

vector<nlohmann::json> DBWrapper::features_between(double start_time, double end_time){
	PGconn *conn;
	PGresult *result;
	 
	 // Create connection
        conn = PQconnectdb(this->connection_string.c_str());
        if (PQstatus(conn) != CONNECTION_OK)
        {
                  std::cout << "Connection error: " << std::endl;
                  std::cout<< PQerrorMessage(conn) << std::endl;
        }

	// Get features from database
	std::string query = "SELECT * FROM features WHERE timestamp >= " + to_string(start_time) + " and timestamp <= " + to_string(end_time) + " and client_id=" + "\'" + this->client_id + "\'";
	result = PQexec(conn, query.c_str());
	if(result == NULL){
	     std::cout << "FAILURE" << std::endl;
             std::cout << PQerrorMessage( conn) << std::endl;
	}
	// Turn features into json object
	vector<nlohmann::json> out; 
	for( int i=0; i<PQntuples(result); i++){
		nlohmann::json message;
		for(int j=0; j<PQnfields(result); j++){
			if(this->column_map.find(PQfname(result, j)) == this->column_map.end()){
				continue;
			}
			string field = this->column_map[PQfname(result, j)];
			double value = atof(PQgetvalue(result, i, j));
			message[field] = value;		
		}
		out.push_back(message);
	}

	// Clear result
        PQclear(result);
	
	// End connection
	PQfinish(conn);
	
	return out;	
}

string DBWrapper::format_to_db_string(std::string in){
	// Check if value already in map
	if(this->column_map.find(in) != this->column_map.end()){
		return this->column_map[in];	
	}
	
	string original = string(in);
	boost::to_lower(in);
	boost::replace_all(in, ")", "");
	for(char c : this->INVALID_COLUMN_CHARACTERS){
		boost::replace_all(in, string(1,c), "_");
	}
	this->column_map[in] = original;
	this->column_map[original] = in;

	return in;
}

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <iterator>

#include <boost/algorithm/string.hpp>
#include <libpq-fe.h>
#include <nlohmann/json.hpp>

#include "DBWrapper.h"

using namespace std;

const vector<char> DBWrapper::INVALID_COLUMN_CHARACTERS = {'+','-','(',')','\n'}; 

DBWrapper::DBWrapper(){
}

DBWrapper::~DBWrapper(){
}

void DBWrapper::initialize(){
	// Create connection string
	string connection_string = "user=" + this->user + " password= " + this->pass + " dbname=" + this->db;
	// Create connection
	this->conn = PQconnectdb(connection_string.c_str());	
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
	
	// Convert columns and values to string format
	ostringstream oss;
	copy(begin(columns), end(columns), ostream_iterator<string>(oss, ","));
	string column_string = oss.str();
	oss.clear();

	copy(begin(values), end(values), ostream_iterator<double>(oss, ","));
	string value_string = oss.str();

	// Generate sql query
	string query = "INSERT INTO features (" +
					     column_string +
					     ") VALUES (" +
					     value_string +
					     ")";

	std::cout << query << std::endl;
	// Send query
	PGresult *result = PQexec(this->conn, query.c_str());
	std::cout << query << std::endl;
	
}

string DBWrapper::format_to_db_string(std::string in){
	for(char c : this->INVALID_COLUMN_CHARACTERS){
		boost::replace_all(in, string(1,c), "_");
	}
	return in;
}

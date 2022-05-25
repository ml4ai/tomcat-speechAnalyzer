// STDLIB
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <queue>
#include <sstream>
#include <stack>
#include <stdlib.h>
#include <string>
#include <vector>

// Third Party 
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <libpq-fe.h>
#include <nlohmann/json.hpp>

// Local
#include "DBWrapper.h"

using namespace std;

DBWrapper::DBWrapper(int connection_pool_size) { 
        this->connection_pool_size = connection_pool_size;

        InitializeColumnMap();
	InitializeConnections();
}

void DBWrapper::InitializeColumnMap() {
    ifstream file("conf/column_map.txt");
    string opensmile_format;
    string postgres_format;

    while (!file.eof()) {
        file >> opensmile_format;
        file >> postgres_format;
        column_map[opensmile_format] = postgres_format;
        column_map[postgres_format] = opensmile_format;
    }
}

void DBWrapper::InitializeConnections(){
    string connection_string = "host=" + host + " port=" + port +
                              " dbname=" + db + " user=" + user +
                              " password= " + pass;
    string CHUNK_EXTRACT_STR = "SELECT * FROM features WHERE seconds_offset >= $1 and seconds_offset <= $2 and participant = $3 and trial_id = $4;";
    string CLEAR_TRIAL_STR = "DELETE FROM features WHERE trial_id = $1";
    string INDECIES = "CREATE INDEX extract ON features (trial_id, experiment_id, participant, seconds_offset);";

    // Initialize connection pool
    for (int i = 0; i < connection_pool_size; i++) {
        // Create connection object
	PGconn* conn;
	conn = PQconnectdb(connection_string.c_str());

	// Prepare statements	
	PQprepare(conn, "CHUNK_EXTRACT", CHUNK_EXTRACT_STR.c_str(), 4, NULL);  	
	PQprepare(conn, "CLEAR_TRIAL", CLEAR_TRIAL_STR.c_str(), 1, NULL);  	

	// Create indecies
	PQexec(conn, INDECIES.c_str());

	// Push connection to pool
	this->connection_pool.push(conn);
    }

} 

PGconn* DBWrapper::GetConnection() {
    PGconn* conn;
    std::unique_lock<std::mutex> guard(connection_mutex);
    if (ConnectionAvaliable()) {
        conn = connection_pool.top();
        connection_pool.pop();
    }
    else {
        connection_condition.wait(
            guard, [this] { return this->ConnectionAvaliable(); });
        conn = connection_pool.top();
        connection_pool.pop();
    }
    return conn;
}

void DBWrapper::FreeConnection(PGconn* conn) {
    std::lock_guard<std::mutex> guard(connection_mutex);
    connection_pool.push(conn);
    connection_condition.notify_one();
}

bool DBWrapper::ConnectionAvaliable() {
    if (connection_pool.empty()) {
        return false;
    }
    return true;
}

void DBWrapper::PublishChunk(nlohmann::json message) { 
    PGconn* conn;
    PGresult* result;

    // Get connection object from pool
    conn = GetConnection();

    // Generate columns and values
    vector<string> columns;
    vector<double> values;
    for (auto element : message["data"]["features"]["lld"].items()) {
        columns.push_back(format_to_db_string(element.key()));
        values.push_back(element.value());
    }

    string participant_id = to_string(message["data"]["participant_id"]);
    boost::replace_all(participant_id, "\"", "\'");
    string trial_id = to_string(message["msg"]["trial_id"]);
    boost::replace_all(trial_id, "\"", "\'");
    string experiment_id = to_string(message["msg"]["experiment_id"]);
    boost::replace_all(experiment_id, "\"", "\'");

    // Convert columns to string format
    ostringstream oss;
    for (string element : columns) {
        oss << element << ",";
    }
    oss << "seconds_offset, "
        << "timestamp, "
        << "participant, "
        << "trial_id, "
        << "experiment_id";
    string column_string = oss.str();
    oss.str("");

    // Convert values to string format
    for (double element : values) {
        oss << to_string(element) << ",";
    }
    std::string utc_timestamp =
        boost::posix_time::to_iso_extended_string(
            boost::posix_time::microsec_clock::universal_time()) +
        "Z";

    oss << message["data"]["tmeta"]["time"] << ","
        << "\'" << utc_timestamp << "\'"
        << "," << message["data"]["participant_id"] << ","
        << message["msg"]["trial_id"] << "," << message["msg"]["experiment_id"];
    string value_string = oss.str();

    // Generate sql query
    string query = "INSERT INTO features (" + column_string + ") VALUES (" +
                   value_string + ")";

    boost::replace_all(query, "\"", "\'");
    // Send query
    result = PQexec(conn, query.c_str());
    if (result == NULL) {
        BOOST_LOG_TRIVIAL(error) << "Execution error: " << PQerrorMessage(conn);
    }
    
    // Clean session
    PQclear(result);
    this->FreeConnection(conn);
}

void DBWrapper::ClearTrial(string trial_id){
	PGconn *conn;
	PGresult* result;

	conn = GetConnection();

	// Set data for prepared statement
	const char* values[1];
	int lengths[1];
	values[0] = trial_id.c_str();
	lengths[0] = strlen(values[0]);

	// Run statement 
	result = PQexecPrepared(conn, "CLEAR_TRIAL", 1, values, lengths, NULL, 0);

	// Clean session
	PQclear(result);
	FreeConnection(conn);
}

vector<nlohmann::json> DBWrapper::FeaturesBetween(double start_time,
                                                   double end_time,
                                                   std::string participant_id,
                                                   std::string trial_id) {
    PGconn* conn;
    PGresult* result;

    conn = GetConnection();
	
    const char* values[4];
    int lengths[4];
    values[0] = to_string(start_time).c_str();
    values[1] = to_string(end_time).c_str();
    values[2] = participant_id.c_str();
    values[3] = trial_id.c_str();
    lengths[0] = strlen(values[0]);
    lengths[1] = strlen(values[1]);
    lengths[2] = strlen(values[2]);
    lengths[3] = strlen(values[3]);

    result = PQexecPrepared(conn, "CHUNK_EXTRACT", 4, values, lengths, NULL, 0);
    
    // Turn features into json object
    vector<nlohmann::json> out;
    for (int i = 0; i < PQntuples(result); i++) {
        nlohmann::json message;
        for (int j = 0; j < PQnfields(result); j++) {
            if (this->column_map.find(PQfname(result, j)) ==
                this->column_map.end()) {
                continue;
            }
            string field = this->column_map[PQfname(result, j)];
            double value = atof(PQgetvalue(result, i, j));
            message[field] = value;
        }
        out.push_back(message);
    }

    // Clean session
    PQclear(result);
    FreeConnection(conn);
    return out;
}


string DBWrapper::format_to_db_string(std::string in) {
	return column_map[in];
}

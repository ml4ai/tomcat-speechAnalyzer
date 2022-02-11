#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <queue>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <libpq-fe.h>
#include <nlohmann/json.hpp>

#include "DBWrapper.h"
#include "GlobalMosquittoListener.h"

using namespace std;

const vector<char> DBWrapper::INVALID_COLUMN_CHARACTERS = {
    '+', '-', '(', ')', '\n', '.'};

DBWrapper::DBWrapper() {}

DBWrapper::~DBWrapper() {}

void DBWrapper::initialize() {
    this->running = true;

    // Create connection string
    this->connection_string = "host=" + this->host + " port=" + this->port +
                              " dbname=" + this->db + " user=" + this->user +
                              " password= " + this->pass;

    for (int i = 0; i < this->thread_pool_size; i++) {
        // Create connection objects
        this->connection_pool.push_back(
            PQconnectdb(this->connection_string.c_str()));
        this->thread_pool.push_back(std::thread{[this, i] { this->loop(i); }});
        this->queue_pool.push_back(std::queue<nlohmann::json>());
    }
}
void DBWrapper::shutdown() {
    this->running = false;
    for (int i = 0; i < this->thread_pool_size; i++) {
        this->thread_pool[i].join();
        PQfinish(this->connection_pool[i]);
    }
    std::cout << "Shutdown DB connections" << std::endl;
}

PGconn* DBWrapper::get_connection() {
    return this->connection_pool[rand() % this->thread_pool_size];
}

void DBWrapper::loop(int index) {
    while (this->running) {
        if (!this->queue_pool[index].empty()) {
            nlohmann::json message = this->queue_pool[index].front();
            this->queue_pool[index].pop();
            this->publish_chunk_private(message, index);
        }
        else {
            continue;
        }
    }
}

void DBWrapper::publish_chunk(nlohmann::json message) {
    this->queue_pool[rand() % this->thread_pool_size].push(message);
}

void DBWrapper::publish_chunk_private(nlohmann::json message, int index) {
    PGconn* conn;
    PGresult* result;

    conn = this->connection_pool[index];

    // Generate columns and values
    vector<string> columns;
    vector<double> values;
    for (auto element : message["data"]["features"]["lld"].items()) {
        columns.push_back(this->format_to_db_string(element.key()));
        values.push_back(element.value());
    }

    // Get timestamp
    this->timestamp = message["data"]["tmeta"]["time"];

    this->participant_id = to_string(message["data"]["participant_id"]);
    boost::replace_all(this->participant_id, "\"", "\'");
    this->trial_id = to_string(message["msg"]["trial_id"]);
    boost::replace_all(this->trial_id, "\"", "\'");
    this->experiment_id = to_string(message["msg"]["experiment_id"]);
    boost::replace_all(this->experiment_id, "\"", "\'");

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
        std::cout << "Execution error: " << std::endl;
        std::cout << PQerrorMessage(conn) << std::endl;
    }
    // Clear result
    PQclear(result);
}

vector<nlohmann::json> DBWrapper::features_between(double start_time,
                                                   double end_time) {
    PGconn* conn;
    PGresult* result;

    // Create connection
    conn = PQconnectdb(this->connection_string.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cout << "Connection error: " << std::endl;
        std::cout << PQerrorMessage(conn) << std::endl;
    }

    // Get features from database
    std::string query = "SELECT * FROM features WHERE seconds_offset >= " +
                        to_string(start_time) +
                        " and seconds_offset <= " + to_string(end_time) +
                        " and participant=" + this->participant_id +
                        " and trial_id=" + this->trial_id; //+
    result = PQexec(conn, query.c_str());
    if (result == NULL) {
        std::cout << "FAILURE" << std::endl;
        std::cout << PQerrorMessage(conn) << std::endl;
    }
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

    // Clear result
    PQclear(result);
    PQfinish(conn);
    return out;
}

string DBWrapper::format_to_db_string(std::string in) {
    // Check if value already in map
    if (this->column_map.find(in) != this->column_map.end()) {
        return this->column_map[in];
    }

    string original = string(in);
    boost::to_lower(in);
    boost::replace_all(in, ")", "");
    for (char c : this->INVALID_COLUMN_CHARACTERS) {
        boost::replace_all(in, string(1, c), "_");
    }
    this->column_map[in] = original;
    this->column_map[original] = in;

    return in;
}

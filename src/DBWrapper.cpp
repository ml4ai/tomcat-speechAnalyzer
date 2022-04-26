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

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/trivial.hpp>
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

DBWrapper::DBWrapper() { this->Initialize(); }

DBWrapper::~DBWrapper() {
    if (this->running) {
        this->Shutdown();
    }
}

void DBWrapper::Initialize() {
    this->running = true;

    // Initialize Column Map
    this->InitializeColumnMap();

    // Create connection string
    this->connection_string = "host=" + this->host + " port=" + this->port +
                              " dbname=" + this->db + " user=" + this->user +
                              " password= " + this->pass;

    // Initialize connection pool
    for (int i = 0; i < this->connection_pool_size; i++) {
        // Create connection objects
        this->connection_pool.push(
            PQconnectdb(this->connection_string.c_str()));
    }
}

void DBWrapper::Shutdown() {
    this->running = false;
    BOOST_LOG_TRIVIAL(info) << "Shutdown DB connections";
}

PGconn* DBWrapper::GetConnection() {
    PGconn* conn;
    std::unique_lock<std::mutex> guard(connection_mutex);
    if (this->ConnectionAvaliable()) {
        conn = connection_pool.top();
        connection_pool.pop();
    }
    else {
        this->connection_condition.wait(
            guard, [this] { return this->ConnectionAvaliable(); });
        conn = connection_pool.top();
        connection_pool.pop();
    }
    return conn;
}

void DBWrapper::FreeConnection(PGconn* conn) {
    std::lock_guard<std::mutex> guard(connection_mutex);
    this->connection_pool.push(conn);
    this->connection_condition.notify_one();
}

bool DBWrapper::ConnectionAvaliable() {
    if (this->connection_pool.empty()) {
        return false;
    }
    return true;
}
void DBWrapper::publish_chunk(nlohmann::json message) {
    // Create thread object to handle io
    std::thread io([this, message] { this->publish_chunk_private(message); });
    io.detach();
}

void DBWrapper::publish_chunk_private(nlohmann::json message) {
    PGconn* conn;
    PGresult* result;

    // Get connection object from pool
    conn = this->GetConnection();

    // Generate columns and values
    vector<string> columns;
    vector<double> values;
    for (auto element : message["data"]["features"]["lld"].items()) {
        columns.push_back(this->format_to_db_string(element.key()));
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
    // Clear result
    PQclear(result);

    // Add connection object back to pool
    this->FreeConnection(conn);
}

vector<nlohmann::json> DBWrapper::features_between(double start_time,
                                                   double end_time,
                                                   std::string participant_id,
                                                   std::string trial_id) {
    PGconn* conn;
    PGresult* result;

    // Create connection
    conn = PQconnectdb(this->connection_string.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        BOOST_LOG_TRIVIAL(error)
            << "Connection error: " << PQerrorMessage(conn);
    }

    // Get features from database
    std::string query = "SELECT * FROM features WHERE seconds_offset >= " +
                        to_string(start_time) +
                        " and seconds_offset <= " + to_string(end_time) +
                        " and participant=" + "\'" + participant_id + "\'" +
                        " and trial_id=" + "\'" + trial_id + "\'"; //+
    result = PQexec(conn, query.c_str());
    if (result == NULL) {
        BOOST_LOG_TRIVIAL(error) << "FAILURE" << PQerrorMessage(conn);
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

void DBWrapper::InitializeColumnMap() {
    ifstream file("conf/column_map.txt");
    string opensmile_format;
    string postgres_format;

    while (!file.eof()) {
        file >> opensmile_format;
        file >> postgres_format;
        this->column_map[opensmile_format] = postgres_format;
        this->column_map[postgres_format] = opensmile_format;
    }
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

// STDLIB
#include <cstdlib>
#include <string>
#include <thread>
#include <iostream>

// Third Party - Boost
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>


// Third Party
#include <nlohmann/json.hpp>

// Local
#include "OpensmileProcessor.h"

using namespace std;

OpensmileProcessor::OpensmileProcessor(string participant_id, string trial_id, string experiment_id) {
    this->participant_id = participant_id;
    this->trial_id = trial_id;
    this->experiment_id = experiment_id;

    // Create database object
    postgres = make_unique<DBWrapper>(25); // 25 connections for publishing chunks 
}

void OpensmileProcessor::ProcessOpensmileLog(string message) {
    string temp(message);
    temp.erase(remove(temp.begin(), temp.end(), ' '), temp.end());
    if (tmeta) {
        if (temp.find("lld") != string::npos) {
            postgres.PublishChunk(opensmile_message);
            opensmile_message["data"]["participant_id"] =
                participant_id;
            opensmile_message["header"] =
                create_common_header("observation");
            opensmile_message["msg"] = create_common_msg("openSMILE");
            tmeta = false;
        }
        if (tmeta) {
            auto equals_index = temp.find('=');
            string field = temp.substr(0, equals_index);
            double value = atof(temp.substr(equals_index + 1).c_str());
            opensmile_message["data"]["tmeta"][field] = value;
        }
    }

    if (temp.find("lld") != string::npos) {
        std::string lld = temp.substr(4);
        auto equals_index = lld.find('=');
        string field = lld.substr(0, equals_index);
        double value = atof(lld.substr(equals_index + 1).c_str());

        // Replace '[' and ']' characters
        size_t open = field.find("[");
        size_t close = field.find("]");

        if (open != string::npos && close != string::npos) {
            field.replace(open, 1, "(");
            field.replace(close, 1, ")");
        }

        opensmile_message["data"]["features"]["lld"][field] = value;
    }

    if (temp.find("tmeta:") != string::npos) {
        tmeta = true;
    }
}

// Methods for creating common message types
nlohmann::json OpensmileProcessor::create_common_header(string message_type) {
    nlohmann::json header;
    string timestamp =
        boost::posix_time::to_iso_extended_string(
            boost::posix_time::microsec_clock::universal_time()) +
        "Z";

    header["timestamp"] = timestamp;
    header["message_type"] = message_type;
    header["version"] = "1.1";

    return header;
}

nlohmann::json OpensmileProcessor::create_common_msg(std::string sub_type) {
    nlohmann::json message;
    string timestamp =
        boost::posix_time::to_iso_extended_string(
            boost::posix_time::microsec_clock::universal_time()) +
        "Z";

    message["timestamp"] = timestamp;
    message["experiment_id"] = experiment_id;
    message["trial_id"] = trial_id;
    message["version"] = "0.6";
    message["source"] = "tomcat_speech_analyzer";
    message["sub_type"] = sub_type;

    return message;
}

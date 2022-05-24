#pragma once

// STDLIB
#include <string>
#include <thread>
#include <vector>
#include <memory>

// Third party
#include <nlohmann/json.hpp>

// Local
#include "DBWrapper.h"

class OpensmileProcessor {
  public:
    OpensmileProcessor(std::string participant_id, std::string trial_id, std::string experiment_id);

    // Process an openSMILE log message
    void ProcessOpensmileLog(std::string message);

  private:
    std::string participant_id;
    std::string trial_id;
    std::string experiment_id;

    // Data for handling openSMILE messages
    nlohmann::json opensmile_message;
    bool tmeta = false;
    
    // DBWrapper object
    std::unique_ptr<DBWrapper> postgres;

    // Functions for creating cmomon message types
    nlohmann::json create_common_header(std::string message_type);
    nlohmann::json create_common_msg(std::string sub_type);
};

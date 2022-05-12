#pragma once
#include "Mosquitto.h"
#include "TrialListener.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <deque>
#include <nlohmann/json.hpp>
#include <smileapi/SMILEapi.h>
#include <string>
#include <thread>
#include <vector>

#include "DBWrapper.h"
#include "arguments.h"

class JsonBuilder {
  public:
    static Arguments args;
    std::string participant_id;
    std::string trial_id;
    std::string experiment_id;

    JsonBuilder();
    ~JsonBuilder();

    void Initialize();
    void Shutdown();

    // Process an openSMILE log messag
    void process_message(std::string message);
    void process_sentiment_message(nlohmann::json m);
    std::string process_mmc_message(std::string message);



  private:
    // Mosquitto client objects
    Mosquitto mosquitto_client;
    TrialListener listener_client;
    std::thread listener_client_thread;

    // Data for handling openSMILE messages
    nlohmann::json opensmile_message;
    std::vector<std::string> feature_list;
    bool tmeta = false;
    double sync_time = 0.0;
    std::deque<nlohmann::json> opensmile_history;
    std::vector<nlohmann::json> features_between(double start_time,
                                                 double end_time);
    // DBWrapper object
    DBWrapper postgres;

    // Functions for creating cmomon message types
    nlohmann::json create_common_header(std::string message_type);
    nlohmann::json create_common_msg(std::string sub_type);
};

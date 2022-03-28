#pragma once
#include "Mosquitto.h"
#include "TrialListener.h"
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <deque>
#include <nlohmann/json.hpp>
#include <smileapi/SMILEapi.h>
#include <string>
#include <thread>
#include <vector>

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

    // Process an asr message
    void process_asr_message(
        google::cloud::speech::v1::StreamingRecognizeResponse response,
        std::string id);

    // Process an asr message from vosk
    void process_asr_message_vosk(std::string response);

    // Update the sync time for word/feature alignment messages
    void update_sync_time(double sync_time);

  private:
    // Time object for start of stream
    boost::posix_time::ptime stream_start_time;
    boost::posix_time::ptime stream_start_time_vosk;

    // Bool object for determining start of utterance
    bool is_initial = false;
    std::string utterance_start_timestamp;

    // Mosquitto client objects
    Mosquitto mosquitto_client;
    Mosquitto mosquitto_client_internal;
    TrialListener listener_client;
    std::thread listener_client_thread;

    // Data for handling openSMILE messages
    double sync_time = 0.0;
    
    // Functions for creating cmomon message types
    nlohmann::json create_common_header(std::string message_type);
    nlohmann::json create_common_msg(std::string sub_type);
};

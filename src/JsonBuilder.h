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

    // Process an openSMILE log messag
    void process_message(std::string message);

    // Process an asr message
    void process_asr_message(
        google::cloud::speech::v1::StreamingRecognizeResponse response,
        std::string id);

    // Process an asr message from vosk
    void process_asr_message_vosk(
        std::string response);
     
    // Process a word/feature alignment message
    std::string process_alignment_message(
        google::cloud::speech::v1::StreamingRecognizeResponse response,
        std::string id);
    std::string process_alignment_message_vosk(nlohmann::json response, std::string id);
    std::string process_mmc_message(std::string message);

    // Process a audio chunk message
    void process_audio_chunk_message(std::vector<char> chunk, std::string id);
    void process_audio_chunk_metadata_message(std::vector<char> chunk,
                                              std::string id);

    // Update the sync time for word/feature alignment messages
    void update_sync_time(double sync_time);

  private:
    // Time object for start of stream
    boost::posix_time::ptime stream_start_time;
    boost::posix_time::ptime stream_start_time_vosk;

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

    // Functions for creating cmomon message types
    nlohmann::json create_common_header(std::string message_type);
    nlohmann::json create_common_msg(std::string sub_type);
};

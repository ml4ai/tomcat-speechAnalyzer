#pragma once

#include <string>
#include <vector>

#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include <grpc++/grpc++.h>

class SpeechWrapper {
  public:
    SpeechWrapper(bool dummy_read);
    SpeechWrapper(bool dummy_read, int sample_rate);
    ~SpeechWrapper();

    // Speech Session state variables
    std::unique_ptr<grpc::ClientReaderWriterInterface<
        google::cloud::speech::v1::StreamingRecognizeRequest,
        google::cloud::speech::v1::StreamingRecognizeResponse>>
        streamer;
    grpc::Status status;
    grpc::ClientContext context;

    void start_stream();
    void finish_stream();
    void send_chunk(std::vector<int16_t> int_chunk);
    void send_writes_done();

  private:
    int sample_rate = 48000;
    bool dummy_read = false;
    bool finished = false;
    std::vector<std::string> speech_context;

    void initialize_stream();
    void load_speech_context();
    void send_config();
};

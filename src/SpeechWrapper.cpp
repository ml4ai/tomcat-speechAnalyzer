#include <algorithm>
#include <fstream>
#include <string>

#include <boost/log/trivial.hpp>

#include "SpeechWrapper.h"

#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include <grpc++/grpc++.h>

using google::cloud::speech::v1::Speech,
    google::cloud::speech::v1::RecognitionConfig,
    google::cloud::speech::v1::StreamingRecognizeRequest,
    google::cloud::speech::v1::StreamingRecognizeResponse,
    google::cloud::speech::v1::StreamingRecognitionConfig;

using namespace std;

SpeechWrapper::SpeechWrapper(bool dummy_read) { this->dummy_read = dummy_read; }

SpeechWrapper::SpeechWrapper(bool dummy_read, int sample_rate) {
    this->dummy_read = dummy_read;
    this->sample_rate = sample_rate;
}

SpeechWrapper::~SpeechWrapper() {}

void SpeechWrapper::start_stream() {
    initialize_stream();
    send_config();
}

void SpeechWrapper::finish_stream() {
    // Read responses if dummy_read
    if (dummy_read) {
        StreamingRecognizeResponse response;
        while (streamer->Read(&response)) {
        }
    }

    // Finish stream
    status = streamer->Finish();
    if (!status.ok()) {
        // Report the RPC failure.
        BOOST_LOG_TRIVIAL(error) << status.error_message();
    }
}

void SpeechWrapper::send_chunk(vector<int16_t> int_chunk) {
    StreamingRecognizeRequest content_request;
    content_request.set_audio_content(&int_chunk[0],
                                      int_chunk.size() * sizeof(int16_t));
    streamer->Write(content_request);
}

void SpeechWrapper::send_writes_done() { streamer->WritesDone(); }

void SpeechWrapper::initialize_stream() {
    // Create speech stub
    auto creds = grpc::GoogleDefaultCredentials();
    auto channel = grpc::CreateChannel("speech.googleapis.com", creds);
    unique_ptr<Speech::Stub> speech(Speech::NewStub(channel));

    // Start stream
    streamer = speech->StreamingRecognize(&context);
}

void SpeechWrapper::send_config() {
    this->load_speech_context();

    // Write first request with config
    StreamingRecognizeRequest config_request;
    StreamingRecognitionConfig* streaming_config =
        config_request.mutable_streaming_config();

    RecognitionConfig* mutable_config = streaming_config->mutable_config();
    mutable_config->set_language_code("en-US");
    mutable_config->set_sample_rate_hertz(this->sample_rate);
    mutable_config->set_encoding(RecognitionConfig::LINEAR16);
    mutable_config->set_max_alternatives(5);
    mutable_config->set_enable_word_time_offsets(true);
    mutable_config->set_use_enhanced(true);
    mutable_config->set_model("video");

    auto context = mutable_config->add_speech_contexts();
    for (string phrase : this->speech_context) {
        context->add_phrases(phrase);
        context->set_boost(7.5);
    }

    streaming_config->set_interim_results(true);
    streamer->Write(config_request);
}

void SpeechWrapper::load_speech_context() {
    int line_count = 0;
    int character_count = 0;
    int max_character_length = 0;

    ifstream file("conf/speech_context.txt");
    string line;
    while (getline(file, line)) {
        // Check if comment
        if (line.find('#') != string::npos) {
            continue;
        }

        line_count++;
        character_count += line.length();
        max_character_length = max<int>(max_character_length, line.length());
        this->speech_context.push_back(line);
    }

    // Check limits
    if (line_count > 5000) {
        BOOST_LOG_TRIVIAL(error)
            << "Number of phrases provided for speech adaptation ("
            << line_count << ") exceeds the limit (5000)!";
        this->speech_context.clear();
    }
    if (character_count > 100000) {
        BOOST_LOG_TRIVIAL(error)
            << "Number of characters provided for speech adaptation ("
            << character_count << ") exceeds the limit (100,000)!";
        this->speech_context.clear();
    }

    if (max_character_length > 100) {
        BOOST_LOG_TRIVIAL(error)
            << "Maximum number of characters per phrase for speech adaptation ("
            << max_character_length << ") exceeds the limit (100)!";
        this->speech_context.clear();
    }
}

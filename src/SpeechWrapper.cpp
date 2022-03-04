#include <fstream>
#include <string>
#include <iostream>
#include <string>
#include <typeinfo>

#include "SpeechWrapper.h"

//#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include "google/cloud/speech/v1p1beta1/cloud_speech.grpc.pb.h"
#include <grpc++/grpc++.h>

//using google::cloud::speech::v1::RecognitionConfig;
//using google::cloud::speech::v1::Speech;
//using google::cloud::speech::v1::StreamingRecognizeRequest;
//using google::cloud::speech::v1::StreamingRecognizeResponse;
//using google::cloud::speech::v1::StreamingRecognitionConfig;
using google::cloud::speech::v1p1beta1::RecognitionConfig;
using google::cloud::speech::v1p1beta1::Speech;
using google::cloud::speech::v1p1beta1::StreamingRecognizeRequest;
using google::cloud::speech::v1p1beta1::StreamingRecognizeResponse;
using google::cloud::speech::v1p1beta1::StreamingRecognitionConfig;

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
        cerr << status.error_message() << endl;
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
    // Load speech context
    this->load_speech_context();

    // Write first request with config
    StreamingRecognizeRequest config_request;
    StreamingRecognitionConfig* streaming_config = config_request.mutable_streaming_config();

    RecognitionConfig* mutable_config = streaming_config->mutable_config();
//    mutable_config->set_language_code("en");
    mutable_config->set_language_code("en-US");
    mutable_config->set_sample_rate_hertz(this->sample_rate);
    mutable_config->set_encoding(RecognitionConfig::LINEAR16);
    mutable_config->set_max_alternatives(5);
    mutable_config->set_enable_word_time_offsets(true);
    // converting to the 'video' model seems to have a great impact on output
    mutable_config->set_model("video");

    // Add speech context phrases
    auto context = mutable_config->add_speech_contexts();
//    context->add_phrases("rubble cave-in");
//    context->set_boost(10000.0);

    for (string phrase : this->speech_context) {
        context->add_phrases(phrase);
        // try adding boosts to each phrase ?
//        if (phrase.find("rubble") != string::npos) {
//            context->set_boost(20.0);
//            float boost = 20.0;
//            std::cout << phrase << " ";
//            std::cout << boost << std::endl;
//        } else {
        context->set_boost(7.5);
//        float boost = 10.0;
//        std::cout << phrase << " ";
//        std::cout << boost << std::endl;
//        }


    }
////    std::cout << speech_context.phrases << std::endl;
//    // try adding a single boost to all phrases first
//    context->set_boost(10000.0);

//    std::cout << "this part has worked" << std::endl;

//    std::cout << typeid(mutable_config).name() << std::endl;

//    std::string config_info = *mutable_config;
//    std::cout << config_info << std::endl;
//    std::cout << context << std::endl;
//    std::cout << streaming_config;

//    ofstream thefile;
//    thefile.open ("google_config.txt");
//    thefile << mutable_config;
//    thefile.close();

    streaming_config->set_interim_results(true);
    streamer->Write(config_request);
}

void SpeechWrapper::load_speech_context() {
    ifstream file("speech_context.txt");
    string line;
    while (getline(file, line)) {
//        std::cout << line << std::endl;
        this->speech_context.push_back(line);
    }
//    std::vector<char> speech_context = speech_context;
//    for (char i: speech_context) {
//        std::cout << i << std::endl;
//    }

//    std::cout << speech_context << std::endl;
}

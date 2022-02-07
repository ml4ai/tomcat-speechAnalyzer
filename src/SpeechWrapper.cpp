#include <string>
#include <fstream>

#include "SpeechWrapper.h"

#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include <grpc++/grpc++.h>

using google::cloud::speech::v1::RecognitionConfig;
using google::cloud::speech::v1::Speech;
using google::cloud::speech::v1::StreamingRecognizeRequest;
using google::cloud::speech::v1::StreamingRecognizeResponse;

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
    auto* streaming_config = config_request.mutable_streaming_config();

    auto mutable_config = streaming_config->mutable_config();
    mutable_config->set_language_code("en");
    mutable_config->set_sample_rate_hertz(this->sample_rate);
    mutable_config->set_encoding(RecognitionConfig::LINEAR16);
    mutable_config->set_max_alternatives(5);
    mutable_config->set_enable_word_time_offsets(true);
    
    // Add speech context phrases 
    auto context = mutable_config->add_speech_contexts();
    for(string phrase : this->speech_context){
	context->add_phrases(phrase);
    }

    streaming_config->set_interim_results(true);
    streamer->Write(config_request);
}

void SpeechWrapper::load_speech_context(){
	ifstream file("speech_context.txt");
	string line;
	while(getline(file, line)){
		this->speech_context.push_back(line);
	}
}

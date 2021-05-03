#include <boost/date_time/posix_time/posix_time.hpp>
#include <cerrno>  //errno
#include <cstring> //strerror
#include <fstream>
#include <grpc++/grpc++.h>
#include <iostream>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include "JsonBuilder.cpp"
#include "SMILEapi.h"
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include "parse_arguments.h"

using google::cloud::speech::v1::RecognitionConfig;
using google::cloud::speech::v1::Speech;
using google::cloud::speech::v1::StreamingRecognizeRequest;
using google::cloud::speech::v1::StreamingRecognizeResponse;

enum speech_analyzer_mode { STDIN, WEBSOCKET, MICROPHONE };
struct audio_data_format {
    int chunk_size = 2048;
    int sample_rate = 44100;
    int n_bits = 16;
};
class SpeechAnalyzer() {
  public:
    SpeechAnalyzer();

    void setup();

    void run();

  private:
    // Audio data format
    struct audio_data_format format;

    speech_analyzer_mode mode;

    // Chunk readers
    StdinReader stdin_reader;
    WebsockerReader websocket_reader;

    // Thread objects
    std::thread openSMILE_thread;
    std::thread google_asr_thread;
    std::thread reader_thread;

    // openSMILE resources
    // Initialize and start opensmile
    smileobj_t* handle;
    // JsonBuilder object which will be passed to openSMILE log callback
    JsonBuilder builder;
};

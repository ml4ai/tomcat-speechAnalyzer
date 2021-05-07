// STD
#include <cerrno>  //errno
#include <cstring> //strerror
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Boost
#include <boost/chrono.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

// Third Party Libraries
#include "JsonBuilder.h"
#include "Mosquitto.h"
#include "SpeechWrapper.h"
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include <grpc++/grpc++.h>
#include <smileapi/SMILEapi.h>

// Global variables
#include "arguments.h"
#include "spsc.h"
#include "util.h"

// Websocket Server files
#include "WebsocketSession.h"
#include "HTTPSession.h"
#include "Listener.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace ws = beast::websocket;
namespace asio = boost::asio;

using tcp = boost::asio::ip::tcp;

using google::cloud::speech::v1::RecognitionConfig;
using google::cloud::speech::v1::Speech;
using google::cloud::speech::v1::StreamingRecognizeRequest;
using google::cloud::speech::v1::StreamingRecognizeResponse;
using google::cloud::speech::v1::WordInfo;
using namespace boost::program_options;
using namespace boost::chrono;
using namespace std;

void read_chunks_stdin(Arguments args);
void read_chunks_websocket(Arguments args);
void write_thread(Arguments args);
boost::lockfree::spsc_queue<vector<float>, boost::lockfree::capacity<1024>>
    shared;
bool read_done = false;
bool write_start = false;

Arguments JsonBuilder::args;
Arguments WebsocketSession::args;

int main(int argc, char* argv[]) {
    // Handle options
    Arguments args;
    try {
        options_description desc{"Options"};
        desc.add_options()("help,h", "Help screen")(
            "mode",
            value<string>(&args.mode)->default_value("stdin"),
            "Where to read audio chunks from")(
            "mqtt_host",
            value<string>(&args.mqtt_host)->default_value("mosquitto"),
            "The host address of the mqtt broker")(
            "mqtt_port",
            value<int>(&args.mqtt_port)->default_value(1883),
            "The port of the mqtt broker")(
            "ws_host",
            value<string>(&args.ws_host)->default_value("0.0.0.0"),
            "The host address of the websocket server")(
            "ws_port",
            value<int>(&args.ws_port)->default_value(8888),
            "The port of the websocket server")(
            "disable_asr",
            value<bool>(&args.disable_asr)->default_value(false),
            "Disable the asr system of the speechAnalyzer agent")(
            "disable_opensmile",
            value<bool>(&args.disable_opensmile)->default_value(false),
            "Disable the opensmile feature extraction system of the "
            "speechAnalyzer agent")(
            "disable_audio_writing",
            value<bool>(&args.disable_audio_writing)->default_value(false),
            "Disable writing audio files for the speechAnalyzer agent");
    }
    catch (const error& ex) {
        cout << "Error parsing arguments" << endl;
        return -1;
    }
    JsonBuilder::args = args;
    WebsocketSession::args = args;
    if (args.mode.compare("stdin") == 0) {
        thread thread_object(read_chunks_stdin, args);
        thread thread_object2(write_thread, args);
        thread_object.join();
        thread_object2.join();
    }
    else if (args.mode.compare("websocket") == 0) {
        thread thread_object(read_chunks_websocket, args);
        thread_object.join();
    }
    else {
        cout << "Unknown mode" << endl;
    }

    return 0;
}

void read_chunks_stdin(Arguments args) {
    while (!write_start) {
    }

    freopen(nullptr, "rb", stdin); // reopen stdin in binary mode

    vector<float> chunk(1024);
    size_t length;
    while ((length = fread(&chunk[0], sizeof(float), 1024, stdin)) > 0) {
        while (!shared.push(chunk)) {
        } // If queue is full it will keep trying until avaliable space
    }

    read_done = true;
}

void read_chunks_websocket(Arguments args) {
    cout << "Starting Websocket Server" << endl;
    auto const address = asio::ip::make_address(args.ws_host);
    auto const port = static_cast<unsigned short>(args.ws_port);
    auto const doc_root = make_shared<string>(".");
    auto const n_threads = 4;

    asio::io_context ioc{n_threads};

    auto listener =
        make_shared<Listener>(ioc, tcp::endpoint{address, port}, doc_root);
    listener->run();

    asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](beast::error_code const&, int) { ioc.stop(); });

    vector<thread> threads;
    threads.reserve(n_threads - 1);
    for (auto i = n_threads; i > 0; --i) {
        threads.emplace_back([&ioc] { ioc.run(); });
    }
    ioc.run();

    for (auto& thread : threads) {
        thread.join();
    }
}

void write_thread(Arguments args) {
    int sample_rate = 44100;
    int samples_done = 0;

    // JsonBuilder object which will be passed to openSMILE log callback
    JsonBuilder builder;

    // Initialize and start opensmile
    smileobj_t* handle;
    handle = smile_new();
    smile_initialize(
        handle, "conf/is09-13/IS13_ComParE.conf", 0, NULL, 1, 0, 0, 0);
    smile_set_log_callback(handle, &log_callback, &builder);

    // Initialize openSMILE thread
    thread opensmile_thread(smile_run, handle);

    // Start speech streamer
    SpeechWrapper* speech_handler = new SpeechWrapper(false);
    speech_handler->start_stream();
    process_real_cpu_clock::time_point stream_start =
        process_real_cpu_clock::now(); // Need to know starting time to restart
                                       // stream
    // Initialize response reader thread
    thread asr_reader_thread(
        process_responses, speech_handler->streamer.get(), &builder);

    ofstream float_sample("float_sample", ios::out | ios::binary | ios::trunc);
    ofstream int_sample("int_sample", ios::out | ios::binary | ios::trunc);
    StreamingRecognizeRequest content_request;
    vector<float> chunk(1024);
    write_start = true;
    while (!read_done) {
        while (shared.pop(chunk)) {
            samples_done += 1024;
            // Write to opensmile
            while (true) {
                smileres_t result = smile_extaudiosource_write_data(
                    handle,
                    "externalAudioSource",
                    (void*)&chunk[0],
                    chunk.size() * sizeof(float));
                if (result == SMILE_SUCCESS) {
                    break;
                }
            }

            // Convert 32f chunk to 16i chunk
            vector<int16_t> int_chunk;
            for (float f : chunk) {
                int_chunk.push_back((int16_t)(f * 32768));
            }
            float_sample.write((char*)&chunk[0], sizeof(float) * chunk.size());
            int_sample.write((char*)&int_chunk[0],
                             sizeof(int16_t) * int_chunk.size());

            // Write to google asr service
            speech_handler->send_chunk(int_chunk);

            // Check if asr stream needs to be restarted
            process_real_cpu_clock::time_point stream_current =
                process_real_cpu_clock::now();
            if (stream_current - stream_start > seconds{240}) {
                // Send writes_done and finish reading responses
                speech_handler->send_writes_done();
                asr_reader_thread.join();
                // End the stream
                speech_handler->finish_stream();
                // Sync openSMILE time
                double sync_time = (double)samples_done / sample_rate;
                builder.update_sync_time(sync_time);
                // Create new stream
                speech_handler = new SpeechWrapper(false);
                speech_handler->start_stream();
                // Restart response reader thread
                asr_reader_thread = thread(process_responses,
                                           speech_handler->streamer.get(),
                                           &builder);
                stream_start = process_real_cpu_clock::now();
            }
        }
    }
    float_sample.close();
    int_sample.close();

    speech_handler->send_writes_done();
    asr_reader_thread.join();
    speech_handler->finish_stream();
    
    smile_extaudiosource_set_external_eoi(handle, "externalAudioSource");
    opensmile_thread.join();
}

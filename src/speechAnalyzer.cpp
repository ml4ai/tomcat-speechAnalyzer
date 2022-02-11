// STD
#include <cerrno>  //errno
#include <cstring> //strerror
#include <fstream>
#include <iostream>
#include <mutex>
#include <smileapi/SMILEapi.h>
#include <string>
#include <thread>
#include <vector>

// Boost
#include <boost/chrono.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/program_options.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>

// Third Party Libraries
#include "ChunkListener.h"
#include "GlobalMosquittoListener.h"
#include "JsonBuilder.h"
#include "Mosquitto.h"
#include "SpeechWrapper.h"
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include <grpc++/grpc++.h>
#include <smileapi/SMILEapi.h>

// Global variables
#include "arguments.h"
#include "util.h"

// Websocket Server files
#include "WebsocketSession.h"

#include "HTTPSession.h"

#include "Listener.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace ws = beast::websocket;
namespace asio = boost::asio;

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

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
void read_chunks_mqtt(Arguments args);
void write_thread(
    Arguments args,
    boost::lockfree::spsc_queue<vector<char>, boost::lockfree::capacity<1024>>*
        queue);

Arguments JsonBuilder::args;
Arguments WebsocketSession::args;
int WebsocketSession::socket_port;
std::vector<std::string> WebsocketSession::current_sessions;
std::mutex* WebsocketSession::session_mutex;
int main(int argc, char* argv[]) {

    // Enable Boost logging
    boost::log::add_console_log(std::cout,
                                boost::log::keywords::auto_flush = true);

    // Handle options
    Arguments args;
    try {
        options_description desc{"Options"};
        desc.add_options()("help,h", "Help screen")(
            "mode",
            value<string>(&args.mode)->default_value("stdin"),
            "Where to read audio chunks from")(
            "mqtt_host,l",
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
            "sample_rate",
            value<int>(&args.sample_rate)->default_value(48000),
            "The sample rate of the input audio")(
            "disable_asr_google",
            value<bool>(&args.disable_asr_google)->default_value(false),
            "Disable the google asr system of the speechAnalyzer agent")(
            "disable_asr_vosk",
            value<bool>(&args.disable_asr_vosk)->default_value(false),
            "Disable the vosk asr system of the speechAnalyzer agent")(
            "disable_opensmile",
            value<bool>(&args.disable_opensmile)->default_value(true),
            "Disable the opensmile feature extraction system of the "
            "speechAnalyzer agent")(
            "disable_audio_writing",
            value<bool>(&args.disable_audio_writing)->default_value(true),
            "Disable writing audio files for the speechAnalyzer agent")(
            "disable_chunk_publishing",
            value<bool>(&args.disable_chunk_publishing)->default_value(true),
            "Disable the publishing of audio chunks to the message bus")(
            "disable_chunk_metadata_publishing",
            value<bool>(&args.disable_chunk_metadata_publishing)
                ->default_value(true),
            "Disable the publishing of audio chunk  metadata to the message "
            "bus")(
            "intermediate_first_only",
            value<bool>(&args.intermediate_first_only)->default_value(false),
            "Only publish the first intermediate transcription for each "
            "utterance");

        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);
    }
    catch (const error& ex) {
        cout << "Error parsing arguments" << endl;
        return -1;
    }

    JsonBuilder::args = args;
    WebsocketSession::args = args;
    WebsocketSession::socket_port = 15556;
    WebsocketSession::current_sessions = std::vector<std::string>();
    WebsocketSession::session_mutex = new std::mutex();
    // Setup Global Listener
    GLOBAL_LISTENER.connect(args.mqtt_host, args.mqtt_port, 1000, 1000, 1000);
    GLOBAL_LISTENER.subscribe("trial");
    GLOBAL_LISTENER.subscribe("experiment");
    GLOBAL_LISTENER.set_max_seconds_without_messages(
        2147483647); // Max Long value
    GLOBAL_LISTENER_THREAD = thread([] { GLOBAL_LISTENER.loop(); });

    BOOST_LOG_TRIVIAL(info)
        << "Starting speechAnalyzer in " << args.mode << " mode";
    if (args.mode.compare("stdin") == 0) {
        thread thread_object(read_chunks_stdin, args);
        thread_object.join();
    }
    else if (args.mode.compare("websocket") == 0) {
        thread thread_object(read_chunks_websocket, args);
        thread_object.join();
    }
    else if (args.mode.compare("mqtt") == 0) {
        thread thread_object(read_chunks_mqtt, args);
        thread_object.join();
    }
    else {
        cout << "Unknown mode" << endl;
    }

    // Join Global Listener
    GLOBAL_LISTENER.close();
    GLOBAL_LISTENER_THREAD.join();

    return 0;
}

void read_chunks_stdin(Arguments args) {
    // Create spsc queue
    boost::lockfree::spsc_queue<vector<char>, boost::lockfree::capacity<1024>>
        queue;
    thread consumer_thread(write_thread, args, &queue);

    freopen(nullptr, "rb", stdin); // reopen stdin in binary mode

    vector<char> chunk(8196);
    size_t length;
    while ((length = fread(&chunk[0], 1, 8196, stdin)) > 0) {
        while (!queue.push(chunk)) {
        } // If queue is full it will keep trying until avaliable space
    }

    consumer_thread.join();
}

void read_chunks_mqtt(Arguments args) {
    // TODO: Implement replay of published chunks
    int num_participants = 4;
    vector<boost::lockfree::spsc_queue<vector<char>,
                                       boost::lockfree::capacity<1024>>>
        participant_queues(num_participants);
    vector<ChunkListener> chunk_listeners;
    vector<thread> read_threads;
    vector<thread> write_threads;
    // Create listener clients for each participant
    for (int i = 0; i < num_participants; i++) {
        ChunkListener listener("test", &participant_queues[i]);
        listener.connect(args.mqtt_host, args.mqtt_port, 1000, 1000, 1000);
        listener.set_max_seconds_without_messages(2137483647);
        listener.subscribe("audio");

        chunk_listeners.push_back(listener);
    }
    // Start read/write threads for each participant
    for (int i = 0; i < num_participants; i++) {
        read_threads.push_back(thread([&]() { chunk_listeners[i].loop(); }));
        write_threads.push_back(
            thread(write_thread, args, &participant_queues[i]));
    }

    while (true)
        ;
}
void read_chunks_websocket(Arguments args) {
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

void write_thread(
    Arguments args,
    boost::lockfree::spsc_queue<vector<char>, boost::lockfree::capacity<1024>>*
        queue) {
    int sample_rate = args.sample_rate;
    int samples_done = 0;
    bool is_float = true;
    bool is_int16 = false;

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
    SpeechWrapper* speech_handler = new SpeechWrapper(false, sample_rate);
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
    vector<char> chunk(8192);
    while (queue->pop(chunk)) {
        // Convert char vector to float and int16 vector
        std::vector<float> float_chunk(chunk.size() / sizeof(float));
        std::vector<int16_t> int_chunk(chunk.size() / sizeof(int16_t));
        if (is_float) {
            memcpy(&float_chunk[0], &chunk[0], chunk.size());
            int_chunk.clear();
            for (float f : float_chunk) {
                int_chunk.push_back((int16_t)(f * 32768));
            }
        }
        else if (is_int16) {
            memcpy(&int_chunk[0], &chunk[0], chunk.size());
            float_chunk.clear();
            for (int i : int_chunk) {
                float_chunk.push_back((float)(i / 32768.0));
            }
        }

        samples_done += 4096;

        // Write to opensmile
        while (true) {
            smileres_t result =
                smile_extaudiosource_write_data(handle,
                                                "externalAudioSource",
                                                (void*)&float_chunk[0],
                                                chunk.size() * sizeof(float));
            if (result == SMILE_SUCCESS) {
                break;
            }
        }

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
            speech_handler = new SpeechWrapper(false, sample_rate);
            speech_handler->start_stream();
            // Restart response reader thread
            asr_reader_thread = thread(
                process_responses, speech_handler->streamer.get(), &builder);
            stream_start = process_real_cpu_clock::now();
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

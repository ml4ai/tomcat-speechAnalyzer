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
#include "GlobalMosquittoListener.h"
#include "JsonBuilder.h"
#include "Mosquitto.h"
#include "SpeechWrapper.h"
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include <grpc++/grpc++.h>

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
            value<string>(&args.mode)->default_value("websocket"),
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
            value<bool>(&args.disable_asr_vosk)->default_value(true),
            "Disable the vosk asr system of the speechAnalyzer agent")(
            "disable_opensmile",
            value<bool>(&args.disable_opensmile)->default_value(true),
            "Disable the opensmile feature extraction system of the "
            "speechAnalyzer agent")(
            "disable_audio_writing",
            value<bool>(&args.disable_audio_writing)->default_value(true),
            "Disable writing audio files for the speechAnalyzer agent")(
            "intermediate_first_only",
            value<bool>(&args.intermediate_first_only)->default_value(true),
            "Only publish the first intermediate transcription for each "
            "utterance");

        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);
    }
    catch (const error& ex) {
        BOOST_LOG_TRIVIAL(error) << "Error parsing arguments!";
        return -1;
    }

    JsonBuilder::args = args;
    WebsocketSession::args = args;
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
        BOOST_LOG_TRIVIAL(info) << "Mode not supported in current version";
    }
    else if (args.mode.compare("websocket") == 0) {
        thread thread_object(read_chunks_websocket, args);
        thread_object.join();
    }
    else if (args.mode.compare("mqtt") == 0) {
        BOOST_LOG_TRIVIAL(info) << "Mode not supported in current version";
    }
    else {
        BOOST_LOG_TRIVIAL(info) << "Unknown mode";
    }

    // Join Global Listener
    GLOBAL_LISTENER.close();
    GLOBAL_LISTENER_THREAD.join();

    return 0;
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

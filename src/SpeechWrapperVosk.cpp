#include "SpeechWrapperVosk.h"
#include <iostream>
#include <string>

#include <sys/types.h>
#include <unistd.h>

#include "JsonBuilder.h"
#include <nlohmann/json.hpp>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/log/trivial.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using namespace std;

SpeechWrapperVosk::SpeechWrapperVosk(int sample_rate, JsonBuilder* builder) {
    this->sample_rate = sample_rate;
    this->builder = builder;
}
SpeechWrapperVosk::~SpeechWrapperVosk() { free(this->ws); }
void SpeechWrapperVosk::start_stream() {
    this->running = true;
    this->initialize_stream();
    this->send_config();
}

void SpeechWrapperVosk::end_stream() {
    // Stop read thread
    this->running = false;
    this->read_thread.join();

    // Close websocket
    this->ws->close(websocket::close_code::normal);
}
void SpeechWrapperVosk::initialize_stream() {
    // These objects perform our I/O
    tcp::resolver resolver(this->ioc);
    this->ws = new websocket::stream<tcp::socket>{this->ioc};

    // Look up the domain name
    auto const results = resolver.resolve(this->host, this->port);

    // Make the connection on the IP address we get from a lookup
    net::connect(this->ws->next_layer(), results.begin(), results.end());

    // Set a decorator to change the User-Agent of the handshake
    this->ws->set_option(
        websocket::stream_base::decorator([](websocket::request_type& req) {
            req.set(http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-client-coro");
        }));

    this->ws->handshake(this->host, "/");

    // Start read thread
    this->read_thread = std::thread([this]() {
        beast::flat_buffer buffer;
        while (this->running) {
            try {
                this->ws->read(buffer);
                string response = beast::buffers_to_string(buffer.data());
                buffer.consume(buffer.size());
                this->builder->process_asr_message_vosk(response);
            }
            catch (std::exception const& e) {
                BOOST_LOG_TRIVIAL(error)
                    << "Error recieving result from vosk server: " << e.what();
                this->running = false;
            }
        }
    });
}
void SpeechWrapperVosk::send_config() {
    // Set up configuration
    nlohmann::json config;
    config["config"]["sample_rate"] = this->sample_rate;
    config["config"]["max_alternatives"] = 5;

    // Send config
    this->ws->text(true);
    this->ws->write(net::buffer(config.dump()));
}
void SpeechWrapperVosk::send_chunk(vector<int16_t> int_chunk) {
    this->ws->binary(true);
    this->ws->write(net::buffer(&int_chunk[0], int_chunk.size()));
}
void SpeechWrapperVosk::send_writes_done() {
    // Send end of file
    ws->write(net::buffer("{\"eof\" : 1}"));
}

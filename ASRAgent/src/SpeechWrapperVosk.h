#include <string>
#include <thread>
#include <vector>

#include "JsonBuilder.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdlib>
#include <iostream>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class SpeechWrapperVosk {
  public:
    SpeechWrapperVosk(int sample_rate, JsonBuilder* builder);
    ~SpeechWrapperVosk();
    void start_stream();
    void end_stream();
    void send_chunk(std::vector<int16_t> int_chunk);
    void send_writes_done();

  private:
    bool running = false;
    JsonBuilder* builder;
    // Websocket session
    std::string host = "vosk";
    std::string port = "2700";

    net::io_context ioc;
    websocket::stream<tcp::socket>* ws;
    std::thread read_thread;

    // Speech Data
    int sample_rate = 8000;
    bool finished = false;

    void initialize_stream();
    void send_config();
};

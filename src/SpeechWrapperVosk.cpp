#include "SpeechWrapperVosk.h"
#include <iostream>
#include <string>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using namespace std;

SpeechWrapperVosk::SpeechWrapperVosk(int sample_rate) {
        this->sample_rate = sample_rate;
}
void SpeechWrapperVosk::start_stream(){
	this->initialize_stream();
}
void SpeechWrapperVosk::initialize_stream(){

        // These objects perform our I/O
        tcp::resolver resolver(this->ioc);
        this->stream = new beast::tcp_stream(this->ioc);
	//beast::tcp_stream stream(this->ioc);

        // Look up the domain name
        auto const results = resolver.resolve(this->host, this->port);

        // Make the connection on the IP address we get from a lookup
        this->stream->connect(results);
}

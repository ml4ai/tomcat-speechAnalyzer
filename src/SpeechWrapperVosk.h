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

class SpeechWrapperVosk {
	public:
        	SpeechWrapperVosk(int sample_rate);
		void start_stream();
	private:
		// Websocket session
		std::string host = "vosk";
		std::string port = "2700";
		std::string target = "";
		int version = 11;
	
		net::io_context ioc;
		beast::tcp_stream *stream;
	
		// Speech Data
		int sample_rate = 48000;
		bool finished = false;

		void initialize_stream();
		void send_config();
};

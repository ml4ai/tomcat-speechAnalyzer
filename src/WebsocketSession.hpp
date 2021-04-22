#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
//#include "SMILEapi.h"
#include "helper.h"

#include <smileapi/SMILEapi.h>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/log/trivial.hpp>
#include <grpc++/grpc++.h>
#include <range/v3/all.hpp>
#include <thread>
#include <fstream>
using google::cloud::speech::v1::Speech;
using google::cloud::speech::v1::StreamingRecognizeRequest;
using google::cloud::speech::v1::StreamingRecognizeResponse;

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace ws = beast::websocket;  // from <boost/beast/websocket.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

using namespace boost::chrono;

class WebsocketSession : public enable_shared_from_this<WebsocketSession> {

    ws::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    boost::lockfree::spsc_queue<std::vector<float>,
                                boost::lockfree::capacity<1024>>
        spsc_queue;

    std::atomic<bool> done{false};
    std::thread consumer_thread;
    std::thread asr_reader_thread;
    std::thread opensmile_thread;
    JsonBuilder builder;
    smileobj_t* handle;
    SpeechWrapper *speech_handler;
    process_real_cpu_clock::time_point stream_start;
    int samples_done = 0;
    int sample_rate = 48000;
    bool read_start = false;
    bool read_done = false;
    std::string participant_id;

  public:
    // Take ownership of the socket
    explicit WebsocketSession(tcp::socket&& socket) : ws_(move(socket)) {}

    // Start the asynchronous accept operation
    template <class Body, class Allocator>
    void do_accept(http::request<Body, http::basic_fields<Allocator>> request) {

        using ranges::to;
        using ranges::views::split, ranges::views::drop;

        std::map<std::string, std::string> params;

        auto param_strings = request.target() | drop(2) | split('&') |
                             to<std::vector<std::string>>();

        for (auto param_string : param_strings) {
            auto key_value_pair =
                param_string | split('=') | to<std::vector<std::string>>();
            params[key_value_pair[0]] = key_value_pair[1];
        }

        BOOST_LOG_TRIVIAL(debug) << "Id: " << params["id"]
                                 << ", sampleRate: " << params["sampleRate"];

	this->participant_id = params["id"];

        // Set suggested timeout settings for the websocket
        this->ws_.set_option(
            ws::stream_base::timeout::suggested(beast::role_type::server));

        // Set a decorator to change the server of the handshake
        this->ws_.set_option(
            ws::stream_base::decorator([](ws::response_type& res) {
                res.set(http::field::server,
                        std::string(BOOST_BEAST_VERSION_STRING) +
                            " advanced-server");
            }));

        // Accept the websocket handshake
        this->ws_.async_accept(
            request,
            beast::bind_front_handler(&WebsocketSession::on_accept,
                                      this->shared_from_this()));
    }

  private:
    void write_thread(){
	//Wait for writes to start
	while(!this->read_start);

    	ofstream float_sample("float_sample_" + this->participant_id , std::ios::out | std::ios::binary | std::ios::trunc);
        ofstream int_sample("int_sample_" + this->participant_id, std::ios::out | std::ios::binary | std::ios::trunc);
        StreamingRecognizeRequest content_request;
        std::vector<float> chunk(1024);
	while(!this->read_done){
		while(spsc_queue.pop(chunk)){
			this->samples_done += 1024;

			//Write to openSMILE
			while(true){
				smileres_t result = smile_extaudiosource_write_data(this->handle, "externalAudioSource", (void*)&chunk[0], chunk.size()*sizeof(float));
				if(result == SMILE_SUCCESS){
					break;
				}
			}

			//Convert 32f chunk to 16i chunk
			std::vector<int16_t> int_chunk;
			for(float f : chunk){
				int_chunk.push_back((int16_t)(f*32768));
			}
			float_sample.write((char *)&chunk[0], sizeof(float)*chunk.size());
			int_sample.write((char *)&int_chunk[0], sizeof(int16_t)*int_chunk.size());

			//Write to google asr service
			this->speech_handler->send_chunk(int_chunk);

			//Check if asr stream needs to be restarted
			process_real_cpu_clock::time_point stream_current = process_real_cpu_clock::now();
			if(stream_current - this->stream_start >  minutes{4}){
				//Send writes_done and finish reading responses
				this->speech_handler->send_writes_done();
				this->asr_reader_thread.join();
				//End the stream
				this->speech_handler->finish_stream();
				//Sync Opensmile time
				double sync_time = (double)this->samples_done/this->sample_rate;
				this->builder.update_sync_time(sync_time);
				//Create new stream
				this->speech_handler = new SpeechWrapper(false);
				this->speech_handler->start_stream();
				//Restart response reader thread
				this->asr_reader_thread = std::thread(process_responses, this->speech_handler->streamer.get(), &(this->builder));
				this->stream_start = process_real_cpu_clock::now();
			}
		}
	}
	std::cout << "DONE " << std::endl;
	float_sample.close();
	int_sample.close();
	
	this->speech_handler->send_writes_done();
	this->asr_reader_thread.join();
	this->speech_handler->finish_stream();
	
	smile_extaudiosource_set_external_eoi(this->handle, "externalAudioSource");

        this->opensmile_thread.join();
    }

    void on_accept(beast::error_code ec) {
	if (ec) {
            return fail(ec, "accept");
        }
	
	//Initialize openSMILE 
	this->handle = smile_new();
	smile_initialize(this->handle, "conf/is09-13/IS13_ComParE.conf", 0, NULL, 1, 0, 0, 0);
        smile_set_log_callback(this->handle, &log_callback, &(this->builder));
	this->opensmile_thread = std::thread(smile_run, this->handle);	
	
	//Initialize Speech Session
	this->speech_handler = new SpeechWrapper(false);
	this->speech_handler->start_stream();
	this->stream_start = process_real_cpu_clock::now();

	//Initialize asr_reader thread
	this->asr_reader_thread = std::thread(process_responses, speech_handler->streamer.get(), &(this->builder));
 
	this->consumer_thread = std::thread( [this] { this->write_thread(); });
       
	// Read a message
	this->do_read();
    }

    void do_read() {
        // Read a message into our buffer
        this->ws_.async_read(
            this->buffer_,
            beast::bind_front_handler(&WebsocketSession::on_read,
                                      this->shared_from_this()));
    }

    void on_read(beast::error_code ec, size_t bytes_transferred) {
	boost::ignore_unused(bytes_transferred);

        // This indicates that the WebsocketSession was closed
        if (ec == ws::error::closed) {
            this->read_done = true;
            this->consumer_thread.join();
	    return;
        }

        if (ec) {
            fail(ec, "read");
        }

        // Echo the message
        this->ws_.text(this->ws_.got_text());

        float* arr = new float[bytes_transferred / sizeof(float)];
        boost::asio::buffer_copy(boost::asio::buffer(arr, bytes_transferred),
                                 this->buffer_.data(),
                                 bytes_transferred);

        auto chunk =
            std::vector<float>(arr, arr + (bytes_transferred / sizeof(float)));
	while(!this->spsc_queue.push(chunk));

	//Set read_start
	if(!this->read_start){
		this->read_start = true;
	}	
        this->ws_.async_write(
            this->buffer_.data(),
            beast::bind_front_handler(&WebsocketSession::on_write,
                                      this->shared_from_this()));
    }

    void on_write(beast::error_code ec, size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec) {
            return fail(ec, "write");
        }

        // Clear the buffer
        this->buffer_.consume(buffer_.size());

        // Do another read
        this->do_read();
    }
};


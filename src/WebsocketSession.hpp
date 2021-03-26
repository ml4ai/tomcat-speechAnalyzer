#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include "SMILEapi.h"
#include "spsc.h"

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


class WebsocketSession : public enable_shared_from_this<WebsocketSession> {

    ws::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    boost::lockfree::spsc_queue<std::vector<float>,
                                boost::lockfree::capacity<1024>>
        spsc_queue;

    std::atomic<bool> done{false};
    std::thread consumer_thread;
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
    void
    send_chunks(grpc::ClientReaderWriterInterface<StreamingRecognizeRequest,
                                                  StreamingRecognizeResponse>*
                    streamer) {
        StreamingRecognizeRequest request;
        std::vector<float> chunk;
        while (!this->done) {
            while (this->spsc_queue.pop(&chunk)) {
                std::vector<int16_t> intVec(chunk.size());
                for (float f : chunk) {
                    f = f * 32768;
                    if (f < -32768) {
                        f = -32768;
                    }
                    if (f > 32767) {
                        f = 32767;
                    }
                    int16_t i = (int16_t)f;
                    intVec.push_back(i);
                }

		//Write chunks to google speech api
		request.set_audio_content(&intVec[0], intVec.size());
                streamer->Write(request);
            }

        }
	streamer->WritesDone();
    }

    int process_audio(void) {

	//Google asr instance
        auto creds = grpc::GoogleDefaultCredentials();
        auto channel = grpc::CreateChannel("speech.googleapis.com", creds);
        std::unique_ptr<Speech::Stub> speech(Speech::NewStub(channel));

        StreamingRecognizeRequest request;

        // Set up the configuration
        auto* streaming_config = request.mutable_streaming_config();
        auto* config = streaming_config->mutable_config();
        config->set_language_code("en");
        config->set_sample_rate_hertz(48000);
        config->set_encoding(
            google::cloud::speech::v1::RecognitionConfig::LINEAR16);
        streaming_config->set_interim_results(true);
	grpc::ClientContext context;
        auto streamer = speech->StreamingRecognize(&context);

        // Write the first request, containing the config only.
        streaming_config->set_interim_results(true);
        streamer->Write(request);

        std::thread send_chunks_thread(
            &WebsocketSession::send_chunks, this, streamer.get());

        StreamingRecognizeResponse response;

        while (streamer->Read(&response)) {
	    std::cout << "READ" << std::endl;
	    std::cout << response.results_size();
            for (int r = 0; r < response.results_size(); ++r) {
                const auto& result = response.results(r);
                std::cout << "Result stability: " << result.stability()
                          << std::endl;
                for (int a = 0; a < result.alternatives_size(); ++a) {
                    const auto& alternative = result.alternatives(a);
                    std::cout << alternative.confidence() << "\t"
                              << alternative.transcript() << std::endl;
                }
            }
        }
	shared_done = true;
        grpc::Status status = streamer->Finish();
        send_chunks_thread.join();
        if (!status.ok()) {
            // Report the RPC failure.
            std::cerr << status.error_message() << std::endl;
            return -1;
        }

        return 0;
    }

    void on_accept(beast::error_code ec) {
        std::cout << "ACCEPT" << std::endl;
	if (ec) {
            return fail(ec, "accept");
        }

        this->consumer_thread =
            std::thread(&WebsocketSession::process_audio, this);

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
        std::cout << bytes_transferred << std::endl;
	boost::ignore_unused(bytes_transferred);

        // This indicates that the WebsocketSession was closed
        if (ec == ws::error::closed) {
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
        this->spsc_queue.push(chunk);
	shared.push(chunk);

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

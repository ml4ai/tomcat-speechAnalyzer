#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/log/trivial.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <thread>
#include <mutex>

#include "JsonBuilder.h"
#include "OpensmileSession.h"
#include "SpeechWrapper.h"
#include "SpeechWrapperVosk.h"
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include <grpc++/grpc++.h>
#include <range/v3/all.hpp>
#include <smileapi/SMILEapi.h>

#include "arguments.h"
#include "util.h"

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
    boost::lockfree::spsc_queue<std::vector<char>,
                                boost::lockfree::capacity<1024>>
        spsc_queue;

    std::atomic<bool> done{false};
    std::thread consumer_thread;
    std::thread asr_reader_thread;
    JsonBuilder builder;
    SpeechWrapper* speech_handler;
    SpeechWrapperVosk* speech_handler_vosk;
    process_real_cpu_clock::time_point stream_start;
    int samples_done = 0;
    int sample_rate = 48000;
    int samples_per_chunk = 4096;
    bool read_done = false;
    std::string participant_id;

    bool is_float = false;
    bool is_int16 = true;

    bool is_initialized = false;

    int increment = 0;

    // Opensmile wrapper
    OpensmileSession* opensmile_session;

  public:
    // Command line arguments
    static Arguments args;

    // socket port
    static int socket_port;

    // Static current sessions
    static std::vector<std::string> current_sessions;
    static std::mutex *session_mutex;

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

        this->participant_id = params["id"];
        this->sample_rate = stoi(params["sampleRate"]);

	// Check if session already exists
	if(std::find(this->current_sessions.begin(), this->current_sessions.end(), this->participant_id) != this->current_sessions.end()){
		std::cout << "Session already exists" << std::endl;
		return;
	}
	else{
		this->current_sessions.push_back(this->participant_id);
	}

        this->builder.participant_id = params["id"];

        BOOST_LOG_TRIVIAL(info)
            << "Accepted connection: participant_id = " << params["id"]
            << " sample_rate = " << params["sampleRate"];

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
    void initialize() {
        // Initialize openSMILE
        if (!this->args.disable_opensmile) {
            BOOST_LOG_TRIVIAL(info) << "Initializing Opensmile";
            this->socket_port++;
            this->opensmile_session =
                new OpensmileSession(this->socket_port, &this->builder);
        }
        // Initialize Google Speech Session
        if (!this->args.disable_asr_google) {
            BOOST_LOG_TRIVIAL(info) << "Initializing Google Speech  system";
            this->speech_handler = new SpeechWrapper(false, this->sample_rate);
            this->speech_handler->start_stream();
            this->stream_start = process_real_cpu_clock::now();

            // Initialize asr_reader thread
            this->asr_reader_thread =
                std::thread(process_responses,
                            speech_handler->streamer.get(),
                            &(this->builder));
        }
        // Initialize Vosk Speech session
        if (!this->args.disable_asr_vosk) {
            BOOST_LOG_TRIVIAL(info) << "Initializing Vosk Speech  system";
            this->speech_handler_vosk =
                new SpeechWrapperVosk(this->sample_rate, &builder);
            this->speech_handler_vosk->start_stream();
        };
        BOOST_LOG_TRIVIAL(info) << "Starting write_thread";
        this->consumer_thread = std::thread([this] { this->write_thread(); });
        this->is_initialized = true;
    }

    void write_thread() {
        StreamingRecognizeRequest content_request;
        std::vector<char> chunk(8192);
        while (!this->read_done) {
            while (spsc_queue.pop(chunk)) {
                this->samples_done += 4096;
                // Create f32 and i16 chunks
                std::vector<float> float_chunk(chunk.size() / sizeof(float));
                std::vector<int16_t> int_chunk(chunk.size() / sizeof(int16_t));
                if (this->is_float) {
                    memcpy(&float_chunk[0], &chunk[0], chunk.size());
                    int_chunk.clear();
                    for (float f : float_chunk) {
                        int_chunk.push_back((int16_t)(f * 32768));
                    }
                }
                else if (this->is_int16) {
                    memcpy(&int_chunk[0], &chunk[0], chunk.size());
                    float_chunk.clear();
                    for (int i : int_chunk) {
                        float_chunk.push_back((float)(i / 32768.0));
                    }
                }

                // Write to google asr service
                if (!this->args.disable_asr_google) {
                    this->speech_handler->send_chunk(int_chunk);
                    // Check if asr stream needs to be restarted
                    process_real_cpu_clock::time_point stream_current =
                        process_real_cpu_clock::now();
                    if (stream_current - this->stream_start > minutes{4}) {
                        BOOST_LOG_TRIVIAL(info)
                            << "Restarting google speech stream";
                        // Send writes_done and finish reading responses
                        this->speech_handler->send_writes_done();
                        this->asr_reader_thread.join();
                        // End the stream
                        this->speech_handler->finish_stream();
                        // Sync Opensmile time
                        double sync_time =
                            (double)this->samples_done / this->sample_rate;
                        this->builder.update_sync_time(sync_time);
                        // Create new stream
                        this->speech_handler =
                            new SpeechWrapper(false, this->sample_rate);
                        this->speech_handler->start_stream();
                        // Restart response reader thread
                        this->asr_reader_thread =
                            std::thread(process_responses,
                                        this->speech_handler->streamer.get(),
                                        &(this->builder));
                        this->stream_start = process_real_cpu_clock::now();
                    }
                }

                // Write to Vosk asr service
                if (!this->args.disable_asr_vosk) {
                    this->speech_handler_vosk->send_chunk(int_chunk);
                }
                // Write to openSMILE process
                if (!args.disable_opensmile) {
                    this->opensmile_session->send_chunk(float_chunk);
                }
            }
        }
        // Cleanup subsystems
        if (!this->args.disable_asr_google) {
            this->speech_handler->send_writes_done();
            this->asr_reader_thread.join();
            this->speech_handler->finish_stream();
        }

        if (!this->args.disable_asr_vosk) {
            this->speech_handler_vosk->send_writes_done();
            this->speech_handler_vosk->end_stream();
        }
        if (!this->args.disable_opensmile) {
            this->opensmile_session->send_eoi();
        }
    }

    void on_accept(beast::error_code ec) {
        if (ec) {
            return fail(ec, "accept");
        }

        // Read a message
        this->do_read();
    }

    void do_read() {
        if (!this->read_done) {
            // Read a message into our buffer
            this->ws_.async_read(
                this->buffer_,
                beast::bind_front_handler(&WebsocketSession::on_read,
                                          this->shared_from_this()));
        }
        else {
            BOOST_LOG_TRIVIAL(info) << "Joining write thread";
            this->consumer_thread.join();
        }
    }

    void on_read(beast::error_code ec, size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            BOOST_LOG_TRIVIAL(info) << "Connection has been closed";
            this->read_done = true;
	    this->session_mutex->lock();
	    this->current_sessions.erase(std::remove(this->current_sessions.begin(), this->current_sessions.end(), this->participant_id), this->current_sessions.end());	
            this->session_mutex->unlock();
	}

        if (bytes_transferred != 0) {
            // Echo the message
            this->ws_.text(this->ws_.got_text());
            char* arr = new char[bytes_transferred];
            boost::asio::buffer_copy(
                boost::asio::buffer(arr, bytes_transferred),
                this->buffer_.data(),
                bytes_transferred);

            auto chunk = std::vector<char>(arr, arr + bytes_transferred);
            delete[] arr;

            // Push the chunk to the queue for the write_thread
            while (!this->spsc_queue.push(chunk)) {
            }
            // Send chunk for raw audio message
            string id = to_string(this->increment);
            this->increment++;
            if (!args.disable_chunk_publishing) {
                this->builder.process_audio_chunk_message(chunk, id);
            }
            if (!args.disable_chunk_metadata_publishing) {
                this->builder.process_audio_chunk_metadata_message(chunk, id);
            }

            // Clear the buffer
            this->buffer_.consume(buffer_.size());

            // Initialize write_thread
            if (!this->is_initialized) {
                this->initialize();
                BOOST_LOG_TRIVIAL(info) << "Begin reading chunks";
            }
        }
        // Do another read
        this->do_read();
    }
};

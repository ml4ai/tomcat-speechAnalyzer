#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/split.hpp>
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
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include "JsonBuilder.h"
#include "SpeechWrapper.h"
#include "SpeechWrapperVosk.h"
#include "Mosquitto.h"

#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include <grpc++/grpc++.h>

#include "base64.h"
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
  public:
    // Websocket base
    ws::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    boost::lockfree::spsc_queue<std::vector<char>,
                                boost::lockfree::capacity<8096>>
        spsc_queue;

    // Static objects
    static Arguments args;

    // Session objects
    JsonBuilder* builder;
    SpeechWrapper* speech_handler;
    SpeechWrapperVosk* speech_handler_vosk;
    
    // MQTT client
    Mosquitto mqtt_client;

    // Threads
    std::thread consumer_thread;
    std::thread asr_reader_thread;

    // Default sample data
    int increment = 0;
    int samples_done = 0;
    int sample_rate = 48000;
    int samples_per_chunk = 4096;

    // Flags
    bool is_float = false;
    bool is_int16 = true;
    bool is_initialized = false;
    bool read_done = false;

    // Other
    std::atomic<bool> done{false};
    process_real_cpu_clock::time_point stream_start;
    std::string participant_id;

    // Take ownership of the socket
    explicit WebsocketSession(tcp::socket&& socket) : ws_(move(socket)) {}

    // Start the asynchronous accept operation
    template <class Body, class Allocator>
    void do_accept(http::request<Body, http::basic_fields<Allocator>> request) {
        using boost::split, boost::is_any_of, boost::erase_head;

        std::map<std::string, std::string> params;

        std::string request_target = std::string(request.target());
        erase_head(request_target, 2);

        std::vector<std::string> param_strings;
        split(param_strings, request_target, is_any_of("&"));

        for (auto param_string : param_strings) {
            std::vector<std::string> key_value_pair;
            split(key_value_pair, param_string, is_any_of("="));
            params[key_value_pair.at(0)] = key_value_pair.at(1);
        }

        this->participant_id = params["id"];
        this->sample_rate = stoi(params["sampleRate"]);

        this->participant_id = params["id"];

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
        // Initialize JsonBuilder
        BOOST_LOG_TRIVIAL(info) << this->participant_id << ": "
                                << "Initializing JsonBuilder";
        this->builder = new JsonBuilder();
        this->builder->participant_id = this->participant_id;
        this->builder->Initialize();

        // TODO: Initialize data for publishing chunks to message bus
        if (!this->args.disable_opensmile) {
		this->mqtt_client.connect(this->args.mqtt_host_internal, this->args.mqtt_port_internal, 1000, 1000, 1000);
		this->mqtt_client.set_max_seconds_without_messages(10000);
        }
        // Initialize Google Speech Session
        if (!this->args.disable_asr_google) {
            BOOST_LOG_TRIVIAL(info) << this->participant_id << ": "
                                    << "Initializing Google Speech  system";
            this->speech_handler = new SpeechWrapper(false, this->sample_rate);
            this->speech_handler->start_stream();
            this->stream_start = process_real_cpu_clock::now();

            // Initialize asr_reader thread
            this->asr_reader_thread =
                std::thread(process_responses,
                            speech_handler->streamer.get(),
                            this->builder);
        }
        // Initialize Vosk Speech session
        if (!this->args.disable_asr_vosk) {
            BOOST_LOG_TRIVIAL(info) << this->participant_id << ": "
                                    << "Initializing Vosk Speech  system";
            this->speech_handler_vosk =
                new SpeechWrapperVosk(this->sample_rate, this->builder);
            this->speech_handler_vosk->start_stream();
        };

        BOOST_LOG_TRIVIAL(info) << this->participant_id << ": "
                                << "Starting write_thread";
        this->consumer_thread = std::thread([this] { this->write_thread(); });
        this->is_initialized = true;
    }

    void shutdown() {
        // Cleanup subsystems
        if (!this->args.disable_asr_google) {
            BOOST_LOG_TRIVIAL(info) << this->participant_id << ": "
                                    << "Closing Google speech session";
            this->speech_handler->send_writes_done();
            this->asr_reader_thread.join();
            this->speech_handler->finish_stream();
        }

        if (!this->args.disable_asr_vosk) {
            BOOST_LOG_TRIVIAL(info) << this->participant_id << ": "
                                    << "Closing Vosk speech session";
            this->speech_handler_vosk->send_writes_done();
            this->speech_handler_vosk->end_stream();
        }
        
	//TODO: Free publishing chunks to message bus data
	if (!this->args.disable_opensmile) {
        	this->mqtt_client.close();
	}

        this->builder->Shutdown();
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
                        this->builder->update_sync_time(sync_time);
                        // Create new stream
                        this->speech_handler =
                            new SpeechWrapper(false, this->sample_rate);
                        this->speech_handler->start_stream();
                        // Restart response reader thread
                        this->asr_reader_thread =
                            std::thread(process_responses,
                                        this->speech_handler->streamer.get(),
                                        this->builder);
                        this->stream_start = process_real_cpu_clock::now();
                    }
                }

                // Write to Vosk asr service
                if (!this->args.disable_asr_vosk) {
                    this->speech_handler_vosk->send_chunk(int_chunk);
                }
                // TODO: Publish chunks over message buss
                if (!args.disable_opensmile) {
			// Encode audio chunk
			int encoded_data_length = Base64encode_len(chunk.size());
			char output[encoded_data_length];
			Base64encode(output, &chunk[0], chunk.size());
			string encoded(output);
	
			// Publish to message bus	
			nlohmann::json message;
			message["chunk"]=encoded;
			message["increment"]=this->increment;
			this->mqtt_client.publish(this->participant_id, message.dump());	
                }
		this->increment++;
            }
        }
    }

    void on_accept(beast::error_code ec) {
        if (ec) {
            BOOST_LOG_TRIVIAL(info) << "FAIL ";
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
            BOOST_LOG_TRIVIAL(info) << this->participant_id << ": "
                                    << "Joining write thread";
            this->consumer_thread.join();
            BOOST_LOG_TRIVIAL(info) << this->participant_id << ": "
                                    << "Shutting down subsystems";
            this->shutdown();
        }
    }

    void on_read(beast::error_code ec, size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            BOOST_LOG_TRIVIAL(info) << this->participant_id << ": "
                                    << "Connection has been closed";
            this->read_done = true;
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
            int count = 0;
            while (!this->spsc_queue.push(chunk)) {
                count++;
            }
            if (count > 0) {
                BOOST_LOG_TRIVIAL(info) << count;
            }

            // Clear the buffer
            this->buffer_.consume(buffer_.size());

            // Initialize write_thread
            if (!this->is_initialized) {
                this->initialize();
                BOOST_LOG_TRIVIAL(info) << this->participant_id << ": "
                                        << "Begin reading chunks";
            }
        }
        // Do another read
        this->do_read();
    }
};

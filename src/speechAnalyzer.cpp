#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/chrono.hpp>
#include <boost/program_options.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <grpc++/grpc++.h>
#include <regex>
#include <string>
#include <thread>
#include <cerrno>  //errno
#include <cstring> //strerror
#include <iostream>
#include <vector>
#include <fstream>

#include "SMILEapi.h"
#include "JsonBuilder.hpp"
#include "SpeechWrapper.cpp"
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include "spsc.h"
#include "Mosquitto.h"
#include "helper.h"

#include "util.hpp"
#include "WebsocketSession.hpp"
#include "HTTPSession.hpp"
#include "Listener.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace ws = beast::websocket;
namespace asio = boost::asio;

using tcp = boost::asio::ip::tcp;

using google::cloud::speech::v1::Speech;
using google::cloud::speech::v1::StreamingRecognizeRequest;
using google::cloud::speech::v1::StreamingRecognizeResponse;
using google::cloud::speech::v1::RecognitionConfig;
using google::cloud::speech::v1::WordInfo;
using namespace boost::program_options;
using namespace boost::chrono;
void read_chunks_stdin();
void read_chunks_websocket();
void write_thread();
boost::lockfree::spsc_queue<std::vector<float>, boost::lockfree::capacity<1024>> shared;
bool read_done = false;
bool write_start = false;


int main(int argc, char * argv[]) {
    std::string mode;
    //Handle options
    try{
	options_description desc{"Options"};
	desc.add_options()
	  ("help,h", "Help screen")
	  ("mode", value<std::string>()->default_value("stdin"), "Where to read audio chunks from");
	  ("sampleRate", value<int>()->default_value(48000), "The sample rate of the input audio");
	variables_map vm;
	store(parse_command_line(argc, argv, desc), vm);

	if(vm.count("mode")){
		mode = vm["mode"].as<std::string>();
//		std::cout << "Starting speechAnalyzer in " <<  vm["mode"].as<std::string>() << " mode" <<  std::endl;
	}
    }
    catch(const error &ex){
	std::cout << "Error parsing arguments" << std::endl;
	return -1;
    }


    if(mode.compare("stdin") == 0){
	    std::thread thread_object(read_chunks_stdin);
	    std::thread thread_object2(write_thread);
	    thread_object.join();
	    thread_object2.join();
    }
    else if(mode.compare("websocket") == 0){
	    std::thread thread_object(read_chunks_websocket);
	    //std::thread thread_object2(write_thread);
	    thread_object.join();
	    //thread_object2.join();
    }
    else{
	std::cout << "Unknown mode" << std::endl;
    } 

    return 0;
}

void read_chunks_stdin(){
	while(!write_start);
    	std::freopen(nullptr, "rb", stdin); // reopen stdin in binary mode

	std::vector<float> chunk(1024);
        std::size_t length;
        while ((length = std::fread(&chunk[0], sizeof(float), 1024, stdin)) > 0) {
		while(!shared.push(chunk)); // If queue is full will keep trying until avaliable space
	}

	read_done = true;
}

void read_chunks_websocket(){
	std::cout << "Starting Websocket Server" << std::endl;
	auto const address = asio::ip::make_address("0.0.0.0");
	auto const port = static_cast<unsigned short>(8888);
	auto const doc_root = make_shared<std::string>(".");
	auto const n_threads = 1;

	asio::io_context ioc{n_threads};

	auto listener = make_shared<Listener>(ioc, tcp::endpoint{address, port}, doc_root);
	listener->run();

	asio::signal_set signals(ioc, SIGINT, SIGTERM);
	signals.async_wait([&](beast::error_code const&, int){
		ioc.stop();
	});

	std::vector<std::thread> threads;
	threads.reserve(n_threads-1);
	for(auto i=n_threads; i>0; --i){
		threads.emplace_back([&ioc] { ioc.run(); });
	}
	ioc.run();

	for(auto& thread : threads){
		thread.join();
	}
}

void write_thread(){
	int sample_rate = 44100;
	int samples_done = 0;
	
	//JsonBuilder object which will be passed to openSMILE log callback
	JsonBuilder builder;
	
	//Initialize and start opensmile
	smileobj_t* handle;

	//Start speech streamer
	SpeechWrapper *speech_handler = new SpeechWrapper(false);
	speech_handler->start_stream();
	process_real_cpu_clock::time_point stream_start = process_real_cpu_clock::now(); // Need to know starting time to restart steram 
	//Initialize response reader thread
	std::thread asr_reader_thread(process_responses, speech_handler->streamer.get(), &builder);
	

	handle = smile_new();
	smile_initialize(handle, "conf/is09-13/IS13_ComParE.conf", 0, NULL, 1, 0, 0, 0);
	smile_set_log_callback(handle, &log_callback, &builder);

	//Initialize opensmile thread
	std::thread opensmile_thread(smile_run, handle);
	
	ofstream float_sample("float_sample", std::ios::out | std::ios::binary | std::ios::trunc);
        ofstream int_sample("int_sample", std::ios::out | std::ios::binary | std::ios::trunc);	
	StreamingRecognizeRequest content_request;
	std::vector<float> chunk(1024);
	write_start = true;
	while(!read_done){
		while(shared.pop(chunk)){
			samples_done += 1024;
			//Write to opensmile
			while(true){
				smileres_t result = smile_extaudiosource_write_data(handle, "externalAudioSource", (void*)&chunk[0], chunk.size()*sizeof(float));
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
			speech_handler->send_chunk(int_chunk);
			
			//Check if asr stream needs to be restarted
			process_real_cpu_clock::time_point stream_current = process_real_cpu_clock::now();
			if(stream_current - stream_start >  seconds{500}){
				//Send writes_done and finish reading responses
				speech_handler->send_writes_done();
				asr_reader_thread.join();
				//End the stream
				speech_handler->finish_stream();
				//Sync Opensmile time
				double sync_time = (double)samples_done/sample_rate;
				builder.update_sync_time(sync_time);
				//Create new stream
				speech_handler = new SpeechWrapper(false);
				speech_handler->start_stream();
				//Restart response reader thread
				asr_reader_thread = std::thread(process_responses, speech_handler->streamer.get(), &builder);
				stream_start = process_real_cpu_clock::now();
			}
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


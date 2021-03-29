#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/program_options.hpp>
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
#include "JsonBuilder.cpp"
#include "parse_arguments.h" 
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include "spsc.h"

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

using namespace boost::program_options;
const size_t MAX_NUM_SAMPLES = 512;

void read_chunks_stdin();
void read_chunks_websocket();
void write_thread();
void log_callback(smileobj_t*, smilelogmsg_t, void*);



void read_responses(grpc::ClientReaderWriterInterface<StreamingRecognizeRequest,
						      StreamingRecognizeResponse>*);
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
	    std::thread thread_object2(write_thread);
	    thread_object.join();
	    thread_object2.join();
    }
    else{
	std::cout << "Unknown mode" << std::endl;
    } 

    //grpc::Status status = streamer->Finish();
    /*if (!status.ok()) {
        // Report the RPC failure.
        std::cerr << status.error_message() << std::endl;
	return -1;
    }*/

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
	auto const address = asio::ip::make_address("127.0.0.1");
	auto const port = static_cast<unsigned short>(8888);
	auto const doc_root = make_shared<std::string>(".");
	auto const n_threads = 4;

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

void log_callback(smileobj_t* smileobj, smilelogmsg_t message, void* param){

	
	JsonBuilder *builder = (JsonBuilder*)(param);
	builder->process_message(message);
}

void write_thread(){
	//JsonBuilder object which will be passed to openSMILE log callback
	JsonBuilder builder;

	//Initialize and start opensmile
	smileobj_t* handle;

	//Create a Speech Stub connected to speech service
	auto creds = grpc::GoogleDefaultCredentials();
	auto channel = grpc::CreateChannel("speech.googleapis.com", creds);
	std::unique_ptr<Speech::Stub> speech(Speech::NewStub(channel));
	//Handle streaming config
	StreamingRecognizeRequest request;
	auto* streaming_config = request.mutable_streaming_config();
	//Begin a stream
	grpc::ClientContext context;
	auto streamer = speech->StreamingRecognize(&context);
	//Write first request with config
	auto mutable_config = streaming_config->mutable_config();
	mutable_config->set_language_code("en");
	mutable_config->set_sample_rate_hertz(44100);
	mutable_config->set_encoding(RecognitionConfig::LINEAR16);
	mutable_config->set_max_alternatives(5);
	streaming_config->set_interim_results(true);
	streamer->Write(request);

	handle = smile_new();
	smile_initialize(handle, "conf/is09-13/IS13_ComParE.conf", 0, NULL, 1, 0, 0, 0);
	smile_set_log_callback(handle, &log_callback, &builder);

	//Initialize opensmile thread
	std::thread opensmile_thread(smile_run, handle);
	
	ofstream float_sample("float_sample", std::ios::out | std::ios::binary | std::ios::app);
        ofstream int_sample("int_sample", std::ios::out | std::ios::binary | std::ios::app);	
	StreamingRecognizeRequest content_request;
	std::vector<float> chunk(1024);
	write_start = true;
	while(!read_done){
		while(shared.pop(chunk)){
			//Write to opensmile
			/*while(true){
				smileres_t result = smile_extaudiosource_write_data(handle, "externalAudioSource", (void*)&chunk[0], chunk.size()*sizeof(float));
				if(result == SMILE_SUCCESS){
					break;
				}
			}*/

			//Convert 32f chunk to 16i chunk
			std::vector<int16_t> int_chunk;
			for(float f : chunk){
				int_chunk.push_back((int16_t)(f*32768));
			}
			float_sample.write((char *)&chunk[0], sizeof(float)*chunk.size());
			int_sample.write((char *)&int_chunk[0], sizeof(int16_t)*int_chunk.size());
			
			//Write to google asr service
			content_request.set_audio_content(&int_chunk[0], int_chunk.size()*sizeof(int16_t));
			streamer->Write(content_request);
		}
	}
	float_sample.close();
        int_sample.close();	
	streamer->WritesDone();
	smile_extaudiosource_set_external_eoi(handle, "externalAudioSource");

	opensmile_thread.join();

	StreamingRecognizeResponse response;
	while (streamer->Read(&response)) {  // Returns false when no more to read.
	// Dump the transcript of all the results.
		
	    std::string timestamp = boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time()) + "Z";
	    nlohmann::json j;
	    j["header"]["timestamp"] = timestamp;
	    j["header"]["message_type"] = "observation";
	    j["header"]["version"] = 0.1;
	    j["msg"]["timestamp"] = timestamp;
	    j["msg"]["experiment_id"] = nullptr;
	    j["msg"]["trial_id"] = nullptr;
	    j["msg"]["version"] = "0.1";
	    j["msg"]["source"] = "tomcat_speech_analyzer";
	    j["msg"]["sub_type"] = "speech_analysis";
	    j["data"]["text"] = response.results(0).alternatives(0).transcript();
	    j["data"]["is_final"] = response.results(0).is_final();
	    j["data"]["asr_system"] = "google";
	    j["data"]["participant_id"] = nullptr;
	    
	    std::vector<nlohmann::json> alternatives;
	    auto result = response.results(0);
	    for(int i=0; i<result.alternatives_size(); i++){
		auto alternative = result.alternatives(i); 
		alternatives.push_back(nlohmann::json::object({{"text", alternative.transcript()},{"confidence", alternative.confidence()}}));
	    }
	    j["data"]["alternatives"] = alternatives;
	    std::cout << j << std::endl; 
	}
}

void read_responses(grpc::ClientReaderWriterInterface<StreamingRecognizeRequest,
						      StreamingRecognizeResponse>* streamer){
    
    StreamingRecognizeResponse response;
    while (streamer->Read(&response)) {  // Returns false when no more to read.
    // Dump the transcript of all the results.
		
	    std::string timestamp = boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time()) + "Z";
	    nlohmann::json j;
	    j["header"]["timestamp"] = timestamp;
	    j["header"]["message_type"] = "observation";
	    j["header"]["version"] = 0.1;
	    j["msg"]["timestamp"] = timestamp;
	    j["msg"]["experiment_id"] = nullptr;
	    j["msg"]["trial_id"] = nullptr;
	    j["msg"]["version"] = "0.1";
	    j["msg"]["source"] = "tomcat_speech_analyzer";
	    j["msg"]["sub_type"] = "speech_analysis";
	    j["data"]["text"] = response.results(0).alternatives(0).transcript();
	    j["data"]["is_final"] = response.results(0).is_final();
	    j["data"]["asr_system"] = "google";
            j["data"]["participant_id"] = nullptr;
	    
	    std::vector<nlohmann::json> alternatives;
	    auto result = response.results(0);
	    for(int i=0; i<result.alternatives_size(); i++){
		auto alternative = result.alternatives(i); 
		alternatives.push_back(nlohmann::json::object({{"text", alternative.transcript()},{"confidence", alternative.confidence()}}));
	    }
	    j["data"]["alternatives"] = alternatives;
            std::cout << j << std::endl; 
    }
}

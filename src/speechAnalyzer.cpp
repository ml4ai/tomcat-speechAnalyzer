#include <boost/date_time/posix_time/posix_time.hpp>
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

const size_t MAX_NUM_SAMPLES = 512;

void read_chunks_stdin(smileobj_t*, grpc::ClientReaderWriterInterface<StreamingRecognizeRequest,StreamingRecognizeResponse>*);
void read_chunks_websocket(smileobj_t*, grpc::ClientReaderWriterInterface<StreamingRecognizeRequest, StreamingRecognizeResponse>*);
void log_callback(smileobj_t*, smilelogmsg_t, void*);



void read_responses(grpc::ClientReaderWriterInterface<StreamingRecognizeRequest,
						      StreamingRecognizeResponse>*);
int main(int argc, char * argv[]) {

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
    streaming_config->mutable_config()->set_language_code("en");
    streaming_config->mutable_config()->set_sample_rate_hertz(16000);
    streaming_config->mutable_config()->set_encoding(RecognitionConfig::LINEAR16);
    streaming_config->mutable_config()->set_max_alternatives(5);
    streaming_config->set_interim_results(true);
    streamer->Write(request);

    handle = smile_new();
    smile_initialize(handle, "conf/is09-13/IS13_ComParE.conf", 0, NULL, 1, 0, 0, 0);
    smile_set_log_callback(handle, &log_callback, &builder);
    std::thread thread_object(smile_run, handle);
    std::thread thread_object2(read_chunks_stdin, handle, streamer.get());
    std::thread thread_object3(read_responses, streamer.get());
    thread_object.join();
    thread_object2.join();
    thread_object3.join();

    grpc::Status status = streamer->Finish();
    if (!status.ok()) {
        // Report the RPC failure.
        std::cerr << status.error_message() << std::endl;
	return -1;
    }

    return 0;
}

void read_chunks_stdin(smileobj_t* handle, grpc::ClientReaderWriterInterface<StreamingRecognizeRequest,
					                                    StreamingRecognizeResponse>* streamer){
    StreamingRecognizeRequest request;
    try {
        std::freopen(nullptr, "rb", stdin); // reopen stdin in binary mode

        if (std::ferror(stdin)) {
            throw(std::runtime_error(std::strerror(errno)));
        }

	char chunk[MAX_NUM_SAMPLES*4];
        std::size_t length;
        while ((length = std::fread(chunk, 1, MAX_NUM_SAMPLES*4, stdin)) > 0) {
		//Write the chunk to cExternalAudioSource
		while(true){
			smileres_t result = smile_extaudiosource_write_data(handle, "externalAudioSource", (void*)chunk, length);
			if(result == SMILE_SUCCESS){
				break;
			}
		}
		//Send the chunk to google for asr
		request.set_audio_content(&chunk[0], length);
		streamer->Write(request);

	}
	streamer->WritesDone();
    }
    catch (std::exception const& e) {
        std::cerr << e.what() << std::endl;
    }
    
    smile_extaudiosource_set_external_eoi(handle, "externalAudioSource");

}

void read_chunks_websocket(smileobj_t* handle, grpc::ClientReaderWriterInterface<StreamingRecognizeRequest,
					                                    StreamingRecognizeResponse>* streamer){
	auto const address = asio::ip::make_address("127.0.0.1");
	auto const port = static_cast<unsigned short>(8080);
	auto const doc_root = make_shared<std::string>(".");
	auto const n_threads = 1;

	asio::io_context ioc{n_threads};

	auto listener = make_shared<Listener>(ioc, tcp::endpoint{address, port}, doc_root);
	listener->run();

	asio::signal_set signals(ioc, SIGINT, SIGTERM);
	signals.async_wait([&](beast::error_code const&, int){
		ioc.stop();
	});

	ioc.run();
}

void log_callback(smileobj_t* smileobj, smilelogmsg_t message, void* param){

	
	JsonBuilder *builder = (JsonBuilder*)(param);
	builder->process_message(message);
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
	    /*for (int r = 0; r < response.results_size(); ++r) {
	      const auto& result = response.results(r);
	      std::cout << "Result stability: " << result.stability() << std::endl;
	      for (int a = 0; a < result.alternatives_size(); ++a) {
		const auto& alternative = result.alternatives(a);
		std::cout << alternative.confidence() << "\t"
			  << alternative.transcript() << std::endl;
	      }
	    }*/
    }
}

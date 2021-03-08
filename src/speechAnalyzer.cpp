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

using google::cloud::speech::v1::Speech;
using google::cloud::speech::v1::StreamingRecognizeRequest;
using google::cloud::speech::v1::StreamingRecognizeResponse;
using google::cloud::speech::v1::RecognitionConfig;

const size_t MAX_NUM_SAMPLES = 512;

void parse_audio_data(smileobj_t*, grpc::ClientReaderWriterInterface<StreamingRecognizeRequest,
                                                                     StreamingRecognizeResponse>*);
void log_callback(smileobj_t*, smilelogmsg_t, void*);

		
int main() {

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
    streaming_config->set_interim_results(true);
    streamer->Write(request);

    handle = smile_new();
    smile_initialize(handle, "conf/is09-13/IS13_ComParE.conf", 0, NULL, 1, 0, 0, 0);
    smile_set_log_callback(handle, &log_callback, &builder);
    std::thread thread_object(smile_run, handle);
    std::thread thread_object2(parse_audio_data, handle, streamer.get());

    thread_object.join();
    thread_object2.join();

    StreamingRecognizeResponse response;
    while (streamer->Read(&response)) {  // Returns false when no more to read.
    // Dump the transcript of all the results.
	    for (int r = 0; r < response.results_size(); ++r) {
	      const auto& result = response.results(r);
	      std::cout << "Result stability: " << result.stability() << std::endl;
	      for (int a = 0; a < result.alternatives_size(); ++a) {
		const auto& alternative = result.alternatives(a);
		std::cout << alternative.confidence() << "\t"
			  << alternative.transcript() << std::endl;
	      }
	    }
    }
    grpc::Status status = streamer->Finish();
    if (!status.ok()) {
        // Report the RPC failure.
        std::cerr << status.error_message() << std::endl;
	return -1;
    }

    return 0;
}

void parse_audio_data(smileobj_t* handle, grpc::ClientReaderWriterInterface<StreamingRecognizeRequest,
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

void log_callback(smileobj_t* smileobj, smilelogmsg_t message, void* param){

	
	JsonBuilder *builder = (JsonBuilder*)(param);
	builder->process_message(message);
}

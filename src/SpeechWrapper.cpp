#include <grpc++/grpc++.h>
#include <boost/chrono.hpp>
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
using google::cloud::speech::v1::Speech;
using google::cloud::speech::v1::StreamingRecognizeRequest;
using google::cloud::speech::v1::StreamingRecognizeResponse;
using google::cloud::speech::v1::RecognitionConfig;
using namespace boost::chrono;
class SpeechWrapper{
	
	public:
	SpeechWrapper(){
	
	}
	~SpeechWrapper(){

	}
	void start_stream(){
		initialize_stream();
		send_config();
	}
	void finish_stream(){
		streamer->WritesDone();
		status = streamer->Finish();
		if (!status.ok()) {
                // Report the RPC failure.
	        	std::cerr << status.error_message() << std::endl;
		}
	}
	void reset_stream(){

	}
	void send_chunk(std::vector<int16_t> int_chunk){
		StreamingRecognizeRequest content_request;
		content_request.set_audio_content(&int_chunk[0], int_chunk.size()*sizeof(int16_t));
		streamer->Write(content_request);
	}
		
	//Speech session variables
	grpc::ClientContext context;
	std::unique_ptr<grpc::ClientReaderWriterInterface<StreamingRecognizeRequest, StreamingRecognizeResponse>> streamer;
	grpc::Status status;
	

	private:
	//Time variables
	process_real_cpu_clock::time_point stream_start;

	void initialize_stream(){
		//Create speech stub
		auto creds = grpc::GoogleDefaultCredentials();
		auto channel = grpc::CreateChannel("speech.googleapis.com", creds);
		std::unique_ptr<Speech::Stub> speech(Speech::NewStub(channel));

		//Start stream
		streamer = speech->StreamingRecognize(&context);		

		//Set start time
		stream_start = process_real_cpu_clock::now();
	}
	void send_config(){
		//Write first request with config
		StreamingRecognizeRequest config_request;
		auto *streaming_config = config_request.mutable_streaming_config();

		auto mutable_config = streaming_config->mutable_config();
		mutable_config->set_language_code("en");
		mutable_config->set_sample_rate_hertz(44100);
		mutable_config->set_encoding(RecognitionConfig::LINEAR16);
		mutable_config->set_max_alternatives(5);
		mutable_config->set_enable_word_time_offsets(true);
		streaming_config->set_interim_results(true);
		streamer->Write(config_request);
	}
};

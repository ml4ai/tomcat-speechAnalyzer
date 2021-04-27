#include <grpc++/grpc++.h>
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
using google::cloud::speech::v1::Speech;
using google::cloud::speech::v1::StreamingRecognizeRequest;
using google::cloud::speech::v1::StreamingRecognizeResponse;
using google::cloud::speech::v1::RecognitionConfig;
class SpeechWrapper{
	
	public:
	SpeechWrapper(bool dummy_read){
		this->dummy_read = dummy_read;
	}
	~SpeechWrapper(){

	}
	void start_stream(){
		initialize_stream();
		send_config();
	}
	void finish_stream(){
		//Read responses if dummy_read
		if(dummy_read){	
			StreamingRecognizeResponse response;
    			while (streamer->Read(&response));
		}	
		
		//Finish stream
		status = streamer->Finish();
		if (!status.ok()) {
                	// Report the RPC failure.
	        	std::cerr << status.error_message() << std::endl;
		}
	}
	void send_chunk(std::vector<int16_t> int_chunk){
		StreamingRecognizeRequest content_request;
		content_request.set_audio_content(&int_chunk[0], int_chunk.size()*sizeof(int16_t));
		streamer->Write(content_request);
	}
	void send_writes_done(){
		streamer->WritesDone();
	}	
	//Speech session variables
	std::unique_ptr<grpc::ClientReaderWriterInterface<StreamingRecognizeRequest, StreamingRecognizeResponse>> streamer;
	grpc::Status status;
	grpc::ClientContext context;

	private:
	bool dummy_read = false;
	bool finished = false;
	void initialize_stream(){
		//Create speech stub
		auto creds = grpc::GoogleDefaultCredentials();
		auto channel = grpc::CreateChannel("speech.googleapis.com", creds);
		std::unique_ptr<Speech::Stub> speech(Speech::NewStub(channel));
		
		//Start stream
		streamer = speech->StreamingRecognize(&context);		
	}
	void send_config(){
		//Write first request with config
		StreamingRecognizeRequest config_request;
		auto *streaming_config = config_request.mutable_streaming_config();

		auto mutable_config = streaming_config->mutable_config();
		mutable_config->set_language_code("en");
		mutable_config->set_sample_rate_hertz(48000);
		mutable_config->set_encoding(RecognitionConfig::LINEAR16);
		mutable_config->set_max_alternatives(5);
		mutable_config->set_enable_word_time_offsets(true);
		streaming_config->set_interim_results(true);
		streamer->Write(config_request);
	}
};

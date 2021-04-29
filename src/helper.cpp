#include "helper.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
void log_callback(smileobj_t* smileobj, smilelogmsg_t message, void* param){

        JsonBuilder *builder = (JsonBuilder*)(param);
        builder->process_message(message);
}

void process_responses(grpc::ClientReaderWriterInterface<StreamingRecognizeRequest, StreamingRecognizeResponse>* streamer, JsonBuilder *builder){
    StreamingRecognizeResponse response;
    while (streamer->Read(&response)) {  // Returns false when no more to read.
        std::cout << "Response" << std::endl;
	//Generate UUID4 for messages
        std::string id = boost::uuids::to_string(boost::uuids::random_generator()());
        //Process messages
        builder->process_asr_message(response, id);
        builder->process_alignment_message(response, id);
    }
}

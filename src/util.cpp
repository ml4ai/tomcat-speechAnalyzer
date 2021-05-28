#include <boost/beast/core.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <iostream>

#include "util.h"

namespace beast = boost::beast; // from <boost/beast.hpp>

using google::cloud::speech::v1::StreamingRecognizeRequest;
using google::cloud::speech::v1::StreamingRecognizeResponse;
using namespace std;

// Report a failure
void fail(beast::error_code ec, char const* what) {
    cerr << what << ": " << ec.message() << "\n";
}

// Callback function for openSMILE messages
void log_callback(smileobj_t* smileobj, smilelogmsg_t message, void* param) {

    JsonBuilder* builder = (JsonBuilder*)(param);
    builder->process_message(message);
}

// Process responses from an asr stream
void process_responses(
    grpc::ClientReaderWriterInterface<StreamingRecognizeRequest,
                                      StreamingRecognizeResponse>* streamer,
    JsonBuilder* builder) {
    StreamingRecognizeResponse response;
    while (streamer->Read(&response)) { // Returns false when no more to read.
                                        // Generate UUID4 for messages
        string id = boost::uuids::to_string(boost::uuids::random_generator()());
        // Process messages
        builder->process_asr_message(response, id);
	builder->process_alignment_message(response, id);
    }
}

#include <boost/beast/core.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <thread>

#include <boost/log/trivial.hpp>

#include "GlobalMosquittoListener.h"
#include "util.h"

namespace beast = boost::beast; // from <boost/beast.hpp>

using google::cloud::speech::v1::StreamingRecognizeRequest;
using google::cloud::speech::v1::StreamingRecognizeResponse;
using namespace std;

// Report a failure
void fail(beast::error_code ec, char const* what) {
    BOOST_LOG_TRIVIAL(error) << what << ": " << ec.message() << "\n";
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
        // thread{[=] { builder->process_asr_message(response, id); }}.detach();
        builder->process_asr_message(response, id);
    }
}

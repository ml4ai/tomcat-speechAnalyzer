#include <boost/beast/core.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>

#include <iostream>

#include "GlobalMosquittoListener.h"
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
    	int socket = *((int*)(param));
	int len = -1;
    	int num_bytes = 256;

	// Copy string for sending
	string temp(message.text);
	char text[num_bytes];
	strcpy(text, temp.c_str());

	OPENSMILE_MUTEX.lock();
	// Send message
	len = send(socket, &text, num_bytes, 0);
	OPENSMILE_MUTEX.unlock();
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
    }
}

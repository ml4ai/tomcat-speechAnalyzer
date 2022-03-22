#pragma once
#include "JsonBuilder.h"
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include <grpc++/grpc++.h>
#include <smileapi/SMILEapi.h>

#include <boost/beast/core.hpp>

#include "util.h"
using namespace std;
namespace beast = boost::beast; // from <boost/beast.hpp>

using google::cloud::speech::v1::StreamingRecognizeRequest;
using google::cloud::speech::v1::StreamingRecognizeResponse;

// Report a failure
void fail(beast::error_code ec, char const* what);

// Function for processing asr responses
void process_responses(
    grpc::ClientReaderWriterInterface<StreamingRecognizeRequest,
                                      StreamingRecognizeResponse>*,
    JsonBuilder* builder);

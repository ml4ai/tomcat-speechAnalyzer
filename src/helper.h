#pragma once
#include "JsonBuilder.hpp"
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include <grpc++/grpc++.h>
#include <smileapi/SMILEapi.h>

using google::cloud::speech::v1::StreamingRecognizeRequest;
using google::cloud::speech::v1::StreamingRecognizeResponse;

// Callback function for openSMILE log messages
void log_callback(smileobj_t*, smilelogmsg_t, void*);

// Function for processing asr responses
void process_responses(
    grpc::ClientReaderWriterInterface<StreamingRecognizeRequest,
                                      StreamingRecognizeResponse>*,
    JsonBuilder* builder);

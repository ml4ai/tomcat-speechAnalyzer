#include <smileapi/SMILEapi.h>
#include <grpc++/grpc++.h>
#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include "JsonBuilder.hpp"

using google::cloud::speech::v1::StreamingRecognizeRequest;
using google::cloud::speech::v1::StreamingRecognizeResponse;

//Callback function for openSMILE log messages
void log_callback(smileobj_t*, smilelogmsg_t, void*);

//Function for processing asr responses
void process_responses(grpc::ClientReaderWriterInterface<StreamingRecognizeRequest,
                                                      StreamingRecognizeResponse>*, JsonBuilder* builder);

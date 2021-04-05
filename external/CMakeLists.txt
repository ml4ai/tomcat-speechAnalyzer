file(GLOB_RECURSE GOOGLE_API_PROTOS
    googleapis/google/api/*.proto
    googleapis/google/rpc/*.proto
    googleapis/google/cloud/speech/*.proto
    googleapis/google/longrunning/*.proto
    googleapis/google/logging/*.proto
)

find_package(ProtobufWithTargets REQUIRED)
find_package(gRPC REQUIRED)

include(CompileProtos)
add_custom_target(google-cloud-cpp-protos)
google_cloud_cpp_grpcpp_library(google_cloud_speech ${GOOGLE_API_PROTOS}
    PROTO_PATH_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/googleapis/")

project(speechAnalyzer)

cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_definitions(-DBOOST_LOG_DYN_LINK)
find_package(
    Boost 
    COMPONENTS 
    date_time
    chrono
    program_options
    coroutine
    log
    REQUIRED
)
find_package(nlohmann_json REQUIRED)
find_package(SMILEapi REQUIRED)
find_package(Mosquitto REQUIRED)
find_package(range-v3 REQUIRED)

# ---------------------------------------------------------------------
# Process git version information
# ---------------------------------------------------------------------

execute_process(COMMAND git rev-parse HEAD
		OUTPUT_VARIABLE GIT_COMMIT
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET)

set(VERSION "const char* GIT_COMMIT=\"${GIT_COMMIT}\";")

string(STRIP "${GIT_COMMIT}" GIT_COMMIT)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp)
	file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp VERSION_)
else()
	set(VERSION_ "")
endif()

if (NOT "${VERSION}" STREQUAL "${VERSION_}")
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp "${VERSION}")
endif()

# ----------------------------------------------------------------------

option(BUILD_GOOGLE_CLOUD_SPEECH_LIB "Build google_cloud_speech library" ON)

if(BUILD_GOOGLE_CLOUD_SPEECH_LIB)
    add_subdirectory(external)
else()
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/cmake")
    find_package(ProtobufWithTargets REQUIRED)
    find_package(gRPC REQUIRED)
    find_library(google_cloud_speech NAMES google_cloud_speech)
endif()

add_executable(speechAnalyzer 
    src/speechAnalyzer.cpp
    src/JsonBuilder.hpp
    src/Mosquitto.cpp
    src/SpeechWrapper.cpp
    src/helper.cpp
    src/version.cpp
)

target_link_libraries(
    speechAnalyzer 
    SMILEapi 
    nlohmann_json::nlohmann_json
    ${Mosquitto_LIBRARIES}
    ${Boost_LIBRARIES}
    -pthread
    google_cloud_speech
)

if(NOT BUILD_GOOGLE_CLOUD_SPEECH_LIB)
    target_link_libraries(speechAnalyzer protobuf::libprotobuf gRPC::grpc++)
endif()



file(COPY conf/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/conf/)
file(COPY data/test/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/test/)

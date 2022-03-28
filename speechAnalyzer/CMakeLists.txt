project(speechAnalyzer)

cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG)

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

# Add additional search paths to get things to work with MacPorts-provided
# PostgreSQL
set(PostgreSQL_INCLUDE_ADDITIONAL_SEARCH_SUFFIXES "postgresql14")
set(PostgreSQL_LIBRARY_ADDITIONAL_SEARCH_SUFFIXES "postgresql14")

find_package(PostgreSQL REQUIRED)

add_executable(speechAnalyzer 
    src/main.cpp
    src/ASRProcessor.cpp
    src/JsonBuilder.cpp
    src/Mosquitto.cpp
    src/TrialListener.cpp
    src/GlobalMosquittoListener.cpp
    src/OpensmileSession.cpp
    src/OpensmileListener.cpp
    src/DBWrapper.cpp
    src/util.cpp
    src/base64.c
)
target_include_directories(
    speechAnalyzer PRIVATE
    ${PostgreSQL_INCLUDE_DIRS} 
)
target_link_libraries(
    speechAnalyzer 
    SMILEapi 
    nlohmann_json::nlohmann_json
    ${Mosquitto_LIBRARIES}
    ${Boost_LIBRARIES}
    ${PostgreSQL_LIBRARIES}
    -pthread
)

file(COPY conf/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/conf/)

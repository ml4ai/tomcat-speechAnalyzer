project(speechAnalyzer)

cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Boost COMPONENTS date_time REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(SMILEapi REQUIRED)

add_subdirectory(external)

add_executable(speechAnalyzer src/speechAnalyzer.cpp src/JsonBuilder.cpp)

target_include_directories(speechAnalyzer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(
    speechAnalyzer 
    PRIVATE
    SMILEapi 
    nlohmann_json::nlohmann_json
    ${Boost_LIBRARIES}
    -pthread
    google_cloud_speech
)

file(COPY conf/ DESTINATION conf/)
file(COPY data/test/ DESTINATION data/test/)

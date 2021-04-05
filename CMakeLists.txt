project(speechAnalyzer)

cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
add_subdirectory(external)


find_package(Boost COMPONENTS date_time REQUIRED)
find_package(nlohmann_json REQUIRED)

link_directories(lib/opensmile/build/progsrc/smileapi)

add_executable(speechAnalyzer src/speechAnalyzer.cpp  src/JsonBuilder.cpp)

target_include_directories(speechAnalyzer PUBLIC lib/opensmile/progsrc/include/smileapi)
target_include_directories(speechAnalyzer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(speechAnalyzer SMILEapi)
target_link_libraries(speechAnalyzer nlohmann_json::nlohmann_json)
target_link_libraries(speechAnalyzer ${Boost_LIBRARIES})
target_link_libraries(speechAnalyzer -pthread)
target_link_libraries(speechAnalyzer google_cloud_speech)

file(COPY conf/ DESTINATION conf/)
file(COPY data/test/ DESTINATION data/test/)


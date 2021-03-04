project(speechAnalyzer)

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost COMPONENTS date_time REQUIRED)
link_directories(lib/opensmile/build/progsrc/smileapi)

add_executable(speechAnalyzer src/speechAnalyzer.cpp)

target_include_directories(speechAnalyzer PUBLIC lib/opensmile/progsrc/include/smileapi)
target_include_directories(speechAnalyzer PUBLIC lib/json/single_include/nlohmann)
#target_include_directories(speechAnalyzer PUBLIC boost_1_75_0/)

target_link_libraries(speechAnalyzer SMILEapi)
target_link_libraries(speechAnalyzer ${Boost_LIBRARIES})
target_link_libraries(speechAnalyzer -pthread)

file(COPY conf/ DESTINATION conf/)
file(COPY data/test/ DESTINATION data/test/)


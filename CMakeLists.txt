cmake_minimum_required(VERSION 3.6)
project(speechAnalyzer)

link_directories(opensmile-master/build/progsrc/smileapi)

add_executable(speechAnalyzer src/speechAnalyzer.cpp)

target_include_directories(speechAnalyzer PUBLIC opensmile-master/progsrc/include/smileapi)
target_include_directories(speechAnalyzer PUBLIC json/single_include/nlohmann)
target_include_directories(speechAnalyzer PUBLIC boost_1_75_0/)

target_link_libraries(speechAnalyzer SMILEapi)
target_link_libraries(speechAnalyzer -pthread)

file(COPY conf/ DESTINATION conf/)
file(COPY data/test/ DESTINATION data/test/)


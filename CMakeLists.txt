cmake_minimum_required(VERSION 3.6)
project(speechAnalyzer)

link_directories(opensmile-master/build/progsrc/smileapi)

add_executable(speechAnalyzer src/speechAnalyzer.cpp)

target_include_directories(speechAnalyzer PUBLIC opensmile-master/progsrc/include/smileapi)
target_link_libraries(speechAnalyzer SMILEapi)
target_link_libraries(speechAnalyzer -pthread)

file(COPY conf/external_audio_test.conf DESTINATION conf/)
file(COPY data/test/audio.raw DESTINATION data/test)


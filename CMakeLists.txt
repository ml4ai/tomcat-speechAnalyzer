cmake_minimum_required(VERSION 3.6)
project(test)

link_directories(opensmile-master/build/progsrc/smileapi)

add_executable(smileapi_test smileapi_test.cpp)

target_include_directories(smileapi_test PUBLIC opensmile-master/progsrc/include/smileapi)
target_link_libraries(smileapi_test SMILEapi)
target_link_libraries(smileapi_test -pthread)


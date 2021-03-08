cmake_minimum_required(VERSION 3.15)
project(grpcTest)


#find_package(PkgConfig REQUIRED)
#pkg_search_module(GRPC REQUIRED grpc)
#pkg_search_module(GRPCPP REQUIRED grpc++)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
add_subdirectory(external)

add_executable(grpcTest streaming.cc parse_arguments.cc)

target_include_directories(grpcTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(grpcTest google_cloud_speech)

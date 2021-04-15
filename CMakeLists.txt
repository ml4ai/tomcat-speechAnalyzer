project(speechAnalyzer)

cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#Process git version information
#-------------------------------------------------------------------
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
#-------------------------------------------------------------------

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
add_subdirectory(external)


add_definitions(-DBOOST_LOG_DYN_LINK)
find_package(Boost COMPONENTS chrono program_options date_time Coroutine Log REQUIRED)
find_package(range-v3 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Mosquitto REQUIRED)

link_directories(lib/opensmile/build/progsrc/smileapi)

add_executable(speechAnalyzer src/speechAnalyzer.cpp src/JsonBuilder.cpp src/Mosquitto.cpp src/SpeechWrapper.cpp src/version.cpp)

target_include_directories(speechAnalyzer PUBLIC lib/opensmile/progsrc/include/smileapi)
target_include_directories(speechAnalyzer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(speechAnalyzer SMILEapi)
target_link_libraries(speechAnalyzer nlohmann_json::nlohmann_json)
target_link_libraries(speechAnalyzer ${Boost_LIBRARIES})
target_link_libraries(speechAnalyzer ${Mosquitto_LIBRARIES})
target_link_libraries(speechAnalyzer -pthread)
target_link_libraries(speechAnalyzer google_cloud_speech)

file(COPY conf/ DESTINATION conf/)
file(COPY data/test/ DESTINATION data/test/)


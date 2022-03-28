tomcat-speechAnalyzer
================

Agent that listens to the microphone and outputs messages corresponding to
real-time ASR transcriptions.

Example usage:

    ./speechAnalyzer --mode websocket --mqtt_host "127.0.0.1" --mqtt_port 5556

To see all available options, run:

    ./speechAnalyzer -h

## Prerequisites

* C++ compiler that supports C++17 or higher.
* OpenSMILE: https://www.audeering.com/opensmile/ (the code has been tested with
  OpenSMILE v3.0.0). If you don't already have it, you can download it by
  running the following:
  ```
  ./tools/install_opensmile_from_source
  ```
  If using MacPorts, you can also use the following:
  ```
  sudo port install opensmile
  ```
* nlohmann::json library: https://github.com/nlohmann/json
* Boost Library: https://www.boost.org/ 
* gRPC++ and Protobufers: https://grpc.io/docs/languages/cpp/quickstart/
* Mosquitto: https://mosquitto.org/

## Build process

    mkdir build
    cd build
    cmake ..
    make -j

### Google Cloud engine

To use the Google Cloud Speech Recognition engine, you will need to point the
environment variable GOOGLE_APPLICATION_CREDENTIALS to point to your Google
Cloud credentials file.

### Building locally on MACOS

If using MACOS, there is an additional step required to configure an environmental variable for gRPC:
'''
curl -Lo roots.pem https://pki.google.com/roots.pem
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="$PWD/roots.pem"
'''

Docker instructions
-------------------

You can launch a containerized version of the agent that publishes to an MQTT
message bus using Docker Compose:

    docker-compose up --build


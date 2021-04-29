FROM ubuntu:focal

#ubuntu setup
ENV DEBIAN_FRONTEND "noninteractive"
RUN apt-get update -y && apt-get upgrade -y && \ 
    # Essential build tools
    apt-get install build-essential cmake git -y && \
    # gRPC requirements
    apt-get install g++ autoconf libtool pkg-config clang libc++-dev -y && \
    # Protobuffer
    apt-get install protobuf-compiler -y && \
    # Boost
    apt-get install libboost-all-dev -y  && \
    # range-v3
    apt-get install librange-v3-dev -y && \
    # nlohmann::json
    apt-get install nlohmann-json3-dev -y && \
    # Mosquitto
    apt-get install mosquitto mosquitto-clients libmosquitto-dev -y

#Install protobuf and grpc
ENV GRPC_RELEASE_TAG v1.35.x
RUN git clone -b ${GRPC_RELEASE_TAG} https://github.com/grpc/grpc /var/local/git/grpc && \
		cd /var/local/git/grpc && \
    git submodule update --init --recursive && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake ../.. -DgRPC_INSTALL=ON && \
    make  && \
    make  install 


#speechAnalyzer setup
COPY . /app
WORKDIR /app

#Build opensmile
RUN ./install_opensmile_from_source

#Build speechAnalyzer
RUN tools/build.sh
WORKDIR /app/build


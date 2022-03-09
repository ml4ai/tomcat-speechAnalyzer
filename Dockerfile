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
    apt-get install mosquitto mosquitto-clients libmosquitto-dev -y && \
    # PostgreSQL
    apt-get install libpq-dev postgresql-server-dev-all -y

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


# Build opensmile
RUN apt-get install -y wget
COPY tools/install_opensmile_from_source .
RUN ./install_opensmile_from_source

# speechAnalyzer setup
COPY external /speechAnalyzer/external
WORKDIR /speechAnalyzer/external
RUN mkdir build && cd build && cmake .. && make -j install

COPY src /speechAnalyzer/src
COPY cmake /speechAnalyzer/cmake
COPY CMakeLists.txt /speechAnalyzer
COPY conf /speechAnalyzer/conf
COPY data /speechAnalyzer/data

WORKDIR /speechAnalyzer
RUN mkdir build && cd build && cmake .. -DBUILD_GOOGLE_CLOUD_SPEECH_LIB=OFF &&\
    make -j 

# Build speechAnalyzer
WORKDIR /speechAnalyzer/build
COPY speech_context.txt /speechAnalyzer/build

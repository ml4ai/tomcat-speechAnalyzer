FROM ubuntu:focal

#ubuntu setup
ENV DEBIAN_FRONTEND "noninteractive"
RUN apt-get update -y && apt-get upgrade -y && \ 
    # Essential build tools
    apt-get install -y \
        build-essential \
        cmake \
        git \
        wget \

    # gRPC requirements
    g++ autoconf libtool pkg-config clang libc++-dev \

    # Protocol buffers
    protobuf-compiler \

    # Boost
    libboost-all-dev \
    nlohmann-json3-dev \

    # Mosquitto
    mosquitto mosquitto-clients libmosquitto-dev 


#Install protobuf and grpc
ENV GRPC_RELEASE_TAG v1.35.x
RUN git clone -b ${GRPC_RELEASE_TAG} https://github.com/grpc/grpc /var/local/git/grpc && \
        cd /var/local/git/grpc && \
    git submodule update --init --recursive && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake ../.. -DgRPC_INSTALL=ON && \
    make -j $(nproc) && \
    make -j $(nproc) install 


# Build opensmile
COPY tools/install_opensmile_from_source .
RUN ./install_opensmile_from_source

# speechAnalyzer setup
COPY external /asr_agent/external
WORKDIR /asr_agent/external
RUN mkdir build && cd build && cmake .. && make -j $(nproc) install

COPY src /asr_agent/src
COPY cmake /asr_agent/cmake
COPY CMakeLists.txt /asr_agent
COPY conf /asr_agent/conf
COPY data /asr_agent/data

WORKDIR /asr_agent
RUN mkdir build && cd build && cmake .. -DBUILD_GOOGLE_CLOUD_SPEECH_LIB=OFF &&\
    make -j $(nproc)

# Build speechAnalyzer
WORKDIR /asr_agent/build
COPY speech_context.txt /asr_agent/build

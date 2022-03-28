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

    # Boost
    libboost-all-dev \
    nlohmann-json3-dev \

    # Mosquitto
    mosquitto mosquitto-clients libmosquitto-dev \

    # PostgreSQL
    libpq-dev postgresql-server-dev-all


# Build opensmile
COPY tools/install_opensmile_from_source .
RUN ./install_opensmile_from_source


COPY src /speechAnalyzer/src
COPY cmake /speechAnalyzer/cmake
COPY CMakeLists.txt /speechAnalyzer
COPY conf /speechAnalyzer/conf

WORKDIR /speechAnalyzer
RUN mkdir build && cd build && cmake .. -DBUILD_GOOGLE_CLOUD_SPEECH_LIB=OFF &&\
    make -j $(nproc)

# Build speechAnalyzer
WORKDIR /speechAnalyzer/build

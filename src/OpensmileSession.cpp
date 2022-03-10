#include <iostream>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <boost/log/trivial.hpp>

#include "GlobalMosquittoListener.h"
#include "JsonBuilder.h"
#include "util.h"
#include <smileapi/SMILEapi.h>

#include "OpensmileSession.h"
OpensmileSession::OpensmileSession(int socket_port, JsonBuilder* builder) {
    if (!fork()) {
        this->mode = 1;
        this->server = new OpensmileServer(socket_port);
    }
    else {
        this->mode = 0;
        this->client = new OpensmileClient(socket_port, builder);
    }
}

void OpensmileSession::send_chunk(vector<float> float_chunk) {
    this->client->send_chunk(float_chunk);
}

void OpensmileSession::send_eoi() { this->client->send_eoi(); }

OpensmileClient::OpensmileClient(int socket_port, JsonBuilder* builder) {
    this->socket_port = socket_port;
    this->builder = builder;

    // Start socket client
    if ((this->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        BOOST_LOG_TRIVIAL(error) << "socket_error";
    }

    this->serv_addr.sin_family = AF_INET;
    this->serv_addr.sin_port = htons(this->socket_port);

    if (inet_pton(AF_INET, "127.0.0.1", &this->serv_addr.sin_addr) <= 0) {
        BOOST_LOG_TRIVIAL(error) << "Address error";
    }

    while (connect(this->sock,
                   (struct sockaddr*)&this->serv_addr,
                   sizeof(this->serv_addr)) < 0) {
    }

    // Start listening thread for opensmile output
    this->looping = true;
    this->loop_thread = thread([this] { this->loop(); });
}

void OpensmileClient::send_chunk(vector<float> float_chunk) {
    int len = send(
        this->sock, &float_chunk[0], sizeof(float) * float_chunk.size(), 0);
}

void OpensmileClient::send_eoi() {
    this->looping = false;
    //  this->loop_thread.join();

    close(this->sock);
}

void OpensmileClient::loop() {
    int num_bytes = 256;
    int len = -1;
    while (this->looping) {
        // Recv string
        char c[num_bytes];
        recv(this->sock, &c, num_bytes, 0);
        string message(c);
        this->builder->process_message(message);
    }
}

OpensmileServer::OpensmileServer(int socket_port) {
    this->socket_port = socket_port;

    // Start socket server
    if ((this->server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        BOOST_LOG_TRIVIAL(error) << "socket_error";
    }

    if (setsockopt(this->server_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT,
                   &this->opt,
                   sizeof(this->opt))) {
        BOOST_LOG_TRIVIAL(info) << "setsockopt";
    }

    this->address.sin_family = AF_INET;
    this->address.sin_addr.s_addr = INADDR_ANY;
    this->address.sin_port = htons(this->socket_port);

    if (::bind(this->server_fd,
               (struct sockaddr*)&this->address,
               sizeof(this->address)) < 0) {
        BOOST_LOG_TRIVIAL(info) << "bind";
    }

    if (listen(this->server_fd, 3) < 0) {
        BOOST_LOG_TRIVIAL(info) << "listen";
    }

    if ((this->new_socket = accept(this->server_fd,
                                   (struct sockaddr*)&this->address,
                                   (socklen_t*)&this->addrlen)) < 0) {
        BOOST_LOG_TRIVIAL(info) << "accept";
    }

    // Initialize Opensmile
    this->handle = smile_new();
    smileopt_t* options = NULL;
    smile_initialize(this->handle,
                     "conf/is09-13/IS13_ComParE.conf",
                     0,
                     options,
                     1,
                     0, // Debug
                     0, // Console Output
                     0);
    smile_set_log_callback(this->handle, &log_callback, &(this->new_socket));
    this->opensmile_thread = std::thread(smile_run, this->handle);

    float float_chunk[4096];
    while (true) {
        // Read chunk from socket
        int len = recv(this->new_socket, &float_chunk, sizeof(float) * 4096, 0);
        if (len == 0) {
            break;
        }

        // Send chunk to Opensmile
        while (true) {
            smileres_t result =
                smile_extaudiosource_write_data(handle,
                                                "externalAudioSource",
                                                (void*)&float_chunk[0],
                                                4096 * sizeof(float));
            if (result == SMILE_SUCCESS) {
                break;
            }
        }
    }

    close(this->new_socket);
    smile_extaudiosource_set_external_eoi(this->handle, "externalAudioSource");
    smile_free(this->handle);
    exit(0);
}

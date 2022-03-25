#include <boost/beast/core.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <thread>

#include <boost/log/trivial.hpp>

#include "GlobalMosquittoListener.h"
#include "util.h"

namespace beast = boost::beast; // from <boost/beast.hpp>

using namespace std;

// Report a failure
void fail(beast::error_code ec, char const* what) {
    BOOST_LOG_TRIVIAL(error) << what << ": " << ec.message() << "\n";
}

// Callback function for openSMILE messages
void log_callback(smileobj_t* smileobj, smilelogmsg_t message, void* param) {
    int socket = *((int*)(param));
    int len = -1;
    int num_bytes = 256;

    // Copy string for sending
    string temp(message.text);
    char text[num_bytes];
    strcpy(text, temp.c_str());

    OPENSMILE_MUTEX.lock();
    // Send message
    len = send(socket, &text, num_bytes, 0);
    OPENSMILE_MUTEX.unlock();
}


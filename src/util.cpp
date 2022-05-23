// STDLIB
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <thread>

// Third Party
#include <boost/log/trivial.hpp>
#include <boost/beast/core.hpp>

// Local
#include "OpensmileProcessor.h"
#include "util.h"

namespace beast = boost::beast; // from <boost/beast.hpp>

using namespace std;

// Report a failure
void fail(beast::error_code ec, char const* what) {
    BOOST_LOG_TRIVIAL(error) << what << ": " << ec.message() << "\n";
}

// Callback function for openSMILE messages
void log_callback(smileobj_t* smileobj, smilelogmsg_t message, void* param) {
    OpensmileProcessor* processor = (OpensmileProcessor*)(param);
    processor->ProcessOpensmileLog(message.text);
}

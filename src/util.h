#pragma once
#include "JsonBuilder.h"
#include <smileapi/SMILEapi.h>

#include <boost/beast/core.hpp>

#include "util.h"
using namespace std;
namespace beast = boost::beast; // from <boost/beast.hpp>

// Report a failure
void fail(beast::error_code ec, char const* what);

// Callback function for openSMILE messages
void log_callback(smileobj_t*, smilelogmsg_t, void*);


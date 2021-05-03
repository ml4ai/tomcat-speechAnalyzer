#pragma once
#include <boost/beast/core.hpp>
#include <iostream>

using namespace std;
namespace beast = boost::beast; // from <boost/beast.hpp>

// Report a failure
void fail(beast::error_code ec, char const* what) {
    cerr << what << ": " << ec.message() << "\n";
}

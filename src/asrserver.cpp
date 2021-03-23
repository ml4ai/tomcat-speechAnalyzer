//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include "util.hpp"
#include "WebsocketSession.hpp"
#include "HTTPSession.hpp"
#include "Listener.hpp"

#include <algorithm>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>



namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace ws = beast::websocket;  // from <boost/beast/websocket.hpp>
namespace asio = boost::asio;     // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
using namespace std;



//------------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    // Check command line arguments.
    if (argc != 5) {
        cerr << "Usage: advanced-server <address> <port> <doc_root> <threads>\n"
             << "Example:\n"
             << "    advanced-server 0.0.0.0 8080 . 1\n";
        return EXIT_FAILURE;
    }

    cout << "Starting websocket server." << endl;
    auto const address = asio::ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(atoi(argv[2]));
    auto const doc_root = make_shared<string>(argv[3]);
    auto const n_threads = max<int>(1, atoi(argv[4]));

    // The io_context is required for all I/O
    asio::io_context ioc{n_threads};

    // Create and launch a listening port
    auto listener = make_shared<Listener>(ioc, tcp::endpoint{address, port}, doc_root);
    listener->run();

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](beast::error_code const&, int) {
        // Stop the `io_context`. This will cause `run()`
        // to return immediately, eventually destroying the
        // `io_context` and all of the sockets in it.
        ioc.stop();
    });

    // Run the I/O service on the requested number of threads
    vector<thread> threads;
    threads.reserve(n_threads - 1);
    for (auto i = n_threads - 1; i > 0; --i) {
        threads.emplace_back([&ioc] { ioc.run(); });
    }
    ioc.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)

    // Block until all the threads exit
    for (auto& thread : threads) {
        thread.join();
    }

    return EXIT_SUCCESS;
}

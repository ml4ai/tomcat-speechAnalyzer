#pragma once

#include <boost/beast/core.hpp>

namespace asio = boost::asio;   // from <boost/asio.hpp>
namespace beast = boost::beast; // from <boost/beast.hpp>

// Accepts incoming connections and launches the sessions
class Listener : public enable_shared_from_this<Listener> {
    asio::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<std::string const> doc_root_;

  public:
    Listener(asio::io_context& ioc, tcp::endpoint endpoint,
             std::shared_ptr<std::string const> const& doc_root)
        : ioc_(ioc), acceptor_(asio::make_strand(ioc)), doc_root_(doc_root) {
        beast::error_code ec;

        // Open the acceptor
        this->acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        this->acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
        if (ec) {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        this->acceptor_.bind(endpoint, ec);
        if (ec) {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        this->acceptor_.listen(asio::socket_base::max_listen_connections, ec);
        if (ec) {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void run() {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        asio::dispatch(this->acceptor_.get_executor(),
                       beast::bind_front_handler(&Listener::do_accept,
                                                 this->shared_from_this()));
    }

  private:
    void do_accept() {
        // The new connection gets its own strand
        this->acceptor_.async_accept(
            asio::make_strand(this->ioc_),
            beast::bind_front_handler(&Listener::on_accept,
                                      this->shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (ec) {
            fail(ec, "accept");
        }
        else {
            // Create the http session and run it
            auto http_session =
                make_shared<HTTPSession>(move(socket), this->doc_root_);
            http_session->run();
        }

        // Accept another connection
        this->do_accept();
    }
};

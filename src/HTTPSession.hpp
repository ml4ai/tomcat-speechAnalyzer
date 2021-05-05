#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/optional.hpp>

namespace http = beast::http; // from <boost/beast/http.hpp>
namespace asio = boost::asio; // from <boost/asio.hpp>
using namespace std;
// Return a reasonable mime type based on the extension of a file.
beast::string_view mime_type(beast::string_view path) {
    using beast::iequals;
    auto const ext = [&path] {
        auto const pos = path.rfind(".");
        if (pos == beast::string_view::npos)
            return beast::string_view{};
        return path.substr(pos);
    }();
    if (iequals(ext, ".htm"))
        return "text/html";
    if (iequals(ext, ".html"))
        return "text/html";
    if (iequals(ext, ".php"))
        return "text/html";
    if (iequals(ext, ".css"))
        return "text/css";
    if (iequals(ext, ".txt"))
        return "text/plain";
    if (iequals(ext, ".js"))
        return "application/javascript";
    if (iequals(ext, ".json"))
        return "application/json";
    if (iequals(ext, ".xml"))
        return "application/xml";
    if (iequals(ext, ".swf"))
        return "application/x-shockwave-flash";
    if (iequals(ext, ".flv"))
        return "video/x-flv";
    if (iequals(ext, ".png"))
        return "image/png";
    if (iequals(ext, ".jpe"))
        return "image/jpeg";
    if (iequals(ext, ".jpeg"))
        return "image/jpeg";
    if (iequals(ext, ".jpg"))
        return "image/jpeg";
    if (iequals(ext, ".gif"))
        return "image/gif";
    if (iequals(ext, ".bmp"))
        return "image/bmp";
    if (iequals(ext, ".ico"))
        return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff"))
        return "image/tiff";
    if (iequals(ext, ".tif"))
        return "image/tiff";
    if (iequals(ext, ".svg"))
        return "image/svg+xml";
    if (iequals(ext, ".svgz"))
        return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
string path_cat(beast::string_view base, beast::string_view path) {
    if (base.empty()) {
        return string(path);
    }
    string result(base);
#ifdef BOOST_MSVC
    char constexpr path_separator = '\\';
    if (result.back() == path_separator) {
        result.resize(result.size() - 1);
    }

    result.append(path.data(), path.size());
    for (auto& c : result) {
        if (c == '/') {
            c = path_separator;
        }
    }

#else
    char constexpr path_separator = '/';
    if (result.back() == path_separator) {
        result.resize(result.size() - 1);
    }
    result.append(path.data(), path.size());
#endif
    return result;
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template <class Body, class Allocator, class Send>
void handle_request(beast::string_view doc_root,
                    http::request<Body, http::basic_fields<Allocator>>&& req,
                    Send&& send) {
    // Returns a bad request response
    auto const bad_request = [&req](beast::string_view why) {
        http::response<http::string_body> res{http::status::bad_request,
                                              req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = string(why);
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found = [&req](beast::string_view target) {
        http::response<http::string_body> res{http::status::not_found,
                                              req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error = [&req](beast::string_view what) {
        http::response<http::string_body> res{
            http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + string(what) + "'";
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return send(bad_request("Unknown HTTP-method"));
    }

    // Request path must be absolute and not contain "..".
    if (req.target().empty() || req.target()[0] != '/' ||
        req.target().find("..") != beast::string_view::npos) {
        return send(bad_request("Illegal request-target"));
    }

    // Build the path to the requested file
    string path = path_cat(doc_root, req.target());
    if (req.target().back() == '/') {
        path.append("index.html");
    }

    // Attempt to open the file
    beast::error_code ec;
    http::file_body::value_type body;
    body.open(path.c_str(), beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if (ec == beast::errc::no_such_file_or_directory) {
        return send(not_found(req.target()));
    }

    // Handle an unknown error
    if (ec) {
        return send(server_error(ec.message()));
    }

    // Cache the size since we need it after the move
    auto const size = body.size();

    // Respond to HEAD request
    if (req.method() == http::verb::head) {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(move(res));
    }

    // Respond to GET request
    http::response<http::file_body> res{
        piecewise_construct,
        make_tuple(move(body)),
        make_tuple(http::status::ok, req.version())};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return send(move(res));
}

// Handles an HTTP server connection
class HTTPSession : public enable_shared_from_this<HTTPSession> {
    // This queue is used for HTTP pipelining.
    class queue {
        enum {
            // Maximum number of responses we will queue
            limit = 8
        };

        // The type-erased, saved work item
        struct work {
            virtual ~work() = default;
            virtual void operator()() = 0;
        };

        HTTPSession& self_;
        std::vector<std::unique_ptr<work>> items_;

      public:
        explicit queue(HTTPSession& self) : self_(self) {
            static_assert(limit > 0, "queue limit must be positive");
            items_.reserve(limit);
        }

        // Returns `true` if we have reached the queue limit
        bool is_full() const { return this->items_.size() >= limit; }

        // Called when a message finishes sending
        // Returns `true` if the caller should initiate a read
        bool on_write() {
            BOOST_ASSERT(!items_.empty());
            auto const was_full = this->is_full();
            this->items_.erase(this->items_.begin());
            if (!this->items_.empty()) {
                (*items_.front())();
            }
            return was_full;
        }

        // Called by the HTTP handler to send a response.
        template <bool isRequest, class Body, class Fields>
        void operator()(http::message<isRequest, Body, Fields>&& msg) {
            // This holds a work item
            struct work_impl : work {
                HTTPSession& self_;
                http::message<isRequest, Body, Fields> msg_;

                work_impl(HTTPSession& self,
                          http::message<isRequest, Body, Fields>&& msg)
                    : self_(self), msg_(move(msg)) {}

                void operator()() {
                    http::async_write(
                        self_.stream_,
                        msg_,
                        beast::bind_front_handler(&HTTPSession::on_write,
                                                  self_.shared_from_this(),
                                                  msg_.need_eof()));
                }
            };

            // Allocate and store the work
            this->items_.push_back(
                boost::make_unique<work_impl>(self_, move(msg)));

            // If there was no previous work, start this one
            if (this->items_.size() == 1) {
                (*items_.front())();
            }
        }
    };

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr<std::string const> doc_root_;
    queue queue_;

    // The parser is stored in an optional container so we can
    // construct it from scratch it at the beginning of each new message.
    boost::optional<http::request_parser<http::string_body>> parser_;

  public:
    // Take ownership of the socket
    HTTPSession(tcp::socket&& socket,
                std::shared_ptr<std::string const> const& doc_root)
        : stream_(move(socket)), doc_root_(doc_root), queue_(*this) {}

    // Start the session
    void run() {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        asio::dispatch(this->stream_.get_executor(),
                       beast::bind_front_handler(&HTTPSession::do_read,
                                                 this->shared_from_this()));
    }

  private:
    void do_read() {
        // Construct a new parser for each message
        this->parser_.emplace();

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        this->parser_->body_limit(10000);

        // Set the timeout.
        this->stream_.expires_after(chrono::seconds(30));

        // Read a request using the parser-oriented interface
        http::async_read(this->stream_,
                         buffer_,
                         *this->parser_,
                         beast::bind_front_handler(&HTTPSession::on_read,
                                                   this->shared_from_this()));
    }

    void on_read(beast::error_code ec, size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if (ec == http::error::end_of_stream) {
            return this->do_close();
        }

        if (ec) {
            return fail(ec, "read");
        }

        // See if it is a WebSocket Upgrade
        if (ws::is_upgrade(this->parser_->get())) {
            // Create a websocket session, transferring ownership
            // of both the socket and the HTTP request.
            auto websocket_session =
                make_shared<WebsocketSession>(this->stream_.release_socket());
            websocket_session->do_accept(this->parser_->release());
            return;
        }

        // Send the response
        handle_request(*doc_root_, this->parser_->release(), this->queue_);

        // If we aren't at the queue limit, try to pipeline another request
        if (!this->queue_.is_full()) {
            this->do_read();
        }
    }

    void on_write(bool close, beast::error_code ec, size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec) {
            return fail(ec, "write");
        }

        if (close) {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return this->do_close();
        }

        // Inform the queue that a write completed
        if (this->queue_.on_write()) {
            // Read another request
            this->do_read();
        }
    }

    void do_close() {
        // Send a TCP shutdown
        beast::error_code ec;
        this->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

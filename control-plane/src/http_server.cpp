#include "http_server.hpp"

#include "api/serialization.hpp"

#include <boost/beast/http.hpp>

#include <chrono>
#include <memory>
#include <utility>

namespace proxy_scheduler
{
    namespace
    {
        constexpr std::uint64_t max_request_body_bytes = 64 * 1024;
        constexpr std::uint32_t max_request_header_bytes = 16 * 1024;
        constexpr auto request_read_timeout = std::chrono::seconds(10);
    }

    HttpSession::HttpSession(tcp::socket socket, api::ApiHandler &handler)
        : socket_(std::move(socket)),
          read_timer_(socket_.get_executor()),
          handler_(handler)
    {
        parser_.body_limit(max_request_body_bytes);
        parser_.header_limit(max_request_header_bytes);
    }

    void HttpSession::start()
    {
        read_request();
    }

    void HttpSession::read_request()
    {
        read_timer_.expires_after(request_read_timeout);
        read_timer_.async_wait(
            [self = shared_from_this()](beast::error_code ec)
            {
                if (ec == boost::asio::error::operation_aborted) return;
                beast::error_code ignored;
                self->socket_.close(ignored);
            });

        http::async_read(
            socket_, buffer_, parser_,
            [self = shared_from_this()](beast::error_code ec, std::size_t)
            {
                self->read_timer_.cancel();
                if (ec)
                {
                    if (ec == http::error::body_limit)
                    {
                        self->write_response(api::make_error_response(
                            11, http::status::payload_too_large, "request body is too large"));
                    }
                    else if (ec == http::error::header_limit)
                    {
                        self->write_response(api::make_error_response(
                            11, http::status::request_header_fields_too_large,
                            "request headers are too large"));
                    }
                    return;
                }
                self->handle_request();
            });
    }

    void HttpSession::handle_request()
    {
        auto request = parser_.release();
        try
        {
            handler_.handle(
                request,
                [self = shared_from_this()](http::response<http::string_body> response)
                {
                    self->write_response(std::move(response));
                });
        }
        catch (const std::exception &)
        {
            write_response(api::make_error_response(
                request.version(), http::status::internal_server_error, "internal server error"));
        }
    }

    void HttpSession::write_response(http::response<http::string_body> response)
    {
        auto shared_response =
            std::make_shared<http::response<http::string_body>>(std::move(response));
        http::async_write(
            socket_, *shared_response,
            [self = shared_from_this(), shared_response](beast::error_code, std::size_t)
            {
                beast::error_code ignored;
                self->socket_.shutdown(tcp::socket::shutdown_send, ignored);
            });
    }

    HttpServer::HttpServer(
        boost::asio::io_context &io,
        NodeRegistry &registry,
        Scheduler &scheduler,
        std::uint16_t port)
        : io_(io),
          acceptor_(io, tcp::endpoint(tcp::v4(), port)),
          handler_(registry, scheduler)
    {
    }

    void HttpServer::start()
    {
        do_accept();
    }

    void HttpServer::stop()
    {
        stopped_ = true;
        beast::error_code ignored;
        acceptor_.close(ignored);
    }

    void HttpServer::do_accept()
    {
        if (stopped_) return;
        acceptor_.async_accept(
            [self = shared_from_this()](beast::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    std::make_shared<HttpSession>(std::move(socket), self->handler_)->start();
                }
                if (!self->stopped_) self->do_accept();
            });
    }
}

#include "http_server.hpp"

#include <boost/beast/http.hpp>
#include <iostream>
#include <utility>

namespace proxy_scheduler
{
    HttpSession::HttpSession(
        tcp::socket socket,
        NodeRegistry &registry,
        Scheduler &scheduler)
        : socket_(std::move(socket)),
          registry_(registry),
          scheduler_(scheduler)
    {
    }

    void HttpSession::start()
    {
        read_request();
    }

    void HttpSession::read_request()
    {
        http::async_read(
            socket_, buffer_, request_,
            [self = shared_from_this()](beast::error_code ec, std::size_t)
            {
                if (ec)
                {
                    return;
                }

                self->handle_request();
            });
    }

    void HttpSession::handle_request()
    {
        http::response<http::string_body> response;

        response.version(request_.version());
        response.set(http::field::server, "proxy-scheduler");
        response.set(http::field::content_type, "application/json");
        response.keep_alive(false);

        const auto method = request_.method();
        const std::string target = std::string(request_.target());

        if (method == http::verb::get && target == "/health")
        {
            response.result(http::status::ok);
            response.body() = R"({"status":"ok"})";
        }
        else if (method == http::verb::get && target == "/nodes")
        {
            response.result(http::status::ok);
            response.body() = R"({"message":"list nodes TODO"})";
        }
        else if (method == http::verb::post && target == "/nodes/register")
        {
            response.result(http::status::ok);
            response.body() = R"({"message":"register node TODO"})";
        }
        else if (method == http::verb::post && target == "/schedule")
        {
            response.result(http::status::ok);
            response.body() = R"({"message":"schedule TODO"})";
        }
        else
        {
            response.result(http::status::not_found);
            response.body() = R"({"error":"not found"})";
        }

        response.prepare_payload();
        write_response(std::move(response));
    }

    void HttpSession::write_response(http::response<http::string_body> response)
    {
        auto shared_response =
            std::make_shared<http::response<http::string_body>>(std::move(response));

        http::async_write(
            socket_,
            *shared_response,
            [self = shared_from_this(), shared_response](beast::error_code, std::size_t)
            {
                beast::error_code ec;
                self->socket_.shutdown(tcp::socket::shutdown_send, ec);
            });
    }

    HttpServer::HttpServer(
        boost::asio::io_context &io,
        NodeRegistry &registry,
        Scheduler &scheduler,
        std::uint16_t port)
        : io_(io),
          acceptor_(io, tcp::endpoint(tcp::v4(), port)),
          registry_(registry),
          scheduler_(scheduler)
    {
    }

    void HttpServer::start()
    {
        do_accept();
    }
    void HttpServer::stop()
    {
        stopped_ = true;

        beast::error_code ec;
        acceptor_.close(ec);
    }

    void HttpServer::do_accept()
    {
        if (stopped_)
        {
            return;
        }

        acceptor_.async_accept(
            [self = shared_from_this()](beast::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    auto session = std::make_shared<HttpSession>(
                        std::move(socket),
                        self->registry_,
                        self->scheduler_);
                    session->start();
                }

                self->do_accept();
            });
    }
} // namespace proxy_scheduler

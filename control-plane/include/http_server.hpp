#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "api/handlers.hpp"

namespace proxy_scheduler
{
    namespace beast = boost::beast;
    namespace http = beast::http;
    class HttpSession : public std::enable_shared_from_this<HttpSession>
    {
        using tcp = boost::asio::ip::tcp;

    public:
        HttpSession(
            tcp::socket socket,
            api::ApiHandler &handler);
        void start();

    private:
        void read_request();
        void handle_request();
        void write_response(http::response<http::string_body> response);

        tcp::socket socket_;
        beast::flat_buffer buffer_;
        http::request_parser<http::string_body> parser_;
        boost::asio::steady_timer read_timer_;
        api::ApiHandler &handler_;
    };

    class HttpServer : public std::enable_shared_from_this<HttpServer>
    {
        using tcp = boost::asio::ip::tcp;

    public:
        HttpServer(
            boost::asio::io_context &io,
            NodeRegistry &registry,
            Scheduler &scheduler,
            std::uint16_t port);

        void start();
        void stop();

    private:
        void do_accept();

        boost::asio::io_context &io_;
        tcp::acceptor acceptor_;

        api::ApiHandler handler_;

        bool stopped_{false};
    };
}

#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "node_registry.hpp"
#include "scheduler.hpp"

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
            NodeRegistry &registry,
            Scheduler &scheduler);
        void start();

    private:
        void read_request();
        void handle_request();
        void write_response(http::response<http::string_body> response);

        tcp::socket socket_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> request_;

        NodeRegistry &registry_;
        Scheduler &scheduler_;
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

        NodeRegistry &registry_;
        Scheduler &scheduler_;

        bool stopped_{false};
    };
}
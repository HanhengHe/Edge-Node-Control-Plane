#include "http_server.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <utility>

namespace proxy_scheduler
{
    namespace json = boost::json;

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

        auto send_response = [self = shared_from_this()](http::response<http::string_body> response)
        {
            response.prepare_payload();
            self->write_response(std::move(response));
        };

        auto make_error =
            [&](http::status status, const std::string &message)
        {
            response.result(status);
            json::object body;
            body["error"] = message;
            response.body() = json::serialize(body);
            send_response(std::move(response));
        };

        if (method == http::verb::get && target == "/health")
        {
            response.result(http::status::ok);
            json::object body;
            body["status"] = "ok";
            response.body() = json::serialize(body);
            send_response(std::move(response));
            return;
        }

        if (method == http::verb::post && target == "/nodes/register")
        {
            json::value parsed;

            try
            {
                parsed = json::parse(request_.body());
            }
            catch (const std::exception &)
            {
                make_error(http::status::bad_request, "invalid json");
                return;
            }

            if (!parsed.is_object())
            {
                make_error(http::status::bad_request, "request body must be a json object");
                return;
            }

            const auto &obj = parsed.as_object();

            try
            {
                Node node;
                node.node_id = static_cast<NodeId>(obj.at("node_id").as_int64());
                node.region = std::string(obj.at("region").as_string());
                node.host = std::string(obj.at("host").as_string());
                node.port = static_cast<std::uint16_t>(obj.at("port").as_int64());
                node.current_load = static_cast<std::uint32_t>(obj.at("current_load").as_int64());
                node.max_capacity = static_cast<std::uint32_t>(obj.at("max_capacity").as_int64());
                node.latency_ms = static_cast<std::uint32_t>(obj.at("latency_ms").as_int64());

                registry_.register_node(std::move(node));

                response.result(http::status::ok);

                json::object body;
                body["status"] = "registered";
                body["node_id"] = obj.at("node_id").as_int64();

                response.body() = json::serialize(body);
                send_response(std::move(response));
                return;
            }
            catch (const std::exception &)
            {
                make_error(http::status::bad_request, "missing or invalid node fields");
                return;
            }
        }

        if (method == http::verb::get && target == "/nodes")
        {
            registry_.get_all_nodes(
                [response = std::move(response),
                 send_response](std::vector<Node> nodes) mutable
                {
                    json::array node_array;

                    for (const auto &node : nodes)
                    {
                        json::object node_obj;

                        node_obj["node_id"] = node.node_id;
                        node_obj["region"] = node.region;
                        node_obj["host"] = node.host;
                        node_obj["port"] = node.port;
                        node_obj["current_load"] = node.current_load;
                        node_obj["max_capacity"] = node.max_capacity;
                        node_obj["latency_ms"] = node.latency_ms;

                        switch (node.status)
                        {
                        case NodeStatus::Healthy:
                            node_obj["status"] = "Healthy";
                            break;
                        case NodeStatus::Unhealthy:
                            node_obj["status"] = "Unhealthy";
                            break;
                        case NodeStatus::Draining:
                            node_obj["status"] = "Draining";
                            break;
                        }

                        node_array.push_back(std::move(node_obj));
                    }

                    json::object body;
                    body["nodes"] = std::move(node_array);

                    response.result(http::status::ok);
                    response.body() = json::serialize(body);
                    send_response(std::move(response));
                });

            return;
        }

        if (method == http::verb::get && target == "/healthy-nodes")
        {
            registry_.get_healthy_nodes(
                [response = std::move(response),
                 send_response](std::vector<Node> nodes) mutable
                {
                    json::array node_array;

                    for (const auto &node : nodes)
                    {
                        json::object node_obj;

                        node_obj["node_id"] = node.node_id;
                        node_obj["region"] = node.region;
                        node_obj["host"] = node.host;
                        node_obj["port"] = node.port;
                        node_obj["current_load"] = node.current_load;
                        node_obj["max_capacity"] = node.max_capacity;
                        node_obj["latency_ms"] = node.latency_ms;
                        node_obj["status"] = "Healthy";

                        node_array.push_back(std::move(node_obj));
                    }

                    json::object body;
                    body["nodes"] = std::move(node_array);

                    response.result(http::status::ok);
                    response.body() = json::serialize(body);
                    send_response(std::move(response));
                });

            return;
        }

        if (method == http::verb::post && target == "/schedule")
        {
            json::value parsed;

            try
            {
                if (request_.body().empty())
                {
                    parsed = json::object{};
                }
                else
                {
                    parsed = json::parse(request_.body());
                }
            }
            catch (const std::exception &)
            {
                make_error(http::status::bad_request, "invalid json");
                return;
            }

            if (!parsed.is_object())
            {
                make_error(http::status::bad_request, "request body must be a json object");
                return;
            }

            const auto &obj = parsed.as_object();

            SchedulingRequest scheduling_request;

            if (auto *preferred_region = obj.if_contains("preferred_region"))
            {
                if (!preferred_region->is_string())
                {
                    make_error(http::status::bad_request, "preferred_region must be string");
                    return;
                }

                scheduling_request.preferred_region =
                    std::string(preferred_region->as_string());
            }

            if (auto *max_latency_ms = obj.if_contains("max_latency_ms"))
            {
                if (!max_latency_ms->is_int64())
                {
                    make_error(http::status::bad_request, "max_latency_ms must be integer");
                    return;
                }

                scheduling_request.max_latency_ms =
                    static_cast<std::uint32_t>(max_latency_ms->as_int64());
            }

            if (auto *allow_cross_region = obj.if_contains("allow_cross_region"))
            {
                if (!allow_cross_region->is_bool())
                {
                    make_error(http::status::bad_request, "allow_cross_region must be bool");
                    return;
                }

                scheduling_request.allow_cross_region =
                    allow_cross_region->as_bool();
            }

            scheduler_.select_node(
                std::move(scheduling_request),
                [response = std::move(response), send_response](std::optional<SchedulingDecision> decision) mutable
                {
                    if (!decision)
                    {
                        response.result(http::status::service_unavailable);

                        json::object body;
                        body["selected"] = false;
                        body["error"] = "no healthy node available";

                        response.body() = json::serialize(body);
                        send_response(std::move(response));
                        return;
                    }

                    const auto &node = decision->node;

                    json::object node_obj;
                    node_obj["node_id"] = node.node_id;
                    node_obj["region"] = node.region;
                    node_obj["host"] = node.host;
                    node_obj["port"] = node.port;
                    node_obj["current_load"] = node.current_load;
                    node_obj["max_capacity"] = node.max_capacity;
                    node_obj["latency_ms"] = node.latency_ms;

                    json::object body;
                    body["selected"] = true;
                    body["score"] = decision->score;
                    body["node"] = std::move(node_obj);

                    response.result(http::status::ok);
                    response.body() = json::serialize(body);

                    send_response(std::move(response));
                });

            return;
        }

        response.result(http::status::not_found);

        json::object body;
        body["error"] = "not found";

        response.body() = json::serialize(body);
        send_response(std::move(response));
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

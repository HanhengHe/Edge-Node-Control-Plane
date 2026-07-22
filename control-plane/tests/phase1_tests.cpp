#include "api/handlers.hpp"
#include "node_registry.hpp"
#include "scheduler.hpp"

#include <boost/asio.hpp>
#include <boost/beast/http.hpp>

#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace http = boost::beast::http;
using proxy_scheduler::NodeRegistry;
using proxy_scheduler::Scheduler;
using proxy_scheduler::api::ApiHandler;

int main()
{
    boost::asio::io_context io;
    auto registry = std::make_shared<NodeRegistry>(io.get_executor());
    Scheduler scheduler(*registry);
    ApiHandler handler(*registry, scheduler);

    auto perform = [&](http::verb method, std::string target, std::string body,
                       bool json_content_type = true)
    {
        ApiHandler::Request request{method, std::move(target), 11};
        request.body() = std::move(body);
        if (json_content_type)
        {
            request.set(http::field::content_type, "application/json; charset=utf-8");
        }
        request.prepare_payload();

        std::optional<ApiHandler::Response> response;
        handler.handle(request, [&](ApiHandler::Response value) { response = std::move(value); });
        io.restart();
        io.run();
        assert(response.has_value());
        return std::move(*response);
    };

    const std::string valid_node = R"({
        "node_id": 7,
        "region": "ca-central",
        "host": "agent-7",
        "port": 9007,
        "current_load": 10,
        "max_capacity": 100,
        "latency_ms": 5
    })";

    assert(perform(http::verb::post, "/nodes/register", valid_node).result() ==
           http::status::created);
    assert(perform(http::verb::post, "/nodes/register", valid_node).result() ==
           http::status::conflict);

    const std::string missing_node_heartbeat = R"({
        "node_id": 8,
        "current_load": 1,
        "max_capacity": 10,
        "latency_ms": 1
    })";
    assert(perform(http::verb::post, "/nodes/heartbeat", missing_node_heartbeat).result() ==
           http::status::not_found);

    const std::string invalid_load = R"({
        "node_id": 7,
        "current_load": 101,
        "max_capacity": 100,
        "latency_ms": 1
    })";
    assert(perform(http::verb::post, "/nodes/heartbeat", invalid_load).result() ==
           http::status::unprocessable_entity);

    const std::string invalid_port = R"({
        "node_id": 9,
        "region": "ca-central",
        "host": "agent-9",
        "port": -1,
        "current_load": 0,
        "max_capacity": 10,
        "latency_ms": 0
    })";
    assert(perform(http::verb::post, "/nodes/register", invalid_port).result() ==
           http::status::unprocessable_entity);
    assert(perform(http::verb::post, "/nodes/drain", R"({"node_id":7})", false).result() ==
           http::status::unsupported_media_type);
    assert(perform(http::verb::post, "/nodes/drain", R"({"node_id":99})").result() ==
           http::status::not_found);
    assert(perform(http::verb::post, "/nodes/drain", R"({"node_id":7})").result() ==
           http::status::ok);
    const auto nodes_after_drain = perform(http::verb::get, "/nodes", "", false);
    assert(nodes_after_drain.body().find("Draining") != std::string::npos);
    assert(perform(http::verb::post, "/schedule", "{}").result() ==
           http::status::service_unavailable);
    assert(perform(http::verb::delete_, "/nodes/remove", R"({"node_id":7})").result() ==
           http::status::no_content);
    assert(perform(http::verb::delete_, "/nodes/remove", R"({"node_id":7})").result() ==
           http::status::not_found);
    return 0;
}

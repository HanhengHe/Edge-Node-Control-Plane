#include "api/serialization.hpp"

namespace proxy_scheduler::api
{
    json::object serialize_node(const Node &node, bool include_status)
    {
        json::object result;
        result["node_id"] = node.node_id;
        result["region"] = node.region;
        result["host"] = node.host;
        result["port"] = node.port;
        result["current_load"] = node.current_load;
        result["max_capacity"] = node.max_capacity;
        result["latency_ms"] = node.latency_ms;
        if (include_status)
        {
            switch (node.status)
            {
            case NodeStatus::Healthy: result["status"] = "Healthy"; break;
            case NodeStatus::Unhealthy: result["status"] = "Unhealthy"; break;
            case NodeStatus::Draining: result["status"] = "Draining"; break;
            }
        }
        return result;
    }

    json::array serialize_nodes(const std::vector<Node> &nodes)
    {
        json::array result;
        result.reserve(nodes.size());
        for (const auto &node : nodes) result.emplace_back(serialize_node(node));
        return result;
    }

    http::response<http::string_body> make_json_response(
        unsigned version, http::status status, json::value body)
    {
        http::response<http::string_body> response{status, version};
        response.set(http::field::server, "proxy-scheduler");
        response.set(http::field::content_type, "application/json");
        response.keep_alive(false);
        response.body() = json::serialize(body);
        response.prepare_payload();
        return response;
    }

    http::response<http::string_body> make_error_response(
        unsigned version, http::status status, const std::string &message)
    {
        json::object body;
        body["error"] = message;
        return make_json_response(version, status, std::move(body));
    }

    http::response<http::string_body> make_empty_response(unsigned version, http::status status)
    {
        http::response<http::string_body> response{status, version};
        response.set(http::field::server, "proxy-scheduler");
        response.keep_alive(false);
        response.prepare_payload();
        return response;
    }
}

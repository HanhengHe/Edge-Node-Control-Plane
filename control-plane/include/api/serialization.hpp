#pragma once

#include "node.hpp"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <string>
#include <vector>

namespace proxy_scheduler::api
{
    namespace http = boost::beast::http;
    namespace json = boost::json;

    json::object serialize_node(const Node &node, bool include_status = true);
    json::array serialize_nodes(const std::vector<Node> &nodes);

    http::response<http::string_body> make_json_response(
        unsigned version,
        http::status status,
        json::value body);

    http::response<http::string_body> make_error_response(
        unsigned version,
        http::status status,
        const std::string &message);

    http::response<http::string_body> make_empty_response(
        unsigned version,
        http::status status);
}

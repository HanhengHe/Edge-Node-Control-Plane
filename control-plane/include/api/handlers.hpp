#pragma once

#include "node_registry.hpp"
#include "scheduler.hpp"

#include <boost/beast/http.hpp>

#include <functional>

namespace proxy_scheduler::api
{
    namespace http = boost::beast::http;

    class ApiHandler
    {
    public:
        using Request = http::request<http::string_body>;
        using Response = http::response<http::string_body>;
        using SendResponse = std::function<void(Response)>;

        ApiHandler(NodeRegistry &registry, Scheduler &scheduler);

        void handle(const Request &request, SendResponse send_response) const;

    private:
        NodeRegistry &registry_;
        Scheduler &scheduler_;
    };
}

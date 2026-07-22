#include "api/handlers.hpp"

#include "api/serialization.hpp"
#include "api/validation.hpp"

#include <boost/json.hpp>

#include <utility>

namespace proxy_scheduler::api
{
    namespace json = boost::json;

    namespace
    {
        http::status validation_status(ValidationErrorKind kind)
        {
            switch (kind)
            {
            case ValidationErrorKind::BadRequest: return http::status::bad_request;
            case ValidationErrorKind::UnprocessableEntity: return http::status::unprocessable_entity;
            case ValidationErrorKind::UnsupportedMediaType: return http::status::unsupported_media_type;
            }
            return http::status::bad_request;
        }

        template <typename T>
        bool send_validation_error(
            const ValidationResult<T> &result,
            unsigned version,
            const ApiHandler::SendResponse &send_response)
        {
            const auto *error = std::get_if<ValidationError>(&result);
            if (error == nullptr) return false;
            send_response(make_error_response(version, validation_status(error->kind), error->message));
            return true;
        }

        json::object status_body(const char *status, NodeId node_id)
        {
            json::object body;
            body["status"] = status;
            body["node_id"] = node_id;
            return body;
        }

        bool requires_json(http::verb method, std::string_view target)
        {
            return (method == http::verb::post &&
                    (target == "/nodes/register" || target == "/nodes/heartbeat" ||
                     target == "/nodes/drain" || target == "/schedule")) ||
                   (method == http::verb::delete_ && target == "/nodes/remove");
        }
    }

    ApiHandler::ApiHandler(NodeRegistry &registry, Scheduler &scheduler)
        : registry_(registry), scheduler_(scheduler)
    {
    }

    void ApiHandler::handle(const Request &request, SendResponse send_response) const
    {
        const auto version = request.version();
        const auto method = request.method();
        const std::string target(request.target());

        if (requires_json(method, target))
        {
            const auto content_type = request[http::field::content_type];
            auto validation = validate_json_content_type(
                std::string_view(content_type.data(), content_type.size()));
            if (send_validation_error(validation, version, send_response)) return;
        }

        if (method == http::verb::get && target == "/health")
        {
            json::object body;
            body["status"] = "ok";
            send_response(make_json_response(version, http::status::ok, std::move(body)));
            return;
        }

        if (method == http::verb::post && target == "/nodes/register")
        {
            auto parsed = parse_registration(request.body());
            if (send_validation_error(parsed, version, send_response)) return;
            const auto node_id = std::get<Node>(parsed).node_id;
            registry_.register_node(
                std::get<Node>(std::move(parsed)),
                [version, node_id, send_response = std::move(send_response)](MutationResult result)
                {
                    if (result == MutationResult::AlreadyExists)
                    {
                        send_response(make_error_response(
                            version, http::status::conflict, "node is already registered"));
                        return;
                    }
                    send_response(make_json_response(
                        version, http::status::created, status_body("registered", node_id)));
                });
            return;
        }

        if (method == http::verb::post && target == "/nodes/heartbeat")
        {
            auto parsed = parse_heartbeat(request.body());
            if (send_validation_error(parsed, version, send_response)) return;
            const auto heartbeat = std::get<HeartbeatInput>(parsed);
            registry_.update_heartbeat(
                heartbeat.node_id, heartbeat.current_load, heartbeat.max_capacity, heartbeat.latency_ms,
                [version, node_id = heartbeat.node_id,
                 send_response = std::move(send_response)](MutationResult result)
                {
                    if (result == MutationResult::NotFound)
                    {
                        send_response(make_error_response(
                            version, http::status::not_found, "node not found"));
                        return;
                    }
                    send_response(make_json_response(
                        version, http::status::ok, status_body("heartbeat_received", node_id)));
                });
            return;
        }

        if (method == http::verb::get && target == "/nodes")
        {
            registry_.get_all_nodes(
                [version, send_response = std::move(send_response)](std::vector<Node> nodes)
                {
                    json::object body;
                    body["nodes"] = serialize_nodes(nodes);
                    send_response(make_json_response(version, http::status::ok, std::move(body)));
                });
            return;
        }

        if (method == http::verb::get && target == "/healthy-nodes")
        {
            registry_.get_healthy_nodes(
                [version, send_response = std::move(send_response)](std::vector<Node> nodes)
                {
                    json::object body;
                    body["nodes"] = serialize_nodes(nodes);
                    send_response(make_json_response(version, http::status::ok, std::move(body)));
                });
            return;
        }

        if (method == http::verb::post && target == "/schedule")
        {
            auto parsed = parse_scheduling_request(request.body());
            if (send_validation_error(parsed, version, send_response)) return;
            scheduler_.select_node(
                std::get<SchedulingRequest>(std::move(parsed)),
                [version, send_response = std::move(send_response)](
                    std::optional<SchedulingDecision> decision)
                {
                    if (!decision)
                    {
                        json::object body;
                        body["selected"] = false;
                        body["error"] = "no healthy node available";
                        send_response(make_json_response(
                            version, http::status::service_unavailable, std::move(body)));
                        return;
                    }

                    json::object body;
                    body["selected"] = true;
                    body["score"] = decision->score;
                    body["node"] = serialize_node(decision->node, false);
                    send_response(make_json_response(version, http::status::ok, std::move(body)));
                });
            return;
        }

        if (method == http::verb::post && target == "/nodes/drain")
        {
            auto parsed = parse_node_id(request.body());
            if (send_validation_error(parsed, version, send_response)) return;
            const auto node_id = std::get<NodeId>(parsed);
            registry_.set_node_draining(
                node_id,
                [version, node_id, send_response = std::move(send_response)](MutationResult result)
                {
                    if (result == MutationResult::NotFound)
                    {
                        send_response(make_error_response(
                            version, http::status::not_found, "node not found"));
                        return;
                    }
                    send_response(make_json_response(
                        version, http::status::ok, status_body("node_draining", node_id)));
                });
            return;
        }

        if (method == http::verb::delete_ && target == "/nodes/remove")
        {
            auto parsed = parse_node_id(request.body());
            if (send_validation_error(parsed, version, send_response)) return;
            registry_.remove_node(
                std::get<NodeId>(parsed),
                [version, send_response = std::move(send_response)](MutationResult result)
                {
                    if (result == MutationResult::NotFound)
                    {
                        send_response(make_error_response(
                            version, http::status::not_found, "node not found"));
                        return;
                    }
                    send_response(make_empty_response(version, http::status::no_content));
                });
            return;
        }

        send_response(make_error_response(version, http::status::not_found, "not found"));
    }
}

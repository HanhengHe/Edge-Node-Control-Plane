#pragma once

#include "node.hpp"
#include "scheduler.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace proxy_scheduler::api
{
    enum class ValidationErrorKind
    {
        BadRequest,
        UnprocessableEntity,
        UnsupportedMediaType
    };

    struct ValidationError
    {
        ValidationErrorKind kind;
        std::string message;
    };

    struct HeartbeatInput
    {
        NodeId node_id;
        std::uint32_t current_load;
        std::uint32_t max_capacity;
        std::uint32_t latency_ms;
    };

    template <typename T>
    using ValidationResult = std::variant<T, ValidationError>;

    ValidationResult<std::monostate> validate_json_content_type(std::string_view content_type);
    ValidationResult<Node> parse_registration(std::string_view body);
    ValidationResult<HeartbeatInput> parse_heartbeat(std::string_view body);
    ValidationResult<NodeId> parse_node_id(std::string_view body);
    ValidationResult<SchedulingRequest> parse_scheduling_request(std::string_view body);
}

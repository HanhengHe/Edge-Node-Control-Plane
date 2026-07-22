#include "api/validation.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <cctype>
#include <limits>
#include <optional>

namespace proxy_scheduler::api
{
    namespace json = boost::json;

    namespace
    {
        using ObjectResult = ValidationResult<json::object>;

        ValidationError bad_request(std::string message)
        {
            return {ValidationErrorKind::BadRequest, std::move(message)};
        }

        ValidationError invalid_value(std::string message)
        {
            return {ValidationErrorKind::UnprocessableEntity, std::move(message)};
        }

        ObjectResult parse_object(std::string_view body, bool allow_empty = false)
        {
            if (body.empty() && allow_empty)
            {
                return json::object{};
            }

            try
            {
                auto parsed = json::parse(body);
                if (!parsed.is_object())
                {
                    return bad_request("request body must be a json object");
                }
                return parsed.as_object();
            }
            catch (const std::exception &)
            {
                return bad_request("invalid json");
            }
        }

        ValidationResult<std::uint64_t> required_unsigned(
            const json::object &object,
            std::string_view name,
            std::uint64_t minimum,
            std::uint64_t maximum)
        {
            const auto *value = object.if_contains(name);
            if (value == nullptr || (!value->is_int64() && !value->is_uint64()))
            {
                return bad_request("missing or invalid " + std::string(name) + " field");
            }

            std::uint64_t raw{};
            if (value->is_int64())
            {
                const auto signed_raw = value->as_int64();
                if (signed_raw < 0)
                {
                    return invalid_value(std::string(name) + " out of range");
                }
                raw = static_cast<std::uint64_t>(signed_raw);
            }
            else
            {
                raw = value->as_uint64();
            }

            if (raw < minimum || raw > maximum)
            {
                return invalid_value(std::string(name) + " out of range");
            }
            return raw;
        }

        ValidationResult<std::string> required_string(
            const json::object &object,
            std::string_view name,
            std::size_t maximum_length,
            bool (*valid_character)(unsigned char))
        {
            const auto *value = object.if_contains(name);
            if (value == nullptr || !value->is_string())
            {
                return bad_request("missing or invalid " + std::string(name) + " field");
            }

            std::string text(value->as_string());
            if (text.empty() || text.size() > maximum_length ||
                !std::all_of(text.begin(), text.end(), [valid_character](unsigned char character)
                             { return valid_character(character); }))
            {
                return invalid_value(std::string(name) + " contains an invalid value");
            }
            return text;
        }

        bool region_character(unsigned char character)
        {
            return std::isalnum(character) != 0 || character == '-' || character == '_';
        }

        bool host_character(unsigned char character)
        {
            return std::isalnum(character) != 0 || character == '-' || character == '_' ||
                   character == '.' || character == ':';
        }

        template <typename T>
        const ValidationError *error_from(const ValidationResult<T> &result)
        {
            return std::get_if<ValidationError>(&result);
        }
    }

    ValidationResult<std::monostate> validate_json_content_type(std::string_view content_type)
    {
        auto delimiter = content_type.find(';');
        auto media_type = content_type.substr(0, delimiter);
        while (!media_type.empty() && std::isspace(static_cast<unsigned char>(media_type.front())))
        {
            media_type.remove_prefix(1);
        }
        while (!media_type.empty() && std::isspace(static_cast<unsigned char>(media_type.back())))
        {
            media_type.remove_suffix(1);
        }

        std::string normalized(media_type);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (normalized != "application/json")
        {
            return ValidationError{ValidationErrorKind::UnsupportedMediaType,
                                   "Content-Type must be application/json"};
        }
        return std::monostate{};
    }

    ValidationResult<Node> parse_registration(std::string_view body)
    {
        auto object_result = parse_object(body);
        if (const auto *error = error_from(object_result)) return *error;
        const auto &object = std::get<json::object>(object_result);

        auto node_id = required_unsigned(object, "node_id", 0, std::numeric_limits<NodeId>::max());
        if (const auto *error = error_from(node_id)) return *error;
        auto region = required_string(object, "region", 64, region_character);
        if (const auto *error = error_from(region)) return *error;
        auto host = required_string(object, "host", 255, host_character);
        if (const auto *error = error_from(host)) return *error;
        auto port = required_unsigned(object, "port", 1, std::numeric_limits<std::uint16_t>::max());
        if (const auto *error = error_from(port)) return *error;
        auto current_load = required_unsigned(object, "current_load", 0, std::numeric_limits<std::uint32_t>::max());
        if (const auto *error = error_from(current_load)) return *error;
        auto max_capacity = required_unsigned(object, "max_capacity", 1, std::numeric_limits<std::uint32_t>::max());
        if (const auto *error = error_from(max_capacity)) return *error;
        auto latency_ms = required_unsigned(object, "latency_ms", 0, std::numeric_limits<std::uint32_t>::max());
        if (const auto *error = error_from(latency_ms)) return *error;
        if (std::get<std::uint64_t>(current_load) > std::get<std::uint64_t>(max_capacity))
        {
            return invalid_value("current_load must not exceed max_capacity");
        }

        Node node;
        node.node_id = static_cast<NodeId>(std::get<std::uint64_t>(node_id));
        node.region = std::get<std::string>(std::move(region));
        node.host = std::get<std::string>(std::move(host));
        node.port = static_cast<std::uint16_t>(std::get<std::uint64_t>(port));
        node.current_load = static_cast<std::uint32_t>(std::get<std::uint64_t>(current_load));
        node.max_capacity = static_cast<std::uint32_t>(std::get<std::uint64_t>(max_capacity));
        node.latency_ms = static_cast<std::uint32_t>(std::get<std::uint64_t>(latency_ms));
        return node;
    }

    ValidationResult<HeartbeatInput> parse_heartbeat(std::string_view body)
    {
        auto object_result = parse_object(body);
        if (const auto *error = error_from(object_result)) return *error;
        const auto &object = std::get<json::object>(object_result);

        auto node_id = required_unsigned(object, "node_id", 0, std::numeric_limits<NodeId>::max());
        if (const auto *error = error_from(node_id)) return *error;
        auto current_load = required_unsigned(object, "current_load", 0, std::numeric_limits<std::uint32_t>::max());
        if (const auto *error = error_from(current_load)) return *error;
        auto max_capacity = required_unsigned(object, "max_capacity", 1, std::numeric_limits<std::uint32_t>::max());
        if (const auto *error = error_from(max_capacity)) return *error;
        auto latency_ms = required_unsigned(object, "latency_ms", 0, std::numeric_limits<std::uint32_t>::max());
        if (const auto *error = error_from(latency_ms)) return *error;
        if (std::get<std::uint64_t>(current_load) > std::get<std::uint64_t>(max_capacity))
        {
            return invalid_value("current_load must not exceed max_capacity");
        }

        return HeartbeatInput{
            static_cast<NodeId>(std::get<std::uint64_t>(node_id)),
            static_cast<std::uint32_t>(std::get<std::uint64_t>(current_load)),
            static_cast<std::uint32_t>(std::get<std::uint64_t>(max_capacity)),
            static_cast<std::uint32_t>(std::get<std::uint64_t>(latency_ms))};
    }

    ValidationResult<NodeId> parse_node_id(std::string_view body)
    {
        auto object_result = parse_object(body);
        if (const auto *error = error_from(object_result)) return *error;
        auto node_id = required_unsigned(std::get<json::object>(object_result), "node_id", 0,
                                         std::numeric_limits<NodeId>::max());
        if (const auto *error = error_from(node_id)) return *error;
        return static_cast<NodeId>(std::get<std::uint64_t>(node_id));
    }

    ValidationResult<SchedulingRequest> parse_scheduling_request(std::string_view body)
    {
        auto object_result = parse_object(body, true);
        if (const auto *error = error_from(object_result)) return *error;
        const auto &object = std::get<json::object>(object_result);
        SchedulingRequest request;

        if (const auto *value = object.if_contains("preferred_region"))
        {
            if (!value->is_string()) return bad_request("preferred_region must be string");
            std::string region(value->as_string());
            if (region.empty() || region.size() > 64 ||
                !std::all_of(region.begin(), region.end(), region_character))
            {
                return invalid_value("preferred_region contains an invalid value");
            }
            request.preferred_region = std::move(region);
        }

        if (object.if_contains("max_latency_ms"))
        {
            auto max_latency = required_unsigned(object, "max_latency_ms", 0,
                                                  std::numeric_limits<std::uint32_t>::max());
            if (const auto *error = error_from(max_latency)) return *error;
            request.max_latency_ms = static_cast<std::uint32_t>(std::get<std::uint64_t>(max_latency));
        }

        if (const auto *value = object.if_contains("allow_cross_region"))
        {
            if (!value->is_bool()) return bad_request("allow_cross_region must be bool");
            request.allow_cross_region = value->as_bool();
        }
        return request;
    }
}

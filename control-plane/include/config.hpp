#pragma once

#include <chrono>
#include <cstdint>

namespace proxy_scheduler::config
{
    inline constexpr std::uint16_t control_plane_port = 8080;

    inline constexpr auto heartbeat_interval =
        std::chrono::seconds{5};

    inline constexpr auto heartbeat_timeout =
        std::chrono::seconds{15};

    inline constexpr std::uint32_t default_max_capacity = 100;
}

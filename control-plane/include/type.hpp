#pragma once

#include <cstdint>
#include <string>

namespace proxy_scheduler
{
    using NodeId = std::uint32_t;

    enum class NodeStatus
    {
        Healthy,
        Unhealthy,
        Draining
    };

}

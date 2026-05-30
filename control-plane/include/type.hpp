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

    // unused for now, but can be extended in the future to support more complex routing policies
    enum class RoutePolicy
    {
        LeastLoad,
        LowestLatency,
        RegionPreferred
    };
}
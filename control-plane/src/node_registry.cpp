#include "node_registry.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace proxy_scheduler
{
    NodeRegistry::NodeRegistry(boost::asio::any_io_executor ex)
        : strand_(boost::asio::make_strand(ex))
    {
    }

    void NodeRegistry::register_node(Node node)
    {
        boost::asio::post(
            strand_,
            [self = shared_from_this(),
             node = std::move(node)]() mutable
            {
                node.last_heartbeat = std::chrono::steady_clock::now();
                node.status = NodeStatus::Healthy;

                self->nodes_[node.node_id] = std::move(node);
            });
    }

    void NodeRegistry::remove_node(NodeId node_id)
    {
        boost::asio::post(
            strand_,
            [self = shared_from_this(), node_id]()
            {
                self->nodes_.erase(node_id);
            });
    }

    void NodeRegistry::update_heartbeat(NodeId node_id)
    {
        boost::asio::post(
            strand_,
            [self = shared_from_this(), node_id]()
            {
                auto it = self->nodes_.find(node_id);
                if (it == self->nodes_.end())
                {
                    return;
                }

                it->second.last_heartbeat = std::chrono::steady_clock::now();

                if (it->second.status == NodeStatus::Unhealthy)
                {
                    it->second.status = NodeStatus::Healthy;
                }
            });
    }

    void NodeRegistry::update_metrics(NodeId node_id, std::uint32_t current_load, std::uint32_t max_capacity, std::uint32_t latency_ms)
    {
        boost::asio::post(
            strand_,
            [self = shared_from_this(),
             node_id, current_load, max_capacity, latency_ms]()
            {
                auto it = self->nodes_.find(node_id);
                if (it == self->nodes_.end())
                {
                    return;
                }

                it->second.current_load = current_load;
                it->second.max_capacity = max_capacity;
                it->second.latency_ms = latency_ms;
            });
    }

    void NodeRegistry::set_status(NodeId node_id, NodeStatus status)
    {
        boost::asio::post(
            strand_,
            [self = shared_from_this(), node_id, status]
            {
                auto it = self->nodes_.find(node_id);
                if (it == self->nodes_.end())
                {
                    return;
                }

                it->second.status = status;
            });
    }

    void NodeRegistry::get_node(NodeId node_id, std::function<void(std::optional<Node>)> callback)
    {
        boost::asio::post(
            strand_,
            [self = shared_from_this(), node_id, callback = std::move(callback)]
            {
                auto it = self->nodes_.find(node_id);
                if (it == self->nodes_.end())
                {
                    callback(std::nullopt);
                    return;
                }

                callback(it->second);
            });
    }

    void NodeRegistry::get_all_nodes(
        std::function<void(std::vector<Node>)> callback)
    {
        boost::asio::post(
            strand_,
            [self = shared_from_this(), callback = std::move(callback)]
            {
                std::vector<Node> result;
                result.reserve(self->nodes_.size());

                for (const auto &[id, node] : self->nodes_)
                {
                    result.push_back(node);
                }

                callback(std::move(result));
            });
    }

    void NodeRegistry::get_healthy_nodes(
        std::function<void(std::vector<Node>)> callback)
    {
        boost::asio::post(
            strand_,
            [self = shared_from_this(), callback = std::move(callback)]
            {
                std::vector<Node> result;
                result.reserve(self->nodes_.size());

                for (const auto &[id, node] : self->nodes_)
                {
                    if (node.status == NodeStatus::Healthy)
                    {
                        result.push_back(node);
                    }
                }

                callback(std::move(result));
            });
    }

    void NodeRegistry::mark_expired_nodes(std::chrono::seconds timeout)
    {
        boost::asio::post(
            strand_,
            [self = shared_from_this(), timeout]
            {
                const auto now = std::chrono::steady_clock::now();
                for (auto &[id, node] : self->nodes_)
                {
                    const auto elapsed = now - node.last_heartbeat;
                    if (elapsed > timeout && node.status != NodeStatus::Draining)
                    {
                        node.status = NodeStatus::Unhealthy;
                    }
                }
            });
    }

    void NodeRegistry::size(std::function<void(size_t)> callback)
    {
        boost::asio::post(
            strand_,
            [self = shared_from_this(),
             callback = std::move(callback)]
            {
                callback(self->nodes_.size());
            });
    }
}
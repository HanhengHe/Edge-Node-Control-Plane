#pragma once

#include "node.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace proxy_scheduler
{
    class NodeRegistry : public std::enable_shared_from_this<NodeRegistry>
    {
        using Executor = boost::asio::any_io_executor;
        using Strand = boost::asio::strand<Executor>;

    public:
        explicit NodeRegistry(boost::asio::any_io_executor ex);

        // CRUD
        void register_node(Node node);
        void remove_node(NodeId node_id);

        // status
        void update_heartbeat(NodeId node_id);
        void update_metrics(NodeId ode_id, std::uint32_t current_load, std::uint32_t max_capacity, std::uint32_t latency_ms);
        void set_status(NodeId node_id, NodeStatus status);
        void set_node_draining(NodeId node_id);
        void try_remove_drained_node(NodeId node_id);
        void force_remove_node(NodeId node_id);

        // getters
        void get_node(NodeId node_id, std::function<void(std::optional<Node>)> callback);
        void get_all_nodes(
            std::function<void(std::vector<Node> const &)> callback);
        void get_healthy_nodes(
            std::function<void(std::vector<Node> const &)> callback);
        void mark_expired_nodes(std::chrono::seconds timeout);
        void size(std::function<void(size_t)> callback);

    private:
        Strand strand_;
        std::unordered_map<NodeId, Node> nodes_;
    };
}
# Edge Node Control Plane

A C++ asynchronous control plane for managing distributed edge nodes.

This project demonstrates the control-plane side of a distributed edge infrastructure system. A central C++ control plane tracks node registration, heartbeat updates, node health, load, latency, draining state, and scheduling decisions. Independent Go node agents run as separate processes/containers and periodically report their status to the control plane.

The project focuses on control-plane logic rather than real data-plane traffic forwarding.

## Features

- C++ asynchronous HTTP control plane using Boost.Asio and Boost.Beast
- JSON HTTP APIs for node registration, heartbeat reporting, node listing, draining, removal, and scheduling
- Strand-based in-memory node registry to serialize concurrent registry updates
- Heartbeat-based failure detection for stale nodes
- Load/latency-aware node scheduling with preferred-region support
- Go-based node agents that register and periodically report heartbeat metrics
- Docker Compose multi-node simulation with one control plane and multiple node agents

## Architecture

```text
+------------------+        register / heartbeat / drain
|   Node Agent 1   | ------------------------------------+
+------------------+                                     |
                                                         |
+------------------+       register / heartbeat / drain  |
|   Node Agent 2   | ------------------------------------+----> +----------------------+
+------------------+                                     |      |     Control Plane    |
                                                         |      |                      |
+------------------+       register / heartbeat / drain  |      | Node Registry        |
|   Node Agent 3   | ------------------------------------+      | Health Checker       |
+------------------+                                            | Scheduler            |
                                                                +----------+-----------+
                                                                            |
                                                                            | /schedule
                                                                            v
                                                                 Routing / node decision
```

## Components

### Control Plane

Located in `control-plane/`.

The control plane is a C++ asynchronous HTTP server built with Boost.Asio, Boost.Beast, and Boost.JSON. It owns the node registry, performs heartbeat-based health checks, and selects the best healthy node for scheduling requests.

Key responsibilities:

- Accept node registrations
- Track node status, load, capacity, region, and latency
- Mark stale nodes as unhealthy when heartbeat timeout expires
- Support graceful draining
- Return load/latency-aware scheduling decisions

### Node Agent

Located in `node-agent/`.

Each node agent runs independently and communicates with the control plane over HTTP. On startup, it registers itself with the control plane. It then periodically sends heartbeat updates containing node load, capacity, and latency metrics.

For demo purposes, `latency_ms` is measured as the agent-to-control-plane HTTP round-trip time. In a production system, this could be replaced with region probes, upstream probes, or real data-plane latency measurements.

## API Overview

### Health Check

```http
GET /health
```

Returns control-plane health status.

### Register Node

```http
POST /nodes/register
```

Example request:

```json
{
  "node_id": 1,
  "region": "ca-central",
  "host": "agent-1",
  "port": 9001,
  "current_load": 30,
  "max_capacity": 100,
  "latency_ms": 20
}
```

Example response:

```json
{
  "status": "registered",
  "node_id": 1
}
```

### Heartbeat

```http
POST /nodes/heartbeat
```

Example request:

```json
{
  "node_id": 1,
  "current_load": 30,
  "max_capacity": 100,
  "latency_ms": 25
}
```

Example response:

```json
{
  "status": "heartbeat_received",
  "node_id": 1
}
```

### List Nodes

```http
GET /nodes
```

Returns all known nodes and their current status.

Example response:

```json
{
  "nodes": [
    {
      "node_id": 1,
      "region": "ca-central",
      "host": "agent-1",
      "port": 9001,
      "current_load": 30,
      "max_capacity": 100,
      "latency_ms": 25,
      "status": "Healthy"
    }
  ]
}
```

### List Healthy Nodes

```http
GET /healthy-nodes
```

Returns only nodes currently marked as healthy.

### Schedule Node

```http
POST /schedule
```

Example request:

```json
{
  "preferred_region": "ca-central",
  "max_latency_ms": 100,
  "allow_cross_region": true
}
```

Example response:

```json
{
  "selected": true,
  "score": 188.5,
  "node": {
    "node_id": 1,
    "region": "ca-central",
    "host": "agent-1",
    "port": 9001,
    "current_load": 30,
    "max_capacity": 100,
    "latency_ms": 25
  }
}
```

If no healthy node is available:

```json
{
  "selected": false,
  "error": "no healthy node available"
}
```

### Drain Node

```http
POST /nodes/drain
```

Marks a node as draining so it is excluded from future scheduling decisions.

Example request:

```json
{
  "node_id": 1
}
```

Example response:

```json
{
  "status": "node_draining",
  "node_id": 1
}
```

### Remove Node

```http
DELETE /nodes/remove
```

Force-removes a node from the registry.

Example request:

```json
{
  "node_id": 1
}
```

Example response:

```json
{
  "status": "node_removed",
  "node_id": 1
}
```

## Scheduling Policy

The scheduler filters out nodes that are not eligible:

- Node is not healthy
- Node has zero capacity
- Node is at or above max capacity
- Node exceeds the request's max latency
- Cross-region routing is disabled and region does not match

Eligible nodes are scored using:

```text
score = latency_score + load_score + preferred_region_bonus
```

Where:

- Lower latency increases the score
- Lower load ratio increases the score
- Matching the preferred region adds a bonus

## Run with Docker Compose

From the repository root:

```bash
docker compose up --build
```

This starts:

- `control-plane` on port `8080`
- `agent-1` in `ca-central`
- `agent-2` in `us-east`
- `agent-3` in `us-west`

The agents automatically register with the control plane and start sending heartbeat updates.

## Manual Testing

In another terminal:

```bash
curl http://localhost:8080/health
```

List all nodes:

```bash
curl http://localhost:8080/nodes
```

List healthy nodes:

```bash
curl http://localhost:8080/healthy-nodes
```

Request a scheduling decision:

```bash
curl -X POST http://localhost:8080/schedule \
  -H "Content-Type: application/json" \
  -d '{"preferred_region":"ca-central","max_latency_ms":100,"allow_cross_region":true}'
```

## Failure Detection Demo

Start the system:

```bash
docker compose up --build
```

Stop one node agent:

```bash
docker compose stop agent-2
```

After the heartbeat timeout expires, the control plane marks that node as unhealthy. It will no longer be returned by `/healthy-nodes` or selected by `/schedule`.

Check node status:

```bash
curl http://localhost:8080/nodes
```

Request scheduling again:

```bash
curl -X POST http://localhost:8080/schedule \
  -H "Content-Type: application/json" \
  -d '{"preferred_region":"us-east","max_latency_ms":100,"allow_cross_region":true}'
```

## Graceful Draining Demo

When a node agent receives a shutdown signal, it attempts to call:

```http
POST /nodes/drain
```

The control plane marks the node as `Draining`, which excludes it from scheduling decisions. This models graceful node removal before shutdown.

Example:

```bash
docker compose stop agent-1
curl http://localhost:8080/nodes
```

## Repository Layout

```text
.
├── control-plane/
│   ├── include/
│   ├── src/
│   ├── CMakeLists.txt
│   └── Dockerfile
├── node-agent/
│   ├── main.go
│   ├── node_agent.go
│   ├── client.go
│   ├── config.go
│   ├── types.go
│   ├── go.mod
│   └── Dockerfile
├── docker-compose.yml
└── README.md
```

## Tech Stack

- C++
- Boost.Asio
- Boost.Beast
- Boost.JSON
- Go
- Docker
- Docker Compose
- CMake
- Ninja

## Project Scope

This project implements a distributed edge-node control plane demo.

It does not implement:

- Full data-plane traffic forwarding
- TCP proxying
- TLS termination
- Persistent storage
- Multi-control-plane high availability
- Leader election
- Consensus

The intended focus is:

- Asynchronous C++ backend design
- Control-plane state management
- Node health tracking
- Failure detection
- Graceful draining
- Scheduling/routing decision logic
- Multi-process distributed-system simulation

## Future Improvements

- Refactor the control-plane HTTP server to use Boost.Asio coroutines
- Split HTTP request parsing/validation from handler logic
- Add automated integration tests for Docker Compose demo

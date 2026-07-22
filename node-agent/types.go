package main

type RegisterRequest struct {
	NodeID      uint32 `json:"node_id"`
	Region      string `json:"region"`
	Host        string `json:"host"`
	Port        uint16 `json:"port"`
	CurrentLoad uint32 `json:"current_load"`
	MaxCapacity uint32 `json:"max_capacity"`
	Latencyms   uint32 `json:"latency_ms"`
}

type HeartbeatRequest struct {
	NodeID      uint32 `json:"node_id"`
	CurrentLoad uint32 `json:"current_load"`
	MaxCapacity uint32 `json:"max_capacity"`
	LatencyMS   uint32 `json:"latency_ms"`
}

type ScheduleRequest struct {
	PreferredRegion  string `json:"preferred_region"`
	MaxLatencyMs     uint32 `json:"max_latency_ms"`
	AllowCrossRegion bool   `json:"allow_cross_region"`
}

type DrainRequest struct {
	NodeID uint32 `json:"node_id"`
}

type RemoveRequest struct {
	NodeID uint32 `json:"node_id"`
}

type RegisterResponse struct {
	Status string `json:"status"`
	NodeID uint32 `json:"node_id"`
}

type HeartbeatResponse struct {
	Status string `json:"status"`
	NodeID uint32 `json:"node_id"`
}

type ScheduleResponse struct {
	Selected bool          `json:"selected"`
	Score    float64       `json:"score,omitempty"`
	Node     *ScheduleNode `json:"node,omitempty"`
	Error    string        `json:"error,omitempty"`
}

type ScheduleNode struct {
	NodeID      uint32 `json:"node_id"`
	Region      string `json:"region"`
	Host        string `json:"host"`
	Port        uint16 `json:"port"`
	CurrentLoad uint32 `json:"current_load"`
	MaxCapacity uint32 `json:"max_capacity"`
	LatencyMS   uint32 `json:"latency_ms"`
}

type DrainResponse struct {
	Status string `json:"status"`
	NodeID uint32 `json:"node_id"`
}

type RemoveResponse struct {
	Status string `json:"status"`
	NodeID uint32 `json:"node_id"`
}

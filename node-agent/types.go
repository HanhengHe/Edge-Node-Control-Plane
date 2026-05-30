package main

type RegisterRequest struct {
	NodeID       uint32 `json:"node_id"`
	Region       string `json:"region"`
	Host         string `json:"host"`
	Port         uint16 `json:"port"`
	CurrentLoad  uint32 `json:"current_load"`
	MaxCapacity  uint32 `json:"max_capacity"`
	Latencyms    uint32 `json:"latency_ms"`
}

type HeartbeatRequest struct {
	NodeID      uint32 `json:"node_id"`
	CurrentLoad uint32 `json:"current_load"`
	MaxCapacity uint32 `json:"max_capacity"`
	Latencyms   uint32 `json:"latency_ms"`
}

type ScheduleRequest struct {
	PreferredRegion string `json:"preferred_region"`
	MaxLatencyMs    uint32 `json:"max_latency_ms"`
	AllowCrossRegion bool  `json:"allow_cross_region"`
}

type DrainRequest struct {
	NodeID uint32 `json:"node_id"`
}

type RemoveRequest struct {
	NodeID uint32 `json:"node_id"`
}
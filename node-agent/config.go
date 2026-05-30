package main

import (
	"fmt"
	"os"
	"strconv"
	"time"
)

type Config struct {
	NodeID         		uint32
	Region         		string
	Host           		string
	Port           		uint16
	MaxCapacity    		uint32
	ControlPlaneURL 	string
	HeartbeatInterval 	time.Duration
	RequestTimeout    	time.Duration
}

func LoadConfig() (*Config, error) {
	// Load configuration from environment variables

	nodeIDStr := os.Getenv("NODE_ID")
	nodeID64, err := strconv.ParseUint(nodeIDStr, 10, 32)
	if err != nil {
		return nil, fmt.Errorf("invalid NODE_ID: %v", err)
	}
	nodeID := uint32(nodeID64)

	region := os.Getenv("REGION", "ca-central")
	host := os.Getenv("HOST", "127.0.0.1")

	port, err := getUint16Env("PORT", 9001)
	if err != nil {
		return nil, fmt.Errorf("invalid PORT: %v", err)
	}

	maxCapacity, err := getUint32Env("MAX_CAPACITY", 100)
	if err != nil {
		return nil, fmt.Errorf("invalid MAX_CAPACITY: %v", err)
	}

	heartbeatInterval, err := getDurationEnv("HEARTBEAT_INTERVAL", 3*time.Second)
	if err != nil {
		return nil, fmt.Errorf("invalid HEARTBEAT_INTERVAL: %v", err)
	}

	requestTimeout, err := getDurationEnv("REQUEST_TIMEOUT", 2*time.Second)
	if err != nil {
		return nil, fmt.Errorf("invalid REQUEST_TIMEOUT: %v", err)
	}
	controlPlaneURL := os.Getenv("CONTROL_PLANE_URL", "http://localhost:8080")

	return &Config{
		NodeID:            	nodeID,
		Region:             region,
		Host:               host,
		Port:               port,
		MaxCapacity:        maxCapacity,
		ControlPlaneURL:    controlPlaneURL,
		HeartbeatInterval:  heartbeatInterval,
		RequestTimeout:     requestTimeout,
	}, nil
}
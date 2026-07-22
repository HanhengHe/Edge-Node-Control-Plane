package main

import (
	"fmt"
	"os"
	"strconv"
	"time"
)

type Config struct {
	NodeID            uint32
	Region            string
	Host              string
	Port              uint16
	CurrentLoad       uint32
	MaxCapacity       uint32
	ControlPlaneURL   string
	HeartbeatInterval time.Duration
	RequestTimeout    time.Duration
	ShutdownTimeout   time.Duration
}

func LoadConfig() (*Config, error) {
	// Load configuration from environment variables

	nodeIDStr := os.Getenv("NODE_ID")
	nodeID64, err := strconv.ParseUint(nodeIDStr, 10, 32)
	if err != nil {
		return nil, fmt.Errorf("invalid NODE_ID: %v", err)
	}
	nodeID := uint32(nodeID64)

	region := getenv("REGION", "ca-central")
	host := getenv("HOST", "127.0.0.1")

	port, err := getUint16Env("PORT", 9001)
	if err != nil {
		return nil, fmt.Errorf("invalid PORT: %v", err)
	}
	if port == 0 {
		return nil, fmt.Errorf("invalid PORT: must be greater than zero")
	}

	currentLoad, err := getUint32Env("CURRENT_LOAD", 0)
	if err != nil {
		return nil, fmt.Errorf("invalid CURRENT_LOAD: %v", err)
	}

	maxCapacity, err := getUint32Env("MAX_CAPACITY", 100)
	if err != nil {
		return nil, fmt.Errorf("invalid MAX_CAPACITY: %v", err)
	}
	if maxCapacity == 0 || currentLoad > maxCapacity {
		return nil, fmt.Errorf("invalid capacity: require 0 <= CURRENT_LOAD <= MAX_CAPACITY and MAX_CAPACITY > 0")
	}

	heartbeatInterval, err := getDurationEnv("HEARTBEAT_INTERVAL", 3*time.Second)
	if err != nil || heartbeatInterval <= 0 {
		return nil, fmt.Errorf("invalid HEARTBEAT_INTERVAL: must be a positive duration")
	}

	requestTimeout, err := getDurationEnv("REQUEST_TIMEOUT", 2*time.Second)
	if err != nil || requestTimeout <= 0 {
		return nil, fmt.Errorf("invalid REQUEST_TIMEOUT: must be a positive duration")
	}
	shutdownTimeout, err := getDurationEnv("SHUTDOWN_TIMEOUT", 5*time.Second)
	if err != nil || shutdownTimeout <= 0 {
		return nil, fmt.Errorf("invalid SHUTDOWN_TIMEOUT: must be a positive duration")
	}
	controlPlaneURL := getenv("CONTROL_PLANE_URL", "http://localhost:8080")

	return &Config{
		NodeID:            nodeID,
		Region:            region,
		Host:              host,
		Port:              port,
		CurrentLoad:       currentLoad,
		MaxCapacity:       maxCapacity,
		ControlPlaneURL:   controlPlaneURL,
		HeartbeatInterval: heartbeatInterval,
		RequestTimeout:    requestTimeout,
		ShutdownTimeout:   shutdownTimeout,
	}, nil
}

func getenv(key string, defaultValue string) string {
	value := os.Getenv(key)
	if value == "" {
		return defaultValue
	}

	return value
}

func getUint16Env(key string, defaultValue uint16) (uint16, error) {
	value := os.Getenv(key)
	if value == "" {
		return defaultValue, nil
	}

	parsed, err := strconv.ParseUint(value, 10, 16)
	if err != nil {
		return 0, err
	}

	return uint16(parsed), nil
}

func getUint32Env(key string, defaultValue uint32) (uint32, error) {
	value := os.Getenv(key)
	if value == "" {
		return defaultValue, nil
	}

	parsed, err := strconv.ParseUint(value, 10, 32)
	if err != nil {
		return 0, err
	}

	return uint32(parsed), nil
}

func getDurationEnv(key string, defaultValue time.Duration) (time.Duration, error) {
	value := os.Getenv(key)
	if value == "" {
		return defaultValue, nil
	}

	return time.ParseDuration(value)
}

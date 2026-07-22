package main

import (
	"context"
	"fmt"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"
)

func testConfig(baseURL string) *Config {
	return &Config{
		NodeID:            42,
		Region:            "ca-central",
		Host:              "agent-42",
		Port:              9042,
		CurrentLoad:       1,
		MaxCapacity:       10,
		ControlPlaneURL:   baseURL,
		HeartbeatInterval: time.Hour,
		RequestTimeout:    time.Second,
		ShutdownTimeout:   time.Second,
	}
}

func TestRunWaitsForDrainToComplete(t *testing.T) {
	agentRunning := make(chan struct{})
	drainStarted := make(chan struct{})
	releaseDrain := make(chan struct{})
	var runningOnce sync.Once

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		switch r.URL.Path {
		case "/nodes/register":
			w.WriteHeader(http.StatusCreated)
			fmt.Fprint(w, `{"status":"registered","node_id":42}`)
		case "/health":
			runningOnce.Do(func() { close(agentRunning) })
			fmt.Fprint(w, `{"status":"ok"}`)
		case "/nodes/drain":
			close(drainStarted)
			<-releaseDrain
			fmt.Fprint(w, `{"status":"node_draining","node_id":42}`)
		default:
			http.NotFound(w, r)
		}
	}))
	defer server.Close()

	config := testConfig(server.URL)
	config.HeartbeatInterval = time.Millisecond
	agent := &NodeAgent{config: config, client: NewClient(server.URL, config.RequestTimeout)}
	ctx, cancel := context.WithCancel(context.Background())
	result := make(chan error, 1)
	go func() { result <- agent.Run(ctx) }()

	<-agentRunning
	cancel()
	<-drainStarted
	select {
	case err := <-result:
		t.Fatalf("Run returned before drain completed: %v", err)
	case <-time.After(30 * time.Millisecond):
	}

	close(releaseDrain)
	if err := <-result; err != nil {
		t.Fatalf("Run returned an unexpected drain error: %v", err)
	}
}

func TestRunReportsDrainTimeout(t *testing.T) {
	agentRunning := make(chan struct{})
	releaseDrain := make(chan struct{})
	var runningOnce sync.Once
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		if r.URL.Path == "/nodes/register" {
			w.WriteHeader(http.StatusCreated)
			fmt.Fprint(w, `{"status":"registered","node_id":42}`)
			return
		}
		if r.URL.Path == "/health" {
			runningOnce.Do(func() { close(agentRunning) })
			fmt.Fprint(w, `{"status":"ok"}`)
			return
		}
		<-releaseDrain
	}))
	defer server.Close()

	config := testConfig(server.URL)
	config.HeartbeatInterval = time.Millisecond
	config.ShutdownTimeout = 30 * time.Millisecond
	agent := &NodeAgent{config: config, client: NewClient(server.URL, config.RequestTimeout)}
	ctx, cancel := context.WithCancel(context.Background())
	result := make(chan error, 1)
	go func() { result <- agent.Run(ctx) }()

	<-agentRunning
	cancel()
	if err := <-result; err == nil || !strings.Contains(err.Error(), "failed to drain node") {
		close(releaseDrain)
		t.Fatalf("expected a drain timeout, got %v", err)
	}
	close(releaseDrain)
}

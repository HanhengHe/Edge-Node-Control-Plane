package main

import (
	"context"
	"fmt"
	"time"
	"log"
)

type NodeAgent struct {
	config *Config
	client *Client
}

func (a *NodeAgent) Run(ctx context.Context) error {
	// Register with the control plane
	err := a.Register(ctx)
	if err != nil {
		return fmt.Errorf("failed to register node: %v", err)
	}

	// Start heartbeat loop
	go a.heartbeatLoop(ctx)

	<-ctx.Done()

	return nil
}

func (a *NodeAgent) Register(ctx context.Context) error {
	req := &RegisterRequest{
		NodeID:      a.config.NodeID,
		Region:      a.config.Region,
		Host:        a.config.Host,
		Port:        a.config.Port,
		MaxCapacity: a.config.MaxCapacity,
	}
	
	log.Printf(
		"starting node agent: node_id=%d region=%s host=%s port=%d control_plane=%s",
		a.config.NodeID,
		a.config.Region,
		a.config.Host,
		a.config.Port,
		a.config.ControlPlaneURL,
	)

	err := a.SendRequest(
		ctx,
		time.Second,
		10*time.Second,
		5,
		func(ctx context.Context) error {
			reqCtx, cancel := context.WithTimeout(ctx, a.config.RequestTimeout)
			defer cancel()
			_, err := a.client.Register(reqCtx, req)
			return err
		})
	if err != nil {
		return fmt.Errorf("failed to register with control plane: %v", err)
	}

	log.Printf("node registered successfully: node_id=%d", a.config.NodeID)

	return nil
}

func (a *NodeAgent) SendRequest(
	ctx context.Context,
	delay time.Duration,
	maxDelay time.Duration,
	maxRetries int,
	sender func(context.Context) error,
) error {
	for i := 0; i < maxRetries; i++ {
		err := sender(ctx)
		if err == nil {
			return nil
		}

		log.Printf("operation failed: %v; retrying in %sms", err, delay)
		timer := time.NewTimer(delay)
		select {
		case <-ctx.Done():
			timer.Stop()
			return ctx.Err()
		case <-timer.C:
			delay = time.Duration(float64(delay) * 2)
			if delay > maxDelay {
				delay = maxDelay
			}
		}
	}

	return fmt.Errorf("operation failed after %d retries", maxRetries)
}

func (a *NodeAgent) heartbeatLoop(ctx context.Context) {
	ticker := time.NewTicker(a.config.HeartbeatInterval)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			latencyCtx, cancel := context.WithTimeout(ctx, a.config.RequestTimeout)
			latency, err := a.client.GetLatency(latencyCtx, a.config.NodeID)
			cancel()

			if err != nil {
				log.Printf("failed to get latency: %v", err)
				latency = 9999
			}

			maxCapacity := a.config.MaxCapacity
			if maxCapacity == 0 {
				maxCapacity = 1
			}

			req := &HeartbeatRequest{
				NodeID:      a.config.NodeID,
				CurrentLoad: a.config.CurrentLoad,
				MaxCapacity: maxCapacity,
				LatencyMS:   latency,
			}

			requestCtx, cancel := context.WithTimeout(ctx, a.config.RequestTimeout)
			_, err = a.client.Heartbeat(requestCtx, req)
			cancel()

			if err != nil {
				log.Printf("failed to send heartbeat: %v", err)
				continue
			}

			log.Printf(
				"heartbeat sent: node_id=%d load=%d max_capacity=%d latency_ms=%d",
				req.NodeID, req.CurrentLoad, req.MaxCapacity, req.LatencyMS)
		case <-ctx.Done():
			log.Printf("shutdown signal received, draining node_id=%d", a.config.NodeID)
			drainCtx, cancel := context.WithTimeout(context.Background(), a.config.RequestTimeout)
			defer cancel()
			
			_, err := a.client.Drain(drainCtx, &DrainRequest{NodeID: a.config.NodeID}); 
			if err != nil {
				log.Printf("failed to drain node: %v", err)
			} else {
				log.Printf("node drained successfully: node_id=%d", a.config.NodeID)
			}

			return
		}
	}
}

package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"
)

type IClient interface {
	Register(ctx context.Context, req *RegisterRequest) (RegisterResponse, error)
	Heartbeat(ctx context.Context, req *HeartbeatRequest) (HeartbeatResponse, error)
	GetLatency(ctx context.Context, nodeID uint32) (uint32, error)
	Schedule(ctx context.Context, req *ScheduleRequest) (ScheduleResponse, error)
	Drain(ctx context.Context, req *DrainRequest) (DrainResponse, error)
	Remove(ctx context.Context, req *RemoveRequest) (RemoveResponse, error)
}

type Client struct {
	baseURL string
	httpClient *http.Client
}

func NewClient(baseURL string, timeout time.Duration) *Client {
	return &Client{
		baseURL: baseURL,
		httpClient: &http.Client{
			Timeout: timeout,
		},
	}
}

func (c *Client) Register(ctx context.Context, req *RegisterRequest) (RegisterResponse, error) {
	var resp RegisterResponse
	err := c.postJSON(ctx, "/nodes/register", req, &resp)
	if err != nil {
		return RegisterResponse{}, err
	}

	return resp, nil
}

func (c *Client) Heartbeat(ctx context.Context, req *HeartbeatRequest) (HeartbeatResponse, error) {
	var resp HeartbeatResponse
	err := c.postJSON(ctx, "/nodes/heartbeat", req, &resp)
	if err != nil {
		return HeartbeatResponse{}, err
	}

	return resp, nil
}

func (c *Client) GetLatency(ctx context.Context) (uint32, error) {
	url := c.baseURL + "/health"
	httpReq, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		return 0, fmt.Errorf("failed to create HTTP request: %v", err)
	}

	start := time.Now()
	resp, err := c.httpClient.Do(httpReq)
	if err != nil {
		return 0, fmt.Errorf("HTTP request failed: %v", err)
	}
	defer resp.Body.Close()
	latency := uint32(time.Since(start).Milliseconds())

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		bodyBytes, _ := io.ReadAll(io.LimitReader(resp.Body, 1024))
		return 0, fmt.Errorf("HTTP request failed with status %d: %s", resp.StatusCode, string(bodyBytes))
	}

	if latency < 1 {
		latency = 1
	}
	return latency, nil
}

func (c *Client) Schedule(ctx context.Context, req *ScheduleRequest) (ScheduleResponse, error) {
	var schedule ScheduleResponse

	err := c.postJSON(ctx, "/schedule", req, &schedule)
	if err != nil {
		return ScheduleResponse{}, err
	}

	return schedule, nil
}

func (c *Client) Drain(ctx context.Context, req *DrainRequest) (DrainResponse, error) {
	var resp DrainResponse
	err := c.postJSON(ctx, "/nodes/drain", req, &resp)
	if err != nil {
		return DrainResponse{}, err
	}

	return resp, nil
}

func (c *Client) Remove(ctx context.Context, req *RemoveRequest) (RemoveResponse, error) {
	var resp RemoveResponse
	err := c.deleteJSON(ctx, "/nodes/remove", req, &resp)
	if err != nil {
		return RemoveResponse{}, err
	}

	return resp, nil
}

func (c *Client) postJSON(ctx context.Context, path string, payload any, response any) error {
	url := c.baseURL + path
	jsonData, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("failed to marshal request: %v", err)
	}

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, url, bytes.NewBuffer(jsonData))
	if err != nil {
		return fmt.Errorf("failed to create HTTP request: %v", err)
	}

	httpReq.Header.Set("Content-Type", "application/json")

	resp, err := c.httpClient.Do(httpReq)
	if err != nil {
		return fmt.Errorf("HTTP request failed: %v", err)
	}
	defer resp.Body.Close()
	
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {	
		bodyBytes, _ := io.ReadAll(io.LimitReader(resp.Body, 1024))
		return fmt.Errorf("HTTP request failed with status %d: %s", resp.StatusCode, string(bodyBytes))
	}

	if response != nil {
		if err := json.NewDecoder(resp.Body).Decode(response); err != nil {
			return fmt.Errorf("failed to decode response body: %w", err)
		}
	}

	return nil
}

func (c *Client) deleteJSON(ctx context.Context, path string, payload any, response any) error {
	url := c.baseURL + path
	jsonData, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("failed to marshal request: %v", err)
	}

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodDelete, url, bytes.NewBuffer(jsonData))
	if err != nil {
		return fmt.Errorf("failed to create HTTP request: %v", err)
	}

	httpReq.Header.Set("Content-Type", "application/json")
	
	resp, err := c.httpClient.Do(httpReq)
	if err != nil {
		return fmt.Errorf("HTTP request failed: %v", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		bodyBytes, _ := io.ReadAll(io.LimitReader(resp.Body, 1024))
		return fmt.Errorf("HTTP request failed with status %d: %s", resp.StatusCode, string(bodyBytes))
	}

	if response != nil {
		if err := json.NewDecoder(resp.Body).Decode(response); err != nil {
			return fmt.Errorf("failed to decode response body: %w", err)
		}
	}

	return nil
}

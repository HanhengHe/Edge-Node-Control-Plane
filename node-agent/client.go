package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"
)

type IClient interface {
	Register(ctx context.Context, req *RegisterRequest) error
	Heartbeat(ctx context.Context, req *HeartbeatRequest) error
	Schedule(ctx context.Context, req *ScheduleRequest) (uint32, error)
	Drain(ctx context.Context, req *DrainRequest) error
	Remove(ctx context.Context, req *RemoveRequest) error
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

func (c *Client) Register(ctx context.Context, req *RegisterRequest) error {
	var resp RegisterResponse
	return c.postJSON(ctx, "/nodes/register", req, &resp)
}

func (c *Client) Heartbeat(ctx context.Context, req *HeartbeatRequest) error {
	var resp HeartbeatResponse
	return c.postJSON(ctx, "/nodes/heartbeat", req, &resp)
}

func (c *Client) Schedule(ctx context.Context, req *ScheduleRequest) (ScheduleResponse, error) {
	var schedule ScheduleResponse

	err := c.postJSON(ctx, "/schedule", req, &schedule)
	if err != nil {
		return ScheduleResponse{}, err
	}

	return schedule, nil
}

func (c *Client) Drain(ctx context.Context, req *DrainRequest) error {
	var resp DrainResponse
	return c.postJSON(ctx, "/nodes/drain", req, &resp)
}

func (c *Client) Remove(ctx context.Context, req *RemoveRequest) error {
	var resp RemoveResponse
	return c.postJSON(ctx, "/nodes/remove", req, &resp)
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
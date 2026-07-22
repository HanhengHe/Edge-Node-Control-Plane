package main

import (
	"context"
	"fmt"
	"os"
	"os/signal"
	"syscall"
)

func main() {
	config, err := LoadConfig()
	if err != nil {
		fmt.Fprintf(os.Stderr, "failed to load config: %v\n", err)
		os.Exit(1)
	}

	client := NewClient(config.ControlPlaneURL, config.RequestTimeout)

	agent := &NodeAgent{
		config: config,
		client: client,
	}

	ctx, stop := signal.NotifyContext(
		context.Background(),
		os.Interrupt,
		syscall.SIGTERM,
	)
	defer stop()

	if err := agent.Run(ctx); err != nil {
		fmt.Fprintf(os.Stderr, "node agent error: %v\n", err)
		os.Exit(1)
	}
}

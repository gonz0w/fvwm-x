// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopworker"
	"github.com/fvwmorg/FvwmPrompt/internal/trayicons"
	"io"
	"os"
	"strings"
	"syscall"
)

func run(input io.Reader, output io.Writer) error {
	ctx, cancel := context.WithTimeout(context.Background(), desktopworker.Deadline)
	defer cancel()
	type result struct {
		data []byte
		err  error
	}
	ready := make(chan result, 1)
	go func() {
		data, err := io.ReadAll(io.LimitReader(input, desktopworker.RequestLimit+1))
		ready <- result{data, err}
	}()
	var data []byte
	select {
	case r := <-ready:
		if r.err != nil {
			return r.err
		}
		data = r.data
	case <-ctx.Done():
		return ctx.Err()
	}
	request, err := desktopworker.Decode(data)
	if err != nil {
		return err
	}
	value, err := desktopworker.Run(ctx, request, desktopworker.NativeEnvironment())
	if err != nil {
		return err
	}
	data, err = json.Marshal(desktopworker.Response{Version: 1, Result: value})
	if err != nil {
		return err
	}
	if len(data)+1 > desktopworker.ResultLimit {
		return errors.New("desktop worker result limit reached")
	}
	data = append(data, '\n')
	n, err := output.Write(data)
	if err == nil && n != len(data) {
		err = io.ErrShortWrite
	}
	return err
}
func main() {
	// Only the executable, never an embedding policy caller, owns a process
	// group. Direct invocation must not share the interactive shell's group.
	if syscall.Getpgrp() != os.Getpid() {
		if err := syscall.Setpgid(0, 0); err != nil {
			fmt.Fprintln(os.Stderr, "cannot isolate desktop worker process group")
			os.Exit(1)
		}
	}
	if len(os.Args) == 2 && os.Args[1] == "--tray-icons" {
		ctx, cancel := context.WithTimeout(context.Background(), desktopworker.Deadline)
		defer cancel()
		environment := map[string]string{}
		for _, item := range os.Environ() {
			if key, value, ok := strings.Cut(item, "="); ok {
				environment[key] = value
			}
		}
		if err := trayicons.Run(ctx, os.Stdin, os.Stdout, environment); err != nil {
			fmt.Fprintln(os.Stderr, "fvwm-desktop-worker tray icons:", err)
			os.Exit(1)
		}
		return
	}
	if len(os.Args) != 1 {
		fmt.Fprintln(os.Stderr, "fvwm-desktop-worker: private JSON stdin protocol; no arguments")
		os.Exit(1)
	}
	if err := run(os.Stdin, os.Stdout); err != nil {
		data, _ := json.Marshal(desktopworker.Response{Version: 1, Error: err.Error()})
		fmt.Fprintln(os.Stdout, string(data))
		if errors.Is(err, desktopworker.ErrAbandonedJob) {
			_ = syscall.Kill(-os.Getpid(), syscall.SIGKILL)
		}
		os.Exit(1)
	}
}

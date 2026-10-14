// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"context"
	"errors"
	"io"
	"os"
	"os/exec"
	"syscall"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

type boundedOutput struct {
	bytes.Buffer
	limit int
}

func (out *boundedOutput) Write(data []byte) (int, error) {
	if len(data) > out.limit-out.Len() {
		return 0, errors.New("desktop menu worker output exceeds limit")
	}
	return out.Buffer.Write(data)
}

// The supervising process exposes only completed output. On a timeout it kills
// the isolated acquisition worker and returns without waiting on a filesystem
// call which the kernel may be unable to interrupt immediately.
func supervise(ctx context.Context, executable string, args []string, out, diagnostics io.Writer) error {
	command := exec.Command(executable, append([]string{"--_worker"}, args...)...)
	command.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	stdout, stderr := &boundedOutput{limit: xdgmenu.MaxRenderBytes}, &boundedOutput{limit: 1024 * 1024}
	command.Stdin = os.Stdin
	command.Stdout = stdout
	command.Stderr = stderr
	if err := command.Start(); err != nil {
		return err
	}
	completed := make(chan error, 1)
	go func() { completed <- command.Wait() }()
	select {
	case err := <-completed:
		if _, writeErr := diagnostics.Write(stderr.Bytes()); writeErr != nil {
			return writeErr
		}
		if err != nil {
			return err
		}
		_, err = out.Write(stdout.Bytes())
		return err
	case <-ctx.Done():
		_ = syscall.Kill(-command.Process.Pid, syscall.SIGKILL)
		return ctx.Err()
	}
}

func workerTimeout(args []string) (time.Duration, error) {
	defaults, _ := xdgmenu.DefaultsFromPreferences(nil)
	o, err := parseOptions(args, defaults)
	if err != nil {
		return 0, err
	}
	return o.timeout, nil
}

func configuredDeadline() (time.Duration, error) {
	value := os.Getenv("FVWM_XDG_MENU_TIMEOUT")
	if value == "" {
		return 5 * time.Second, nil
	}
	deadline, err := time.ParseDuration(value)
	if err != nil || deadline < time.Millisecond || deadline > 5*time.Minute {
		return 0, errors.New("FVWM_XDG_MENU_TIMEOUT must be between 1ms and 5m")
	}
	return deadline, nil
}

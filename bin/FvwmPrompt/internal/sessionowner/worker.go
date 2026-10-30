// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"strings"
	"syscall"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

var errScanReapUnconfirmed = errors.New("autostart worker termination/reaping unconfirmed")

type workerDiagnostic struct {
	data [maxDiagnosticBytes]byte
	size int
}

func (output *workerDiagnostic) Write(data []byte) (int, error) {
	output.size += copy(output.data[output.size:], data)
	return len(data), nil
}

// Scan runs only in the isolated self-worker. The parent owns its real deadline
// because context cancellation cannot interrupt every filesystem/lookup call.
func Scan(ctx context.Context, environment []string, output io.Writer) error {
	env := environmentMap(environment)
	home := env["HOME"]
	if home == "" {
		var err error
		home, err = os.UserHomeDir()
		if err != nil {
			return err
		}
	}
	scan, err := xdgsource.ScanAutostart(ctx, home, env)
	if err != nil {
		return err
	}
	if err = xdgmenu.ValidateAutostartScan(scan); err != nil {
		return err
	}
	data, err := json.Marshal(scan)
	if err != nil {
		return err
	}
	if len(data)+1 > xdgmenu.MaxAutostartOutputBytes {
		return errors.New("autostart worker output exceeds limit")
	}
	_, err = output.Write(append(data, '\n'))
	return err
}

func decodeScan(data []byte) (xdgmenu.AutostartScan, error) {
	var scan xdgmenu.AutostartScan
	if len(data) > xdgmenu.MaxAutostartOutputBytes {
		return scan, errors.New("autostart worker output exceeds limit")
	}
	if trimmed := bytes.TrimSpace(data); len(trimmed) == 0 || trimmed[0] != '{' {
		return scan, errors.New("autostart worker result must be an object")
	}
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(&scan); err != nil {
		return xdgmenu.AutostartScan{}, err
	}
	var extra any
	if err := decoder.Decode(&extra); err != io.EOF {
		return xdgmenu.AutostartScan{}, errors.New("unexpected extra autostart worker output")
	}
	if err := xdgmenu.ValidateAutostartScan(scan); err != nil {
		return xdgmenu.AutostartScan{}, err
	}
	return scan, nil
}

// Cancellation alone does not serialize CLI exit with a scan goroutine that
// has not yet submitted its direct child's Kill. Give acquisition a separate
// two-second teardown barrier, after any WM grace/reap phase. scanWorker spends
// at most one second observing actual Wait after its termination request. A
// blocked Start/Kill or kernel-delayed exit remains explicitly unconfirmed.
func finishScanWork(cancel context.CancelFunc, scanning <-chan scanResult, report *reporter) {
	cancel()
	if scanning == nil {
		return
	}
	timer := time.NewTimer(2 * time.Second)
	defer timer.Stop()
	select {
	case result := <-scanning:
		if errors.Is(result.err, errScanReapUnconfirmed) {
			report.terminal("%v", result.err)
		}
	case <-timer.C:
		report.terminal("autostart acquisition shutdown/reaping is unconfirmed")
	}
}

func scanWorker(parent context.Context, input Config) (xdgmenu.AutostartScan, error) {
	cfg := defaults(input)
	ctx, cancel := context.WithTimeout(parent, cfg.ScanTimeout)
	defer cancel()
	if err := ctx.Err(); err != nil {
		return xdgmenu.AutostartScan{}, fmt.Errorf("autostart worker deadline/cancellation: %w", err)
	}
	reader, writer, err := os.Pipe()
	if err != nil {
		return xdgmenu.AutostartScan{}, err
	}
	defer reader.Close()
	command := cfg.Command(cfg.SelfPath, WorkerArgument)
	var diagnostic workerDiagnostic
	command.Env, command.Stdout, command.Stderr = cfg.Environment, writer, &diagnostic
	command.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := ctx.Err(); err != nil {
		writer.Close()
		return xdgmenu.AutostartScan{}, fmt.Errorf("autostart worker deadline/cancellation: %w", err)
	}
	if err = command.Start(); err != nil {
		writer.Close()
		return xdgmenu.AutostartScan{}, err
	}
	writer.Close()
	waited := make(chan error, 1)
	go func() { waited <- command.Wait() }()
	type outputResult struct {
		data []byte
		err  error
	}
	output := make(chan outputResult, 1)
	go func() {
		data, err := io.ReadAll(io.LimitReader(reader, xdgmenu.MaxAutostartOutputBytes+1))
		output <- outputResult{data, err}
	}()
	// Acquisition never forks: only this direct worker needs termination.
	// Process.Kill synchronizes with Wait's process state, unlike a numeric
	// process-group signal that could race reaping and identifier reuse. The
	// waiter still owns reap if the kernel delays delivery beyond this bounded
	// observation; a deadline never manufactures a completed Wait.
	stop := func(cause error) error {
		_ = command.Process.Kill()
		reader.Close()
		if waited == nil {
			return cause
		}
		timer := time.NewTimer(time.Second)
		defer timer.Stop()
		select {
		case <-waited:
			return cause
		case <-timer.C:
			select {
			case <-waited:
				return cause
			default:
				return errors.Join(cause, errScanReapUnconfirmed)
			}
		}
	}
	var data []byte
	for waited != nil || output != nil {
		select {
		case err := <-waited:
			waited = nil
			if err != nil {
				reader.Close()
				// Wait completed the stderr copier before this bounded read.
				return xdgmenu.AutostartScan{}, fmt.Errorf("autostart worker failed: %w: %s", err, strings.TrimSpace(string(diagnostic.data[:diagnostic.size])))
			}
		case result := <-output:
			output = nil
			if result.err != nil || len(result.data) > xdgmenu.MaxAutostartOutputBytes {
				return xdgmenu.AutostartScan{}, stop(errors.New("autostart worker output unavailable or excessive"))
			}
			data = result.data
		case <-ctx.Done():
			return xdgmenu.AutostartScan{}, stop(fmt.Errorf("autostart worker deadline/cancellation: %w", ctx.Err()))
		}
	}
	return decodeScan(data)
}

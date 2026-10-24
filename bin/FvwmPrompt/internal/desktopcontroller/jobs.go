// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"time"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopworker"
	"golang.org/x/sys/unix"
)

type ioResult struct {
	data []byte
	err  error
}

// The owner alone reaps/signals the leader. Reader/writer goroutines own only
// their pipe ends; cancellation kills the group before a wait can release its
// PID. Never signal a group after Wait4 has reaped its leader.
func child(ctx context.Context, argv []string, input []byte, limit int, deadline time.Duration) ([]byte, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	if len(argv) == 0 {
		return nil, errors.New("Missing desktop helper")
	}
	executable, err := exec.LookPath(argv[0])
	if err != nil {
		return nil, err
	}
	inR, inW, err := os.Pipe()
	if err != nil {
		return nil, err
	}
	defer inR.Close()
	defer inW.Close()
	outR, outW, err := os.Pipe()
	if err != nil {
		return nil, err
	}
	defer outR.Close()
	defer outW.Close()
	null, err := os.OpenFile(os.DevNull, os.O_RDWR, 0)
	if err != nil {
		return nil, err
	}
	defer null.Close()
	process, err := os.StartProcess(executable, argv, &os.ProcAttr{Env: os.Environ(), Files: []*os.File{inR, outW, null}, Sys: &syscall.SysProcAttr{Setpgid: true}})
	if err != nil {
		return nil, err
	}
	defer process.Release()
	inR.Close()
	outW.Close()
	reaped := false
	defer func() {
		if !reaped {
			_ = unix.Kill(-process.Pid, unix.SIGKILL)
			var status unix.WaitStatus
			for {
				_, err := unix.Wait4(process.Pid, &status, 0, nil)
				if err != unix.EINTR {
					break
				}
			}
		}
	}()
	wrote := make(chan error, 1)
	read := make(chan ioResult, 1)
	go func() { _, err := inW.Write(input); inW.Close(); wrote <- err }()
	go func() { data, err := io.ReadAll(io.LimitReader(outR, int64(limit)+1)); read <- ioResult{data, err} }()
	timer := time.NewTimer(deadline)
	defer timer.Stop()
	ticker := time.NewTicker(10 * time.Millisecond)
	defer ticker.Stop()
	var result []byte
	writeDone, readDone := false, false
	for {
		select {
		case err := <-wrote:
			writeDone = true
			if err != nil {
				return nil, err
			}
		case got := <-read:
			readDone = true
			if got.err != nil {
				return nil, got.err
			}
			if len(got.data) > limit {
				return nil, errors.New("Desktop worker result exceeds limit")
			}
			result = got.data
		case <-ctx.Done():
			return nil, ctx.Err()
		case <-timer.C:
			return nil, errors.New("Desktop worker deadline exceeded")
		case <-ticker.C:
		}
		if !writeDone || !readDone {
			continue
		}
		var status unix.WaitStatus
		pid, err := unix.Wait4(process.Pid, &status, unix.WNOHANG, nil)
		if err == unix.EINTR {
			continue
		}
		if err != nil {
			if err == unix.ECHILD {
				reaped = true
			}
			return nil, err
		}
		if pid == 0 {
			continue
		}
		reaped = true
		if !status.Exited() || status.ExitStatus() != 0 {
			return nil, errors.New("Desktop worker failed")
		}
		return result, nil
	}
}
func worker(ctx context.Context, helper, operation string, args values) (any, error) {
	request := values{"version": 1, "operation": operation}
	for k, v := range args {
		request[k] = v
	}
	input, err := json.Marshal(request)
	if err != nil {
		return nil, err
	}
	if len(input) > desktopworker.RequestLimit {
		return nil, errors.New("Desktop worker request exceeds limit")
	}
	data, err := child(ctx, []string{helper}, input, desktopworker.ResultLimit, 6*time.Second)
	if err != nil {
		return nil, err
	}
	if !utf8.Valid(data) {
		return nil, errors.New("Invalid desktop worker encoding")
	}
	decoded, err := desktopsettings.DecodeJSON(data)
	// Settings JSON's smaller bound applies to settings policy, not an apps
	// result. Decode the bounded result with a duplicate-aware local decoder.
	if len(data) > desktopsettingsJSONLimit {
		decoded, err = decodeResult(data)
	}
	if err != nil {
		return nil, err
	}
	result, ok := decoded.(map[string]any)
	if !ok || len(result) != 2 || result["version"] != json.Number("1") {
		return nil, errors.New("Invalid desktop worker response")
	}
	value, ok := result["result"]
	if !ok {
		return nil, errors.New("Invalid desktop worker response")
	}
	return value, nil
}

const desktopsettingsJSONLimit = 65536

func decodeResult(data []byte) (any, error) {
	// Reuse the strict decoder in bounded chunks by rejecting duplicate keys
	// while streaming; depth and aggregate bytes are checked before decoding.
	decoder := json.NewDecoder(strings.NewReader(string(data)))
	decoder.UseNumber()
	var read func(int) (any, error)
	read = func(depth int) (any, error) {
		if depth > 16 {
			return nil, errors.New("Desktop result depth exceeds limit")
		}
		token, err := decoder.Token()
		if err != nil {
			return nil, err
		}
		delim, ok := token.(json.Delim)
		if !ok {
			return token, nil
		}
		switch delim {
		case '{':
			object := map[string]any{}
			for decoder.More() {
				key, err := decoder.Token()
				if err != nil {
					return nil, err
				}
				name, ok := key.(string)
				if !ok {
					return nil, errors.New("Invalid result key")
				}
				if _, ok := object[name]; ok {
					return nil, errors.New("Duplicate result key")
				}
				value, err := read(depth + 1)
				if err != nil {
					return nil, err
				}
				object[name] = value
			}
			_, err = decoder.Token()
			return object, err
		case '[':
			array := []any{}
			for decoder.More() {
				v, err := read(depth + 1)
				if err != nil {
					return nil, err
				}
				array = append(array, v)
			}
			_, err = decoder.Token()
			return array, err
		}
		return nil, errors.New("Invalid result token")
	}
	value, err := read(0)
	if err != nil {
		return nil, err
	}
	if _, err = decoder.Token(); err != io.EOF {
		return nil, errors.New("Trailing result data")
	}
	return value, nil
}
func helperPath(name, override string) string {
	if value := os.Getenv(override); value != "" {
		return value
	}
	self, _ := os.Executable()
	return filepath.Join(filepath.Dir(self), name)
}
func resultAs[T any](value any) (T, error) {
	var result T
	data, err := json.Marshal(value)
	if err != nil {
		return result, err
	}
	decoder := json.NewDecoder(strings.NewReader(string(data)))
	decoder.DisallowUnknownFields()
	err = decoder.Decode(&result)
	return result, err
}

type job struct {
	key     string
	running bool
	result  any
	err     error
	run     func(context.Context) (any, error)
}

func (d *Controller) submit(key string, run func(context.Context) (any, error)) bool {
	if _, ok := d.jobs[key]; ok || len(d.jobs) >= 4 {
		return false
	}
	j := &job{key: key, run: run}
	d.jobs[key] = j
	d.queue = append(d.queue, j)
	return true
}
func (d *Controller) workerJob(key, operation string, args values) bool {
	helper := d.worker
	return d.submit(key, func(ctx context.Context) (any, error) { return worker(ctx, helper, operation, args) })
}
func (d *Controller) startJobs() {
	for d.active < 2 && len(d.queue) > 0 {
		j := d.queue[0]
		d.queue = d.queue[1:]
		j.running = true
		d.active++
		go func() { value, err := j.run(d.ctx); d.completed <- job{key: j.key, result: value, err: err} }()
	}
}
func (d *Controller) cancelQueued(key string) bool {
	for i, j := range d.queue {
		if j.key == key {
			d.queue = append(d.queue[:i], d.queue[i+1:]...)
			delete(d.jobs, key)
			return true
		}
	}
	return false
}
func (d *Controller) jobError(key string, err error) { d.notice(fmt.Sprint(key, ": ", err)) }

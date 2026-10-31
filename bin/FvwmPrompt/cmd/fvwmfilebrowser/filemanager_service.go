// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/runtimepath"
)

// Set by Meson to the installed private helper, never searched through PATH.
var fileManagerBusHelperPath string

type fileManagerFrame struct {
	Version   int      `json:"version"`
	Kind      string   `json:"kind"`
	Owner     string   `json:"owner,omitempty"`
	Detail    string   `json:"detail,omitempty"`
	ID        string   `json:"id,omitempty"`
	Method    string   `json:"method,omitempty"`
	URIs      []string `json:"uris,omitempty"`
	StartupID string   `json:"startup_id,omitempty"`
}

type fileManagerEvent struct {
	frame fileManagerFrame
	err   error
}

func decodeFileManagerJSON(data []byte, value any) error {
	if !utf8.Valid(data) {
		return errors.New("invalid UTF-8 bridge frame")
	}
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(value); err != nil {
		return err
	}
	if decoder.Decode(new(any)) != io.EOF {
		return errors.New("trailing bridge frame data")
	}
	return nil
}

func readFileManagerFrames(reader io.Reader, events chan<- fileManagerEvent, stop <-chan struct{}, failed func()) {
	// Require the NDJSON delimiter before decoding. ScanLines would return an
	// unterminated final JSON object at EOF and could admit that cut frame.
	buffered := bufio.NewReaderSize(reader, fileManagerMaxFrame+1)
	for {
		line, err := buffered.ReadSlice('\n')
		if err != nil {
			if err == io.EOF && len(line) != 0 {
				err = io.ErrUnexpectedEOF
			}
			if failed != nil {
				failed()
			}
			select {
			case events <- fileManagerEvent{err: err}:
			case <-stop:
			}
			return
		}
		var frame fileManagerFrame
		data := line[:len(line)-1]
		err = decodeFileManagerJSON(data, &frame)
		if len(data) > fileManagerMaxFrame || frame.Version != 1 {
			err = errors.New("invalid bridge frame version or length")
		}
		if err != nil && failed != nil {
			failed()
		}
		select {
		case events <- fileManagerEvent{frame, err}:
		case <-stop:
			return
		}
		if err != nil {
			return
		}
	}
}

func fileManagerRequestID(id string) (uint64, bool) {
	number, err := strconv.ParseUint(id, 10, 64)
	return number, err == nil && number != 0 && strconv.FormatUint(number, 10) == id
}

func fileManagerReply(id, status string) ([]byte, error) {
	if _, ok := fileManagerRequestID(id); !ok {
		return nil, errors.New("invalid bridge request ID")
	}
	detail := ""
	switch status {
	case "accepted":
		detail = "Browser work accepted; display completion is not confirmed"
	case "invalid":
		detail = "Invalid or over-limit local file URI request"
	case "refused":
		detail = "Browser launch capacity or startup unavailable"
	case "unknown":
		detail = "Admission outcome is unknown; do not retry automatically"
	default:
		return nil, errors.New("invalid bridge reply status")
	}
	data, err := json.Marshal(struct {
		Version int    `json:"version"`
		ID      string `json:"id"`
		Status  string `json:"status"`
		Detail  string `json:"detail"`
	}{1, id, status, detail})
	if err != nil || len(data) > fileManagerMaxReply {
		return nil, errors.New("bridge reply exceeds limit")
	}
	return append(data, '\n'), nil
}

func fileManagerHelperPath(getenv func(string) string) (string, error) {
	path := getenv("FVWM_FILEMANAGER_BUS_HELPER")
	if path == "" {
		path = fileManagerBusHelperPath
	}
	if !filepath.IsAbs(path) || strings.ContainsRune(path, 0) {
		return "", errors.New("FileManager1 needs an installed helper or an absolute FVWM_FILEMANAGER_BUS_HELPER")
	}
	return path, nil
}

type fileManagerBridge struct {
	cmd     *exec.Cmd
	input   io.WriteCloser
	output  io.ReadCloser
	done    chan error
	stop    chan struct{}
	reaped  bool
	context context.Context
	cancel  context.CancelFunc
}

func (bridge *fileManagerBridge) close() error {
	bridge.cancel()
	close(bridge.stop)
	bridge.input.Close()
	bridge.output.Close()
	if bridge.reaped {
		return nil
	}
	select {
	case <-bridge.done:
		return nil
	default:
	}
	_ = bridge.cmd.Process.Signal(syscall.SIGTERM)
	timer := time.NewTimer(300 * time.Millisecond)
	defer timer.Stop()
	select {
	case <-bridge.done:
		return nil
	case <-timer.C:
	}
	_ = bridge.cmd.Process.Kill()
	timer.Reset(time.Second)
	select {
	case <-bridge.done:
		return nil
	case <-timer.C:
		return errors.New("native FileManager1 helper reap is unconfirmed")
	}
}

func startFileManagerBridge(parent context.Context, path string) (*fileManagerBridge, <-chan fileManagerEvent, <-chan error, chan<- []byte, error) {
	// Own the pipe ends explicitly: Cmd.Wait must not close StdoutPipe before
	// the reader consumes an unavailable diagnostic from a short-lived stub.
	inputRead, input, err := os.Pipe()
	if err != nil {
		return nil, nil, nil, nil, err
	}
	output, outputWrite, err := os.Pipe()
	if err != nil {
		inputRead.Close()
		input.Close()
		return nil, nil, nil, nil, err
	}
	cmd := exec.Command(path, "filemanager")
	cmd.Stdin, cmd.Stdout, cmd.Stderr = inputRead, outputWrite, os.Stderr
	err = cmd.Start()
	inputRead.Close()
	outputWrite.Close()
	if err != nil {
		input.Close()
		output.Close()
		return nil, nil, nil, nil, err
	}
	ctx, cancel := context.WithCancel(parent)
	bridge := &fileManagerBridge{cmd: cmd, input: input, output: output, done: make(chan error, 1), stop: make(chan struct{}), context: ctx, cancel: cancel}
	events := make(chan fileManagerEvent)
	writeErrors := make(chan error, 1)
	replies := make(chan []byte, 1)
	go func() {
		bridge.done <- cmd.Wait()
		cancel()
	}()
	go readFileManagerFrames(output, events, bridge.stop, cancel)
	go func() {
		for {
			select {
			case data := <-replies:
				if _, err := input.Write(data); err != nil {
					cancel()
					writeErrors <- err
					return
				}
			case <-bridge.stop:
				return
			}
		}
	}()
	return bridge, events, writeErrors, replies, nil
}

func runFileManagerService(o options) (result error) {
	path, err := fileManagerHelperPath(os.Getenv)
	if err != nil {
		return err
	}
	executable, err := runtimepath.Executable(os.Args[0])
	if err != nil {
		return err
	}
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	bridge, events, writeErrors, replies, err := startFileManagerBridge(ctx, path)
	if err != nil {
		return errors.New("cannot start native FileManager1 helper")
	}
	defer func() {
		if err := bridge.close(); err != nil {
			if result == nil {
				result = err
			} else {
				result = fmt.Errorf("%w; %v", result, err)
			}
		}
	}()
	startup := time.NewTimer(5 * time.Second)
	defer startup.Stop()
	var owner string
	select {
	case event := <-events:
		if event.err != nil {
			return errors.New("native FileManager1 startup pipe failed")
		}
		if event.frame.Kind == "unavailable" {
			if len(event.frame.Detail) > 512 || !validFileManagerStartupID(event.frame.Detail) {
				return errors.New("native FileManager1 service unavailable; invalid diagnostic")
			}
			return fmt.Errorf("native FileManager1 service unavailable: %s", event.frame.Detail)
		}
		if event.frame.Kind != "ready" || len(event.frame.Owner) < 2 || len(event.frame.Owner) > 255 || !strings.HasPrefix(event.frame.Owner, ":") {
			return errors.New("native FileManager1 ownership was not confirmed")
		}
		owner = event.frame.Owner
	case <-startup.C:
		return errors.New("native FileManager1 startup timed out")
	case <-ctx.Done():
		return nil
	}
	if err := json.NewEncoder(os.Stdout).Encode(struct {
		Version int    `json:"version"`
		Kind    string `json:"kind"`
		Owner   string `json:"owner"`
	}{1, "ready", owner}); err != nil {
		return errors.New("cannot report FileManager1 readiness")
	}
	children := newFileManagerChildren()
	var lastID uint64
	for {
		select {
		case <-ctx.Done():
			return nil
		case <-bridge.done:
			bridge.reaped = true
			return errors.New("native FileManager1 helper exited")
		case <-writeErrors:
			return errors.New("native FileManager1 reply pipe failed")
		case event := <-events:
			if event.err != nil {
				return errors.New("native FileManager1 request pipe failed")
			}
			frame := event.frame
			id, valid := fileManagerRequestID(frame.ID)
			if frame.Kind != "request" || !valid || id <= lastID {
				return errors.New("invalid native FileManager1 request envelope")
			}
			lastID = id
			status := "invalid"
			if groups, err := planFileManager(frame); err == nil {
				admissionCtx, cancel := context.WithTimeout(bridge.context, fileManagerAdmissionWait)
				status = children.admit(admissionCtx, executable, groups, o.socket, frame.StartupID)
				cancel()
			}
			data, err := fileManagerReply(frame.ID, status)
			if err != nil {
				return err
			}
			select {
			case replies <- data:
			case <-ctx.Done():
				return nil
			default:
				return errors.New("native FileManager1 reply writer is busy")
			}
		}
	}
}

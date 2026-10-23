// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bufio"
	"context"
	"encoding/json"
	"errors"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

const watchLease = time.Minute
const watchWire = 16 * 1024

type watchRequest struct {
	Path []byte             `json:"path_bytes"`
	Root filemodel.Identity `json:"root"`
}

type nativeDirectoryWatch interface {
	next() (changed, lost bool, err error)
	close()
}

// Directory opens, native registration and event reads stay in this separate
// process. A blocked kernel call can delay reap; the parent retains its slot.
func runWatchWorker(input io.Reader, output io.Writer, lease time.Duration) error {
	data, err := io.ReadAll(io.LimitReader(input, watchWire+1))
	var request watchRequest
	if err != nil || len(data) > watchWire || decode(data, &request) != nil ||
		!filepath.IsAbs(string(request.Path)) || len(request.Path) > filemodel.MaxPath ||
		strings.ContainsRune(string(request.Path), 0) || !request.Root.Mode.IsDir() {
		return errors.New("invalid watch request")
	}
	emit := func(state string) error { _, err := io.WriteString(output, state+"\n"); return err }
	fd, err := syscall.Open(string(request.Path), syscall.O_RDONLY|syscall.O_NONBLOCK|syscall.O_DIRECTORY|syscall.O_CLOEXEC, 0)
	if err != nil {
		return emit("unavailable")
	}
	directory := os.NewFile(uintptr(fd), "watched directory")
	defer directory.Close()
	info, err := directory.Stat()
	if err != nil || !info.IsDir() {
		return emit("unavailable")
	}
	stat := info.Sys().(*syscall.Stat_t)
	if uint64(stat.Dev) != request.Root.Device || uint64(stat.Ino) != request.Root.Inode {
		return emit("lost")
	}
	watch, err := newNativeDirectoryWatch(directory)
	if err != nil {
		return emit("unavailable")
	}
	defer watch.close()
	if err := emit("ready"); err != nil {
		return err
	}
	deadline, next := time.Now().Add(lease), time.Time{}
	dirty := false
	for time.Now().Before(deadline) {
		changed, lost, err := watch.next()
		if err != nil {
			return emit("unavailable")
		}
		if lost {
			return emit("lost")
		}
		dirty = dirty || changed
		if now := time.Now(); dirty && !now.Before(next) {
			if err := emit("changed"); err != nil {
				return err
			}
			dirty, next = false, now.Add(500*time.Millisecond)
		}
	}
	// A finite lease bounds an orphan even if the parent dies while no directory
	// events arrive. The parent re-arms only after this child has been reaped.
	return emit("expired")
}

type watchEvent struct {
	serial uint64
	state  string
}

func watchProcess(ctx context.Context, executable string, request watchRequest,
	serial uint64, events chan<- watchEvent) {
	emit := func(state string) bool {
		select {
		case events <- watchEvent{serial, state}:
			return true
		case <-ctx.Done():
			return false
		}
	}
	data, err := json.Marshal(request)
	if err != nil || len(data) > watchWire {
		emit("unavailable")
		events <- watchEvent{serial, "done"}
		return
	}
	commandContext, stop := context.WithTimeout(ctx, watchLease+workerLimit)
	defer stop()
	command := exec.CommandContext(commandContext, executable, "--watch-worker")
	command.Stdin = strings.NewReader(string(data))
	command.WaitDelay = time.Second
	pipe, err := command.StdoutPipe()
	if err == nil {
		err = command.Start()
	}
	if err != nil {
		emit("unavailable")
		events <- watchEvent{serial, "done"}
		return
	}
	scanner := bufio.NewScanner(pipe)
	scanner.Buffer(make([]byte, 128), 128)
	ready, terminal := false, false
	for scanner.Scan() {
		state := scanner.Text()
		valid := !terminal
		switch state {
		case "ready":
			valid, ready = valid && !ready, true
		case "changed":
			valid = valid && ready
		case "expired":
			valid, terminal = valid && ready, true
		case "unavailable", "lost":
			terminal = true
		default:
			valid = false
		}
		if !valid || !emit(state) {
			command.Process.Kill()
			terminal = false
			break
		}
	}
	if scanner.Err() != nil || !terminal {
		command.Process.Kill()
	}
	err = command.Wait()
	if ctx.Err() == nil && (err != nil || scanner.Err() != nil || !terminal) {
		emit("unavailable")
	}
	// This final record releases the process slot even after cancellation.
	// The bounded receiver lives until browser shutdown; no replacement starts
	// before Wait has completed. Its send cannot retain another process.
	events <- watchEvent{serial, "done"}
}

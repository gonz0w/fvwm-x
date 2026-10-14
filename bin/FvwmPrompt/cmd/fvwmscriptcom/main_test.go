// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"context"
	"errors"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"testing"
	"time"
)

func TestCommunicationPID(t *testing.T) {
	for _, name := range []string{"ComExample-2", "2", "ComExample-2147483647"} {
		if _, err := communicationPID(name); err != nil {
			t.Fatal(name, err)
		}
	}
	for _, name := range []string{"", "0", "1", "ComExample-0", "-2", "a2", "ComExample-2/3", "ComExample-2147483648", strings.Repeat("0", 250) + "2"} {
		if _, err := communicationPID(name); err == nil {
			t.Fatal("accepted", name)
		}
	}
}

func TestProtocol(t *testing.T) {
	var p protocol
	for _, command := range []string{"startup", "count", "count", "unknown", "multilines", "exit"} {
		text, stop := p.answer(command)
		switch command {
		case "startup":
			if !strings.HasSuffix(text, "is up") {
				t.Fatal(text)
			}
		case "count":
			if text != "Get "+strconv.FormatUint(p.count, 10)+" count messages" {
				t.Fatal(text)
			}
		case "unknown":
			if text != "0" {
				t.Fatal(text)
			}
		case "multilines":
			for i := 0; i < 3; i++ {
				n, err := strconv.Atoi(text[:4])
				if err != nil || n > len(text)-4 {
					t.Fatal(text)
				}
				text = text[4+n:]
			}
			if text != "" {
				t.Fatal(text)
			}
		}
		if stop != (command == "exit") {
			t.Fatal(command)
		}
	}
}

func TestEndpointIdentity(t *testing.T) {
	path := filepath.Join(t.TempDir(), "pipe")
	e, err := newEndpoint(path)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := newEndpoint(path); !os.IsExist(err) {
		t.Fatal(err)
	}
	fd, err := e.open(syscall.O_RDONLY)
	if err != nil {
		t.Fatal(err)
	}
	syscall.Close(fd)
	if err := os.Remove(path); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, []byte("replacement"), 0600); err != nil {
		t.Fatal(err)
	}
	if fd, err := e.open(syscall.O_RDONLY); err == nil {
		syscall.Close(fd)
		t.Fatal("opened replacement")
	}
	e.remove()
	if text, err := os.ReadFile(path); err != nil || string(text) != "replacement" {
		t.Fatal(string(text), err)
	}
}

func TestEndpointSymlink(t *testing.T) {
	path := filepath.Join(t.TempDir(), "pipe")
	e, err := newEndpoint(path)
	if err != nil {
		t.Fatal(err)
	}
	os.Remove(path)
	if err := os.Symlink("absent", path); err != nil {
		t.Fatal(err)
	}
	if fd, err := e.open(syscall.O_RDONLY); err == nil {
		syscall.Close(fd)
		t.Fatal("opened symlink")
	}
	e.remove()
	if _, err := os.Lstat(path); err != nil {
		t.Fatal(err)
	}
}

func TestPauseDeadlineAndCancellation(t *testing.T) {
	if err := pause(context.Background(), time.Now().Add(-time.Second)); !errors.Is(err, context.DeadlineExceeded) {
		t.Fatal(err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if err := pause(ctx, time.Time{}); !errors.Is(err, context.Canceled) {
		t.Fatal(err)
	}
}

func TestReplyNoReaderDeadline(t *testing.T) {
	e, err := newEndpoint(filepath.Join(t.TempDir(), "pipe"))
	if err != nil {
		t.Fatal(err)
	}
	defer e.remove()
	start := time.Now()
	err = reply(context.Background(), e, os.Getpid(), "answer", 40*time.Millisecond)
	if !errors.Is(err, context.DeadlineExceeded) || time.Since(start) > time.Second {
		t.Fatal(err)
	}
}

func TestReplyBlockedWriteDeadline(t *testing.T) {
	e, err := newEndpoint(filepath.Join(t.TempDir(), "pipe"))
	if err != nil {
		t.Fatal(err)
	}
	defer e.remove()
	fd, err := e.open(syscall.O_RDONLY)
	if err != nil {
		t.Fatal(err)
	}
	defer syscall.Close(fd)
	err = reply(context.Background(), e, os.Getpid(), strings.Repeat("x", 1024*1024), 40*time.Millisecond)
	if !errors.Is(err, context.DeadlineExceeded) {
		t.Fatal(err)
	}
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/clipboard"
	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
)

// This independent peer tests Run orchestration, not native memory or secure
// locker readiness. It is selected only for the exact --serve child invocation;
// normal provider/WM test re-execs must continue through the existing helpers.
func TestMain(m *testing.M) {
	mode := os.Getenv("FVWM_TEST_SESSION_CLIPBOARD")
	if mode != "" && len(os.Args) == 9 && os.Args[1] == "--serve" {
		log := os.Getenv("FVWM_TEST_SESSION_CLIPBOARD_LOG")
		if appendClipboardEvent(log, "peer-start") != nil {
			os.Exit(70)
		}
		state := uint64(clipboard.Unknown)
		input := bufio.NewScanner(os.Stdin)
		input.Buffer(make([]byte, 192), 192)
		for input.Scan() {
			fields := strings.Split(input.Text(), " ")
			if len(fields) != 7 || fields[0] != "1" || fields[3] != os.Args[2] {
				os.Exit(71)
			}
			verb, err := strconv.ParseUint(fields[2], 10, 64)
			if err != nil {
				os.Exit(72)
			}
			if verb == clipboard.State {
				value, err := strconv.ParseUint(fields[6], 10, 64)
				if err != nil || value != clipboard.Locked {
					os.Exit(73)
				}
				if appendClipboardEvent(log, "pause-request") != nil {
					os.Exit(74)
				}
				if mode == "refuse" {
					appendClipboardEvent(log, "pause-refused")
					os.Exit(1)
				}
				end := time.Now().Add(time.Second)
				for {
					if _, err := os.Stat(log + ".allow"); err == nil {
						break
					}
					if time.Now().After(end) {
						os.Exit(75)
					}
					time.Sleep(5 * time.Millisecond)
				}
				state = value
				if appendClipboardEvent(log, "pause-ack") != nil {
					os.Exit(76)
				}
			}
			result := uint64(clipboard.OK)
			if verb == clipboard.List {
				result = clipboard.Blocked
			}
			fmt.Printf("1 %s %d %s 0 0 0 0 0 0 %d 0 0 0\n", fields[1], result, fields[3], state)
		}
		if input.Err() != nil {
			os.Exit(77)
		}
		if appendClipboardEvent(log, "peer-eof") != nil {
			os.Exit(78)
		}
		os.Exit(0)
	}
	os.Exit(m.Run())
}

func appendClipboardEvent(path, event string) error {
	file, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0600)
	if err != nil {
		return err
	}
	_, writeErr := file.WriteString(event + "\n")
	closeErr := file.Close()
	if writeErr != nil {
		return writeErr
	}
	return closeErr
}

func clipboardEvents(path string) string { data, _ := os.ReadFile(path); return string(data) }

func TestConfiguredSessionClipboardLifetimeAndLockBarrier(t *testing.T) {
	for _, mode := range []string{"ack", "refuse"} {
		t.Run(mode, func(t *testing.T) {
			environment := serviceEnvironment(t, sessionservice.Defaults())
			root := environmentMap(environment)["HOME"]
			marker, log := filepath.Join(root, "environment.json"), filepath.Join(root, "clipboard-events")
			environment = append(environment, "FVWM_PROVIDER_ENV="+marker, "FVWM_TEST_SESSION_CLIPBOARD="+mode, "FVWM_TEST_SESSION_CLIPBOARD_LOG="+log)
			executable, err := os.Executable()
			if err != nil {
				t.Fatal(err)
			}
			limits, err := clipboard.DefaultTextLimits(uint64(os.Getpagesize()))
			if err != nil {
				t.Fatal(err)
			}
			configuration := clipboard.Config{Path: executable, Epoch: 19, Items: limits.Items, Bytes: limits.PerItem, Total: limits.Total, Mapped: limits.Mapped, Locked: limits.Locked, LifetimeMilliseconds: limits.Lifetime}
			signals := make(chan os.Signal, 1)
			var diagnostics bytes.Buffer
			cfg := Config{WMPath: "/fixture/wm", SelfPath: "/fixture/self", Environment: environment, NoAutostart: true, Signals: signals, Diagnostics: &diagnostics, Clipboard: &configuration, Command: func(path string, args ...string) *exec.Cmd {
				switch path {
				case "/fixture/wm":
					return providerCommand("wm")
				case "/fixture/self":
					return providerCommand("discovery")
				case "/fixture/locker":
					if err := appendClipboardEvent(log, "locker-constructed"); err != nil {
						t.Error(err)
					}
					return providerCommand("ok")
				default:
					t.Errorf("unexpected child %s", path)
					return providerCommand("fail")
				}
			}}
			done := make(chan int, 1)
			go func() { done <- Run(cfg) }()
			finished := false
			t.Cleanup(func() {
				if !finished {
					select {
					case signals <- syscall.SIGTERM:
					default:
					}
					select {
					case <-done:
					case <-time.After(6 * time.Second):
						t.Error("configured session cleanup unconfirmed")
					}
				}
			})
			deadline := time.Now().Add(4 * time.Second)
			var inherited []string
			for time.Now().Before(deadline) {
				data, err := os.ReadFile(marker)
				if err == nil && json.Unmarshal(data, &inherited) == nil {
					break
				}
				time.Sleep(5 * time.Millisecond)
			}
			endpoint := environmentMap(inherited)
			query := func(message sessionservice.RequestMessage) (sessionservice.Reply, error) {
				ctx, cancel := context.WithTimeout(context.Background(), 2500*time.Millisecond)
				defer cancel()
				return sessionservice.RequestTo(ctx, endpoint[sessionservice.SocketEnvironment], endpoint[sessionservice.CapabilityEnvironment], message)
			}
			var status sessionservice.Reply
			for time.Now().Before(deadline) {
				status, err = query(sessionservice.RequestMessage{Version: sessionservice.Version, Verb: "status"})
				_, restarted := os.Stat(marker + "-restart")
				if err == nil && restarted == nil && status.Status.Locker.State == "installed" && strings.Contains(clipboardEvents(log), "peer-start\n") {
					break
				}
				time.Sleep(5 * time.Millisecond)
			}
			if err != nil || status.Status.Locker.State != "installed" || strings.Count(clipboardEvents(log), "peer-start\n") != 1 {
				t.Fatalf("startup unavailable: %+v %v %q", status, err, clipboardEvents(log))
			}
			current := func(verb string) (sessionservice.Reply, error) {
				snapshot, err := query(sessionservice.RequestMessage{Version: sessionservice.Version, Verb: "status"})
				if err != nil {
					return sessionservice.Reply{}, err
				}
				return query(sessionservice.RequestMessage{Version: sessionservice.Version, Verb: verb, Generation: snapshot.Generation, Operation: snapshot.Operation})
			}
			initial, err := current("clipboard-status")
			if err != nil || initial.ClipboardCapabilities == nil || initial.ClipboardCapabilities.State != "blocked" || initial.ClipboardCapabilities.Mode != clipboard.Unknown {
				t.Fatalf("initial state %+v %v", initial, err)
			}
			type lockResult struct {
				reply sessionservice.Reply
				err   error
			}
			lockDone := make(chan lockResult, 1)
			go func() { reply, err := current("lock"); lockDone <- lockResult{reply, err} }()
			pauseDeadline := time.Now().Add(2 * time.Second)
			for time.Now().Before(pauseDeadline) && !strings.Contains(clipboardEvents(log), "pause-request\n") {
				time.Sleep(5 * time.Millisecond)
			}
			events := clipboardEvents(log)
			if !strings.Contains(events, "pause-request\n") || strings.Contains(events, "locker-constructed\n") {
				t.Fatalf("locker bypassed pause barrier %q", events)
			}
			if mode == "ack" {
				if err := os.WriteFile(log+".allow", []byte("ack\n"), 0600); err != nil {
					t.Fatal(err)
				}
			}
			select {
			case result := <-lockDone:
				if mode == "refuse" {
					if result.err == nil || strings.Contains(clipboardEvents(log), "locker-constructed\n") || !strings.Contains(clipboardEvents(log), "pause-refused\n") {
						t.Fatalf("failed pause started locker: %+v %v %q", result.reply, result.err, clipboardEvents(log))
					}
				} else if result.err != nil || result.reply.Status.Locker.SecureReady {
					t.Fatalf("lock outcome %+v %v", result.reply, result.err)
				}
			case <-time.After(3 * time.Second):
				t.Fatal("lock orchestration deadline")
			}
			if mode == "ack" {
				events = clipboardEvents(log)
				if strings.Index(events, "pause-ack\n") < 0 || strings.Index(events, "locker-constructed\n") < strings.Index(events, "pause-ack\n") {
					t.Fatalf("locker preceded acknowledgement %q", events)
				}
				end := time.Now().Add(time.Second)
				for time.Now().Before(end) {
					status, err = query(sessionservice.RequestMessage{Version: sessionservice.Version, Verb: "status"})
					if err == nil && status.Status.Locker.State == "exited" {
						break
					}
					time.Sleep(5 * time.Millisecond)
				}
				if status.Status.Locker.State != "exited" {
					t.Fatal("locker exit not observed")
				}
				caps, err := current("clipboard-status")
				if err != nil || caps.ClipboardCapabilities == nil || caps.ClipboardCapabilities.Mode != clipboard.Locked {
					t.Fatalf("locker exit resumed history %+v %v", caps, err)
				}
			}
			signals <- syscall.SIGTERM
			select {
			case code := <-done:
				finished = true
				if code != 0 {
					t.Fatalf("session exit %d", code)
				}
			case <-time.After(6 * time.Second):
				t.Fatal("logout cleanup deadline")
			}
			events = clipboardEvents(log)
			if strings.Count(events, "peer-start\n") != 1 {
				t.Fatalf("owner restarted across core exec %q", events)
			}
			expected := "clipboard owner cleanup: started=true reaped=true clean=false wait-confirmed=true"
			if mode == "ack" {
				expected = "clipboard owner cleanup: started=true reaped=true clean=true wait-confirmed=true"
				if !strings.Contains(events, "peer-eof\n") {
					t.Fatal("logout did not close native input")
				}
			}
			if strings.Count(diagnostics.String(), expected) != 1 {
				t.Fatalf("missing exact cleanup result %q", diagnostics.String())
			}
		})
	}
}

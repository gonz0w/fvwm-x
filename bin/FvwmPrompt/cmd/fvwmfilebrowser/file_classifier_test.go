// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"context"
	"crypto/rand"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

// The fixture is the test executable, reached through a PATH-local `file`
// symlink or explicit owned-worker mode. No shell participates in these tests.
func applicationRoleProcessFixture() bool {
	if len(os.Args) == 2 && os.Args[1] == "--role-descendant" || os.Getenv("FVWM_TEST_ROLE_APPLICATION") != "" && len(os.Args) > 1 && os.Args[1] != "--worker" {
		heartbeat := os.Getenv("FVWM_TEST_ROLE_HEARTBEAT")
		os.WriteFile(heartbeat+".pid", []byte(strconv.Itoa(os.Getpid())), 0600)
		for i := 0; ; i++ {
			if stop, token := os.Getenv("FVWM_TEST_ROLE_STOP"), os.Getenv("FVWM_TEST_ROLE_STOP_TOKEN"); stop != "" && token != "" {
				if data, err := os.ReadFile(stop); err == nil && string(data) == token {
					os.WriteFile(stop+".retired", []byte(token), 0600)
					os.Exit(0)
				}
			}
			os.WriteFile(heartbeat, []byte(strconv.Itoa(i)), 0600)
			time.Sleep(10 * time.Millisecond)
		}
	}
	startDescendant := func() {
		executable, _ := os.Executable()
		child := exec.Command(executable, "--role-descendant")
		if err := child.Start(); err != nil {
			os.Exit(90)
		}
		child.Process.Release()
		until := time.Now().Add(time.Second)
		for time.Now().Before(until) {
			if _, err := os.Stat(os.Getenv("FVWM_TEST_ROLE_HEARTBEAT") + ".pid"); err == nil {
				return
			}
			time.Sleep(time.Millisecond)
		}
		os.Exit(91)
	}
	if len(os.Args) == 2 && os.Args[1] == "--worker" && os.Getenv("FVWM_TEST_ROLE_WORKER") != "" {
		startDescendant()
		switch os.Getenv("FVWM_TEST_ROLE_WORKER") {
		case "crash":
			os.Exit(7)
		case "no-result":
			os.Exit(0)
		case "normal":
			fmt.Println(`{"schema_version":1,"status":"failed","error":"controlled failure"}`)
			os.Exit(0)
		case "flood":
			fmt.Print(strings.Repeat("x", 4*1024*1024+1))
			time.Sleep(time.Minute)
		default:
			time.Sleep(time.Minute)
		}
		os.Exit(0)
	}
	if len(os.Args) > 2 && os.Args[1] == "-b" && os.Args[2] == "--mime-type" && os.Getenv("FVWM_TEST_CLASSIFIER") != "" {
		if report := os.Getenv("FVWM_TEST_CLASSIFIER_REPORT"); report != "" {
			args := make([][]byte, len(os.Args)-1)
			for i := range args {
				args[i] = []byte(os.Args[i+1])
			}
			data, _ := json.Marshal(args)
			os.WriteFile(report, data, 0600)
		}
		switch os.Getenv("FVWM_TEST_CLASSIFIER") {
		case "nonzero":
			fmt.Fprint(os.Stderr, "controlled classifier failure")
			os.Exit(3)
		case "flood":
			fmt.Print(strings.Repeat("a", 257))
			time.Sleep(time.Minute)
		case "diagnostics":
			fmt.Fprint(os.Stderr, strings.Repeat("a", 4097))
			fmt.Println("text/plain")
		case "hang":
			startDescendant()
			time.Sleep(time.Minute)
		default:
			output, _ := base64.StdEncoding.DecodeString(os.Getenv("FVWM_TEST_CLASSIFIER_MIME"))
			os.Stdout.Write(output)
		}
		os.Exit(0)
	}
	return false
}

func classifierFixture(t *testing.T, mode, output string) string {
	t.Helper()
	executable, _ := os.Executable()
	directory := t.TempDir()
	if err := os.Symlink(executable, filepath.Join(directory, "file")); err != nil {
		t.Fatal(err)
	}
	t.Setenv("PATH", directory)
	t.Setenv("FVWM_TEST_CLASSIFIER", mode)
	t.Setenv("FVWM_TEST_CLASSIFIER_MIME", base64.StdEncoding.EncodeToString([]byte(output)))
	return executable
}

func TestFileClassifierStrictMIMEAndLiteralArguments(t *testing.T) {
	classifierFixture(t, "output", "Text/Plain\r\n")
	path := "/literal -L -z '$[exec.touch];\n/raw-\xff"
	report := filepath.Join(t.TempDir(), "argv")
	t.Setenv("FVWM_TEST_CLASSIFIER_REPORT", report)
	mime, err := classifyFile(context.Background(), path)
	if err != nil || mime != "text/plain" {
		t.Fatal(mime, err)
	}
	data, err := os.ReadFile(report)
	var args [][]byte
	if err != nil || json.Unmarshal(data, &args) != nil || len(args) != 3 || string(args[0]) != "-b" || string(args[1]) != "--mime-type" || string(args[2]) != path {
		t.Fatal("classifier argv was not fixed/literal", args, err)
	}
	for _, value := range []string{"", "text/plain; charset=utf-8", " text/plain", "text/plain ", "text/plain\ntext/html\n", "text/*", "*/plain", "text/plain\x00", "text/pläin", "text/plain\r", "text/plain\n\n", strings.Repeat("a", 257)} {
		t.Setenv("FVWM_TEST_CLASSIFIER_MIME", base64.StdEncoding.EncodeToString([]byte(value)))
		if _, err := classifyFile(context.Background(), path); err == nil {
			t.Fatal("malformed classifier result accepted", value)
		}
	}
	for _, mode := range []string{"nonzero", "flood", "diagnostics"} {
		t.Setenv("FVWM_TEST_CLASSIFIER", mode)
		if _, err := classifyFile(context.Background(), path); err == nil {
			t.Fatal("classifier failure accepted", mode)
		}
	}
	t.Setenv("PATH", t.TempDir())
	if _, err := classifyFile(context.Background(), path); err == nil || !strings.Contains(err.Error(), "file(1)") {
		t.Fatal("missing capability hidden", err)
	}
}

func heartbeatStopped(t *testing.T, path string) {
	t.Helper()
	before, err := os.ReadFile(path)
	if err != nil {
		t.Fatal("descendant did not start", err)
	}
	time.Sleep(80 * time.Millisecond)
	after, err := os.ReadFile(path)
	if err != nil || string(before) != string(after) {
		t.Fatal("owned descendant still running after worker reap", string(before), string(after), err)
	}
}

func TestOwnedWorkerGroupRetiresOnEveryOutcome(t *testing.T) {
	executable, _ := os.Executable()
	for _, mode := range []string{"crash", "no-result", "normal", "flood", "cancel"} {
		t.Run(mode, func(t *testing.T) {
			heartbeat := filepath.Join(t.TempDir(), "child")
			t.Setenv("FVWM_TEST_ROLE_HEARTBEAT", heartbeat)
			t.Setenv("FVWM_TEST_ROLE_WORKER", mode)
			ctx, cancel := context.WithTimeout(context.Background(), 500*time.Millisecond)
			defer cancel()
			result := startWorker(ctx, executable, workRequest{Operation: "scan"})
			if mode == "cancel" && result.Status != "cancelled" || mode != "cancel" && result.Status != "failed" {
				t.Fatal(mode, result)
			}
			heartbeatStopped(t, heartbeat)
		})
	}
}

func TestOwnedWorkerClassifierTimeoutAndCancellation(t *testing.T) {
	for _, cancelEarly := range []bool{false, true} {
		t.Run(strconv.FormatBool(cancelEarly), func(t *testing.T) {
			executable := classifierFixture(t, "hang", "")
			root := t.TempDir()
			os.WriteFile(filepath.Join(root, "selected"), []byte("text"), 0600)
			snapshot, err := filemodel.Scan(root, false)
			if err != nil {
				t.Fatal(err)
			}
			heartbeat := filepath.Join(t.TempDir(), "classifier-child")
			t.Setenv("FVWM_TEST_ROLE_HEARTBEAT", heartbeat)
			limit := 5 * time.Second
			if cancelEarly {
				limit = 500 * time.Millisecond
			}
			ctx, cancel := context.WithTimeout(context.Background(), limit)
			defer cancel()
			started := time.Now()
			result := startWorker(ctx, executable, workRequest{Operation: "file-roles", Snapshot: snapshot, ID: snapshot.Items[0].ID})
			if cancelEarly && result.Status != "cancelled" || !cancelEarly && result.Status != "failed" || time.Since(started) > 4*time.Second {
				t.Fatal(result, time.Since(started))
			}
			heartbeatStopped(t, heartbeat)
		})
	}
}

func TestOwnedWorkerRapidRetirementAndApplicationSurvival(t *testing.T) {
	executable, _ := os.Executable()
	root := t.TempDir()
	os.WriteFile(filepath.Join(root, "selected"), []byte("text"), 0600)
	snapshot, err := filemodel.Scan(root, false)
	if err != nil {
		t.Fatal(err)
	}
	for i := 0; i < 12; i++ {
		result := startWorker(context.Background(), executable, workRequest{Operation: "scan", Path: []byte(root)})
		if result.Status != "loaded" {
			t.Fatal("rapid owned worker retirement", i, result)
		}
	}
	heartbeat := filepath.Join(t.TempDir(), "application")
	stop := heartbeat + ".stop"
	var nonce [16]byte
	if _, err := rand.Read(nonce[:]); err != nil {
		t.Fatal(err)
	}
	token := hex.EncodeToString(nonce[:])
	t.Setenv("FVWM_TEST_ROLE_APPLICATION", "1")
	t.Setenv("FVWM_TEST_ROLE_HEARTBEAT", heartbeat)
	t.Setenv("FVWM_TEST_ROLE_STOP", stop)
	t.Setenv("FVWM_TEST_ROLE_STOP_TOKEN", token)
	// Register before handoff: failed assertions must also request cooperative
	// retirement. A PID read from a file is diagnostic data, not signal authority.
	t.Cleanup(func() {
		if err := os.WriteFile(stop, []byte(token), 0600); err != nil {
			t.Error("could not stop owned application fixture", err)
			return
		}
		until := time.Now().Add(3 * time.Second)
		for time.Now().Before(until) {
			if data, err := os.ReadFile(stop + ".retired"); err == nil && string(data) == token {
				return
			}
			time.Sleep(time.Millisecond)
		}
		t.Error("owned application fixture did not acknowledge cooperative retirement")
	})
	result := startWorker(context.Background(), executable, workRequest{Operation: "custom", Snapshot: snapshot, IDs: []string{snapshot.Items[0].ID}, Program: executable})
	if result.Status != "started" {
		t.Fatal(result)
	}
	until := time.Now().Add(time.Second)
	for time.Now().Before(until) {
		if _, err := os.Stat(heartbeat); err == nil {
			break
		}
		time.Sleep(time.Millisecond)
	}
	before, err := os.ReadFile(heartbeat)
	if err != nil {
		t.Fatal(err)
	}
	time.Sleep(80 * time.Millisecond)
	after, _ := os.ReadFile(heartbeat)
	if string(before) == string(after) {
		t.Fatal("successful application died with worker group")
	}
}

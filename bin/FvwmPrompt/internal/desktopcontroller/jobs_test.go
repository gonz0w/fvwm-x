// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"context"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestChildPeer(t *testing.T) {
	mode := os.Getenv("FVWM_TEST_CHILD")
	if mode == "" {
		return
	}
	switch mode {
	case "echo":
		data, _ := io.ReadAll(os.Stdin)
		fmt.Fprint(os.Stdout, string(data))
	case "blocked":
		time.Sleep(10 * time.Second)
	case "overflow":
		fmt.Fprint(os.Stdout, strings.Repeat("x", 65537))
		time.Sleep(10 * time.Second)
	case "late-exit":
		os.Stdout.Close()
		time.Sleep(10 * time.Second)
	case "descendant":
		command := exec.Command("/bin/sh", "-c", `sleep .5; echo escaped > "$1"`, "peer", os.Getenv("FVWM_TEST_MARKER"))
		command.Stdout = os.Stdout
		command.Stderr = os.Stderr
		if err := command.Start(); err != nil {
			os.Exit(9)
		}
	}
	os.Exit(0)
}
func TestChildDuplexDeadlineOutputAndGroupOwnership(t *testing.T) {
	self, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	argv := []string{self, "-test.run=^TestChildPeer$"}
	for _, mode := range []string{"echo", "blocked", "overflow", "late-exit", "descendant"} {
		t.Run(mode, func(t *testing.T) {
			marker := filepath.Join(t.TempDir(), "escaped")
			t.Setenv("FVWM_TEST_CHILD", mode)
			t.Setenv("GORACE", "atexit_sleep_ms=0")
			t.Setenv("FVWM_TEST_MARKER", marker)
			input := []byte(strings.Repeat("literal payload", 4000))
			began := time.Now()
			data, err := child(context.Background(), argv, input, 65536, 200*time.Millisecond)
			if mode == "echo" {
				if err != nil || string(data) != string(input) {
					t.Fatalf("%d %v", len(data), err)
				}
			} else if err == nil {
				t.Fatal("unbounded child succeeded")
			}
			if time.Since(began) > 2*time.Second {
				t.Fatal("child deadline ineffective")
			}
			if mode == "descendant" {
				time.Sleep(700 * time.Millisecond)
				if _, err := os.Stat(marker); !os.IsNotExist(err) {
					t.Fatal("descendant escaped owned group")
				}
			}
		})
	}
}
func TestWorkerResultDecoderRefusesAmbiguity(t *testing.T) {
	for _, data := range []string{`{"x":1,"x":2}`, `{} {}`, `[`, strings.Repeat("[", 18) + strings.Repeat("]", 18)} {
		if _, err := decodeResult([]byte(data)); err == nil {
			t.Fatalf("accepted %s", data)
		}
	}
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"errors"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/sessionowner"
)

func TestLiteralCoreArguments(t *testing.T) {
	args := []string{"--no-autostart", "--", "-f", "a ; $(literal)", "--replace"}
	disabled, got, err := options(args)
	if err != nil || !disabled || !reflect.DeepEqual(got, args[2:]) {
		t.Fatalf("%v %v %v", disabled, got, err)
	}
	for _, args := range [][]string{{"-f", "file"}, {"--no-autostart", "--no-autostart"}, {"--_session-scan", "extra"}} {
		if _, _, err := options(args); err == nil {
			t.Fatalf("accepted %q", args)
		}
	}
}

func TestWorkerRefusalReportsCause(t *testing.T) {
	var output bytes.Buffer
	if code := run([]string{sessionowner.WorkerArgument}, []string{"HOME=relative"}, &output); code != 1 || !strings.HasPrefix(output.String(), "scan refused: ") || !strings.Contains(output.String(), "invalid autostart home directory") {
		t.Fatalf("code=%d diagnostic=%q", code, output.String())
	}
	output.Reset()
	reportScanError(&output, errors.New("unavailable\n\t\x1b"+strings.Repeat("é", 1000)))
	data := output.String()
	if len(data) > 512 || strings.Count(data, "\n") != 1 || strings.ContainsAny(data, "\t\x1b") || !utf8.ValidString(data) || !strings.HasPrefix(data, "scan refused: unavailable   ") {
		t.Fatalf("unbounded or multiline diagnostic: bytes=%d %q", len(data), data)
	}
}

func TestFixedCorePath(t *testing.T) {
	directory := t.TempDir()
	self := filepath.Join(directory, "fvwm-session")
	core := filepath.Join(directory, "fvwm3")
	if got, err := corePath("/build/fvwm3", self); err != nil || got != "/build/fvwm3" {
		t.Fatalf("%q %v", got, err)
	}
	if _, err := corePath("", self); err == nil {
		t.Fatal("missing sibling/configured image accepted")
	}
	if err := os.WriteFile(core, []byte("test-only image"), 0700); err != nil {
		t.Fatal(err)
	}
	if got, err := corePath("/unused/fvwm3", self); err != nil || got != core {
		t.Fatalf("%q %v", got, err)
	}
	if err := os.Chmod(core, 0600); err != nil {
		t.Fatal(err)
	}
	if _, err := corePath("/build/fvwm3", self); err == nil {
		t.Fatal("invalid sibling silently fell back")
	}
	if err := os.Remove(core); err != nil {
		t.Fatal(err)
	}
	for _, path := range []string{"fvwm3", "/opt/../fvwm3"} {
		if _, err := corePath(path, self); err == nil {
			t.Fatal(path)
		}
	}
	if err := os.Symlink(filepath.Join(directory, "missing"), core); err != nil {
		t.Fatal(err)
	}
	if _, err := corePath("/build/fvwm3", self); err == nil {
		t.Fatal("dangling sibling silently fell back")
	}
}

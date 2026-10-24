// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"context"
	"os"
	"path/filepath"
	"testing"
)

func TestCLIUsage(t *testing.T) {
	var out, stderr bytes.Buffer
	for _, args := range [][]string{{"--check-config"}, {"--json"}, {"--check-config", "one", "extra"}} {
		if status := check(context.Background(), args, &out, &stderr); status != 2 {
			t.Fatalf("%v: %d", args, status)
		}
	}
	if err := run(context.Background(), nil); err == nil {
		t.Fatal("module-less invocation accepted")
	}
}

func TestBuildAndInstalledHelperLookup(t *testing.T) {
	root := t.TempDir()
	bin := filepath.Join(root, "bin", "FvwmPrompt")
	build := filepath.Join(root, "default-config")
	for _, dir := range []string{bin, build} {
		if err := os.MkdirAll(dir, 0700); err != nil {
			t.Fatal(err)
		}
	}
	name := "fvwm-desktop-capture"
	fallback := filepath.Join(build, name)
	if err := os.WriteFile(fallback, nil, 0700); err != nil {
		t.Fatal(err)
	}
	if got := helperBeside(bin, name); got != fallback {
		t.Fatalf("build capture lookup: %s", got)
	}
	adjacent := filepath.Join(bin, name)
	if err := os.WriteFile(adjacent, nil, 0700); err != nil {
		t.Fatal(err)
	}
	if got := helperBeside(bin, name); got != adjacent {
		t.Fatalf("adjacent helper did not win: %s", got)
	}
}

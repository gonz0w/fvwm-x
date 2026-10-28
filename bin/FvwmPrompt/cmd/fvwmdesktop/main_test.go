// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"context"
	"encoding/json"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopcheck"
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
	build := filepath.Join(root, "bin")
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

func TestOfflineReportPreservesSchemaAndPhysicalIncludeSpans(t *testing.T) {
	result := desktopcheck.Result{SchemaVersion: 1, Coverage: "desktop-settings-literals-and-Read-v1", Errors: 1,
		Diagnostics: []desktopcheck.Diagnostic{{Location: desktopcheck.Location{Source: desktopcheck.Source{Path: "/tmp/a\t$\"%", Line: 2, EndLine: 4}, IncludedFrom: []desktopcheck.Source{{Path: "/root", Line: 7, EndLine: 9}}}, Severity: "error", Message: "Invalid value for palette"}}}
	var out bytes.Buffer
	printReport(&out, result, false)
	for _, want := range []string{`"/tmp/a\t$\"%":2-4`, `Read from "/root":7-9`} {
		if !strings.Contains(out.String(), want) {
			t.Fatal("text source context lost", out.String(), want)
		}
	}
	out.Reset()
	printReport(&out, result, true)
	var got desktopcheck.Result
	if err := json.Unmarshal(out.Bytes(), &got); err != nil || got.SchemaVersion != 1 || got.Diagnostics[0].IncludedFrom[0].EndLine != 9 || got.Diagnostics[0].Path != result.Diagnostics[0].Path {
		t.Fatal("schema 1 source context lost", out.String(), err)
	}
}

func TestBrowserPeerLookupIsOptionalAndPreservesExplicitOverride(t *testing.T) {
	directory := t.TempDir()
	commands := t.TempDir()
	t.Setenv("PATH", commands)
	missing := filepath.Join(directory, "fvwm-file-browser")
	if got := helperBeside(directory, "fvwm-file-browser"); got != missing {
		t.Fatal("absent optional peer unexpectedly changed resolution", got)
	}
	if _, err := os.Stat(missing); !os.IsNotExist(err) {
		t.Fatal("test optional peer exists", err)
	}
	installed := filepath.Join(commands, "fvwm-file-browser")
	if err := os.WriteFile(installed, nil, 0700); err != nil {
		t.Fatal(err)
	}
	if got := helperBeside(directory, "fvwm-file-browser"); got != installed {
		t.Fatal("installed bindir browser was not discovered through PATH", got)
	}
	if err := os.WriteFile(missing, nil, 0700); err != nil {
		t.Fatal(err)
	}
	if got := helperBeside(directory, "fvwm-file-browser"); got != missing {
		t.Fatal("adjacent build browser did not win over PATH", got)
	}
	// Optional application lookup must not become PATH discovery for self-workers.
	worker := filepath.Join(commands, "fvwm-desktop-worker")
	if err := os.WriteFile(worker, nil, 0700); err != nil {
		t.Fatal(err)
	}
	if got := helperBeside(directory, "fvwm-desktop-worker"); got != filepath.Join(directory, "fvwm-desktop-worker") {
		t.Fatal("self-worker lookup unexpectedly searched PATH", got)
	}
	literal := filepath.Join(directory, "browser $[]; with spaces")
	t.Setenv("FVWM_FILE_BROWSER", literal)
	got, err := adjacent("fvwm-file-browser", "FVWM_FILE_BROWSER")
	if err != nil || got != literal {
		t.Fatal("explicit optional browser override was expanded or rejected", got, err)
	}
}

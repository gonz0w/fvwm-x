// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcheck

import (
	"context"
	"os"
	"path/filepath"
	"strings"
	"syscall"
	"testing"
)

func fixture(t *testing.T) (Request, func(string, string)) {
	t.Helper()
	root := t.TempDir()
	return Request{Path: filepath.Join(root, "config"), Userdir: root,
		Datadir: filepath.Join(root, "data"), Cwd: root}, func(name, text string) {
		t.Helper()
		if err := os.WriteFile(filepath.Join(root, name), []byte(text), 0600); err != nil {
			t.Fatal(err)
		}
	}
}
func checked(t *testing.T, r Request) Result {
	t.Helper()
	v, err := Check(context.Background(), r)
	if err != nil {
		t.Fatal(err)
	}
	return v
}
func TestLiteralSubset(t *testing.T) {
	r, write := fixture(t)
	write("config", "# comment\n*FvwmDesktop: palette ink\n*FvwmDesktop: motion on\n")
	v := checked(t, r)
	if v.Status != "checked-subset" || v.CheckedDirectives != 2 || len(v.Diagnostics) != 0 || len(v.Files) != 1 || len(v.Files[0].SHA256) != 64 {
		t.Fatalf("%+v", v)
	}
	write("config", "*FvwmDesktop: unknown value\n*FvwmDesktop: motion 3\n*FvwmDesktop: day 09:00\n*FvwmDesktop: night 09:00\n")
	v = checked(t, r)
	if v.Errors != 3 || v.Diagnostics[2].Code != "schedule-equal" {
		t.Fatalf("%+v", v)
	}
}
func TestReadLocationsAndCycles(t *testing.T) {
	r, write := fixture(t)
	write("config", "# parent\nRead 'child config'\n")
	write("child config", "Read grandchild\n")
	write("grandchild", "# first\n*FvwmDesktop: palette \\\n wrong\n")
	v := checked(t, r)
	d := v.Diagnostics[0]
	if d.Path != filepath.Join(r.Userdir, "grandchild") || d.Line != 2 || d.EndLine != 3 || len(d.IncludedFrom) != 2 || d.IncludedFrom[0].Line != 2 {
		t.Fatalf("%+v", v)
	}
	if err := os.Symlink(r.Path, filepath.Join(r.Userdir, "alias")); err != nil {
		t.Fatal(err)
	}
	write("config", "Read alias\n")
	if checked(t, r).Diagnostics[0].Code != "include-cycle" {
		t.Fatal("missed inode cycle")
	}
	write("config", "Read 'child config'\nRead 'child config'\n")
	if len(checked(t, r).Files) != 5 {
		t.Fatal("nonrecursive reads must repeat")
	}
}
func TestReadSearchOrder(t *testing.T) {
	r, write := fixture(t)
	r.Cwd = filepath.Join(r.Userdir, "cwd")
	for _, root := range []string{r.Datadir, r.Cwd} {
		if err := os.Mkdir(root, 0700); err != nil {
			t.Fatal(err)
		}
	}
	write("config", "Read choice\nRead missing Quiet\n")
	roots := []string{r.Userdir, r.Datadir, r.Cwd}
	for _, root := range roots {
		if err := os.WriteFile(filepath.Join(root, "choice"), []byte("*FvwmDesktop: motion on"), 0600); err != nil {
			t.Fatal(err)
		}
	}
	for _, root := range roots {
		v := checked(t, r)
		if v.Errors != 0 || v.Files[1].Path != filepath.Join(root, "choice") {
			t.Fatalf("%+v", v)
		}
		if err := os.Remove(filepath.Join(root, "choice")); err != nil {
			t.Fatal(err)
		}
	}
	if checked(t, r).Errors != 1 {
		t.Fatal("missing literal should error")
	}
}
func TestNeverExecuteAndRejectNonregular(t *testing.T) {
	r, write := fixture(t)
	write("config", "Exec touch marker\nPipeRead echo\nModule shell\nRead $[env.FILE]\nRead `echo file`\nRead ~/file\nRead \"unterminated\n*FvwmDesktop: terminal /bin/sh\n")
	v := checked(t, r)
	if v.Unchecked != 8 || v.Errors != 0 || v.Status != "partial" {
		t.Fatalf("%+v", v)
	}
	fifo := filepath.Join(r.Userdir, "fifo")
	if err := syscall.Mkfifo(fifo, 0600); err != nil {
		t.Fatal(err)
	}
	write("config", "Read fifo Quiet\n")
	if checked(t, r).Diagnostics[0].Code != "read-type" {
		t.Fatal("fifo not rejected")
	}
	write("config", "Read\n")
	if checked(t, r).Diagnostics[0].Code != "read-argument" {
		t.Fatal("missing argument")
	}
}
func TestBudgets(t *testing.T) {
	for _, tt := range []struct {
		name, text, code string
		change           func(*limits)
	}{
		{"bytes", strings.Repeat("x", 20), "byte-limit", func(l *limits) { l.bytes = 8 }},
		{"line", strings.Repeat("x", 20), "line-limit", func(l *limits) { l.lineBytes = 8 }},
		{"lines", "# one\n# two\n# three\n", "work-limit", func(l *limits) { l.lines = 2 }},
		{"diagnostics", strings.Repeat("Style * NeverFocus\n", 5), "diagnostic-limit", func(l *limits) { l.diagnostics = 3 }},
		{"depth", "Read child\n", "include-limit", func(l *limits) { l.depth = 1 }},
		{"visits", "Read child\nRead child\n", "include-limit", func(l *limits) { l.files = 2 }},
		{"time", "# empty\n", "time-limit", func(l *limits) { l.duration = 0 }},
	} {
		t.Run(tt.name, func(t *testing.T) {
			r, write := fixture(t)
			write("config", tt.text)
			write("child", "# child\n")
			l := defaultLimits()
			tt.change(&l)
			v := check(context.Background(), r, l)
			if v.Status != "partial" || v.Diagnostics[len(v.Diagnostics)-1].Code != tt.code {
				t.Fatalf("%+v", v)
			}
		})
	}
}
func TestEncodingAndContinuationEOF(t *testing.T) {
	r, write := fixture(t)
	write("config", "*FvwmDesktop: motion on\\\n")
	if checked(t, r).CheckedDirectives != 1 {
		t.Fatal("continuation at EOF")
	}
	write("config", "bad\x00\n\xff\n")
	v := checked(t, r)
	if v.Errors != 1 || v.Unchecked != 1 || v.Diagnostics[0].Code != "nul-byte" || v.Diagnostics[1].Code != "encoding" {
		t.Fatalf("%+v", v)
	}
}
func TestCancellationAndRequestBounds(t *testing.T) {
	r, write := fixture(t)
	write("config", "# empty\n")
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	v, err := Check(ctx, r)
	if err != nil || v.Status != "partial" || v.Diagnostics[0].Code != "time-limit" {
		t.Fatalf("%+v %v", v, err)
	}
	r.Path = strings.Repeat("a", 4097)
	if _, err = Check(context.Background(), r); err == nil {
		t.Fatal("oversize path accepted")
	}
	r.Path = "bad\x00path"
	if _, err = Check(context.Background(), r); err == nil {
		t.Fatal("NUL path accepted")
	}
}

func TestStrictRequest(t *testing.T) {
	for _, text := range []string{
		`{}`, `{"path":"a","path":"b","userdir":"u","datadir":"d","cwd":"c"}`,
		`{"path":"a","userdir":"u","datadir":"d","extra":"c"}`,
		`{"path":"a","userdir":"u","datadir":"d","cwd":null}`,
	} {
		if _, err := DecodeRequest([]byte(text)); err == nil {
			t.Fatalf("accepted %s", text)
		}
	}
}

func TestReadPreservesSymlinkParentTraversal(t *testing.T) {
	r, write := fixture(t)
	if err := os.MkdirAll(filepath.Join(r.Userdir, "sub", "deep"), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(filepath.Join(r.Userdir, "sub", "deep"), filepath.Join(r.Userdir, "link")); err != nil {
		t.Fatal(err)
	}
	write("config", "Read link/../choice\n")
	write("choice", "*FvwmDesktop: palette wrong\n")
	write("sub/choice", "*FvwmDesktop: palette ink\n")
	v := checked(t, r)
	if v.Errors != 0 || len(v.Files) != 2 || v.Files[1].Path != r.Userdir+"/link/../choice" {
		t.Fatalf("%+v", v)
	}
	r.Path = r.Userdir + "/link/../choice"
	if checked(t, r).Errors != 0 {
		t.Fatal("entry path was lexically cleaned")
	}
}

func TestDynamicGrammarCorpusNeverExecutesOrReadsGeneratedSource(t *testing.T) {
	r, write := fixture(t)
	marker := filepath.Join(r.Userdir, "executed")
	write("generated", "*FvwmDesktop: palette invalid\n")
	lines := []string{
		"Exec touch " + marker,
		"PipeRead 'touch " + marker + "; echo Read generated'",
		"Module /bin/sh -c 'touch " + marker + "'",
		"AddToFunc TestFunction I Exec touch " + marker,
		"+ I Read generated",
		"TestFunction",
		"Silent Read generated",
		"Read $[env.FVWM_TEST_SOURCE]",
		"Read `echo generated`",
		"Read generated Extra",
		"Read \"generated\\name\"",
		"*FvwmDesktop: palette $[env.FVWM_TEST_PALETTE]",
		"*OtherModule: Read generated",
	}
	write("config", strings.Join(lines, "\n")+"\n")
	v := checked(t, r)
	if v.Errors != 0 || v.Unchecked != len(lines) || v.CheckedDirectives != 0 || len(v.Files) != 1 {
		t.Fatalf("dynamic grammar claimed coverage: %+v", v)
	}
	if _, err := os.Stat(marker); !os.IsNotExist(err) {
		t.Fatal("checker executed input", err)
	}
}

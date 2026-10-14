// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"errors"
	"io"
	"os"
	"path/filepath"
	"strings"
	"syscall"
	"testing"
)

func TestOptions(t *testing.T) {
	t.Setenv("HOME", t.TempDir())
	for _, tc := range []struct {
		args   []string
		status int
	}{
		{[]string{"--ALL", "--no-all", "--lin", "--order=-6"}, 0},
		{[]string{"--dir"}, 255}, {[]string{"--i"}, 255},
		{[]string{"--all=yes"}, 255}, {[]string{"positional"}, 255},
		{[]string{"--order=not-a-number"}, 255}, {[]string{"--nowm-icons"}, 255},
	} {
		var out, diagnostic bytes.Buffer
		o, _, status := parse(tc.args, &out, &diagnostic, "tool")
		if status != tc.status {
			t.Fatalf("%v: %d %s", tc.args, status, diagnostic.String())
		}
		if status == 0 && (o.all || !o.links || o.order != -6) {
			t.Fatalf("options: %+v", o)
		}
	}
	var out, diagnostic bytes.Buffer
	o, done, status := parse([]string{"--exec-app", "--special-dirs", "--command-file", ""}, &out, &diagnostic, "tool")
	if done || status != 0 || o.execApp == nil || *o.execApp != "" || o.specialDirs == nil || *o.specialDirs != "" || o.commandFile == nil || *o.commandFile != "" {
		t.Fatalf("optional strings: %+v", o)
	}
}

func TestFormatting(t *testing.T) {
	for _, tc := range []struct{ format, name, want string }{
		{"%8n:%-8n", "abc", "     abc:abc     "},
		{"%*5n:%*-5n", "abcde", "ab...:...de"},
		{"%*3n:%q:%n", "a%n", "a%n:%q:a%n"},
		{`%n\t%T`, "name", "name\tF"},
	} {
		fields, err := plan(tc.format, "nNs", "dDtT")
		if err != nil {
			t.Fatal(err)
		}
		var out bytes.Buffer
		if err := formatted(&out, fields, map[string]string{"n": tc.name, "T": "F"}, false); err != nil {
			t.Fatal(err)
		}
		if out.String() != tc.want {
			t.Fatalf("%q: %q != %q", tc.format, out.String(), tc.want)
		}
	}
	if _, err := plan("%999999999999999999999999n", "n", ""); err == nil {
		t.Fatal("overflow accepted")
	}
	if got := collapseSlashes("///a////b//"); got != "/a/b/" {
		t.Fatal(got)
	}
	var out bytes.Buffer
	io.WriteString(labelWriter{&out}, "a%&*\"$\\\n")
	if out.String() != "a%%&&**\\\"$$\\\\ " {
		t.Fatal(out.String())
	}
}

type failedWriter struct{}

func (failedWriter) Write([]byte) (int, error) { return 0, errors.New("fixture failure") }
func TestOutputFailure(t *testing.T) {
	fields, _ := plan("%99n", "n", "")
	if formatted(failedWriter{}, fields, map[string]string{"n": "abc"}, false) == nil {
		t.Fatal("lost writer error")
	}
	if _, err := (labelWriter{failedWriter{}}).Write([]byte("quote\"")); err == nil {
		t.Fatal("lost label error")
	}
}

func TestListing(t *testing.T) {
	dir := t.TempDir()
	for _, name := range []string{"b", "a", ".hidden"} {
		if err := os.WriteFile(filepath.Join(dir, name), []byte("abc"), 0600); err != nil {
			t.Fatal(err)
		}
	}
	if err := os.Mkdir(filepath.Join(dir, "folder"), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink("folder", filepath.Join(dir, "link")); err != nil {
		t.Fatal(err)
	}
	o := options{dir: dir, order: 5}
	entries, err := listing(dir, o)
	if err != nil {
		t.Fatal(err)
	}
	var names []string
	for _, e := range entries {
		names = append(names, e.name)
	}
	if strings.Join(names, ",") != "folder,link,a,b" {
		t.Fatal(names)
	}
	if !entries[1].link || !entries[1].directory {
		t.Fatal(entries[1])
	}
	fields, _ := plan("%s:%T", "nNs", "dDtT")
	if values(entries[1], fields)["T"] != "L" || values(entries[2], fields)["s"] != "3" {
		t.Fatal("metadata")
	}
	o.order = -4
	o.all = true
	entries, err = listing(dir, o)
	if err != nil {
		t.Fatal(err)
	}
	if entries[0].name != "link" || entries[len(entries)-1].name != ".hidden" {
		t.Fatal(entries)
	}
}

func TestRememberedNavigation(t *testing.T) {
	// remember deliberately changes cwd in its isolated invocation: no parallel tests.
	dir := t.TempDir()
	cwd, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	path := "/a literal ' quoted $ path"
	if err := remember(dir, path); err != nil {
		t.Fatal(err)
	}
	got, err := readRemembered(dir)
	if err != nil || got != path {
		t.Fatalf("%q %v", got, err)
	}
	info, err := os.Stat(filepath.Join(dir, stateName))
	if err != nil || info.Mode().Perm() != 0600 {
		t.Fatalf("mode %v %v", info, err)
	}
	if now, _ := os.Getwd(); now != cwd {
		t.Fatalf("cwd %q", now)
	}
	original := filepath.Join(dir, "old-state")
	if err := os.Link(filepath.Join(dir, stateName), original); err != nil {
		t.Fatal(err)
	}
	if err := remember(dir, "/next"); err != nil {
		t.Fatal(err)
	}
	data, _ := os.ReadFile(original)
	if string(data) != path+"\n" {
		t.Fatal("overwrote linked inode")
	}
	if err := remember(dir, "line\nbreak"); err == nil {
		t.Fatal("accepted multiline")
	}
	if err := os.Chmod(dir, 0770); err != nil {
		t.Fatal(err)
	}
	if err := remember(dir, "/refused"); err == nil {
		t.Fatal("accepted writable directory")
	}
	os.Chmod(dir, 0700)
}

func TestStateRefusals(t *testing.T) {
	for _, kind := range []string{"symlink", "fifo", "writable", "large", "directory"} {
		t.Run(kind, func(t *testing.T) {
			dir := t.TempDir()
			state := filepath.Join(dir, stateName)
			switch kind {
			case "symlink":
				if err := os.Symlink("missing", state); err != nil {
					t.Fatal(err)
				}
			case "fifo":
				if err := syscall.Mkfifo(state, 0600); err != nil {
					t.Fatal(err)
				}
			case "directory":
				if err := os.Mkdir(state, 0700); err != nil {
					t.Fatal(err)
				}
			default:
				data := []byte("/previous\n")
				if kind == "large" {
					data = bytes.Repeat([]byte("x"), stateLimit+1)
				}
				if err := os.WriteFile(state, data, 0600); err != nil {
					t.Fatal(err)
				}
				if kind == "writable" {
					if err := os.Chmod(state, 0666); err != nil {
						t.Fatal(err)
					}
				}
			}
			if _, err := readRemembered(dir); err == nil {
				t.Fatal("accepted state")
			}
			if kind != "large" {
				if err := remember(dir, "/next"); err == nil {
					t.Fatal("accepted destination")
				}
			}
		})
	}
	if got, err := readRemembered(t.TempDir()); err != nil || got != "" {
		t.Fatal(got, err)
	}
}

func TestEnumerationBoundIncludesHidden(t *testing.T) {
	dir := t.TempDir()
	for _, name := range []string{".a", ".b", ".c"} {
		if err := os.WriteFile(filepath.Join(dir, name), nil, 0600); err != nil {
			t.Fatal(err)
		}
	}
	if entries, err := listingBounded(dir, options{}, 2); err == nil || entries != nil {
		t.Fatal("hidden entries bypassed limit")
	}
	if entries, err := listingBounded(dir, options{}, 3); err != nil || len(entries) != 0 {
		t.Fatal(entries, err)
	}
}

func TestExistingPlainState(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, stateName), []byte("/legacy\n"), 0644); err != nil {
		t.Fatal(err)
	}
	if got, err := readRemembered(dir); err != nil || got != "/legacy" {
		t.Fatal(got, err)
	}
	var out, diagnostic bytes.Buffer
	if status := run([]string{"--fvwm-remember", dir, "/next", "line\nbreak"}, &out, &diagnostic, "tool"); status != 1 || out.Len() != 0 {
		t.Fatal(status, out.String())
	}
	if got, _ := readRemembered(dir); got != "/legacy" {
		t.Fatal("invalid request changed state")
	}
	fifo := filepath.Join(dir, "parent-fifo")
	if err := syscall.Mkfifo(fifo, 0600); err != nil {
		t.Fatal(err)
	}
	if err := remember(fifo, "/next"); err == nil {
		t.Fatal("accepted nondirectory")
	}
}

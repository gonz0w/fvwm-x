//go:build go1.25

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filejobs

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"golang.org/x/sys/unix"
)

// These tests never mount filesystems. The target owner explicitly supplies
// prepared private roots and marker files; unset roots are recorded as skips.
func fixtureRoot(t *testing.T, variable, kind string) string {
	t.Helper()
	root := os.Getenv(variable)
	if root == "" {
		t.Skip(variable + " is not provisioned")
	}
	if !validPath([]byte(root)) {
		t.Fatal("invalid owned fixture root")
	}
	info, err := os.Lstat(root)
	if err != nil || !info.IsDir() {
		t.Fatal("fixture root must be a real directory", err)
	}
	marker, err := os.ReadFile(filepath.Join(root, ".fvwm-filejobs-fixture"))
	if err != nil || string(marker) != "FVWM-FILEJOBS "+kind+"\n" {
		t.Fatal("missing exact owned fixture marker", err)
	}
	return root
}
func fixtureTemp(t *testing.T, parent string) string {
	t.Helper()
	p, err := os.MkdirTemp(parent, "fvwm-filejobs-test-")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		if filepath.Dir(p) != parent || !strings.HasPrefix(filepath.Base(p), "fvwm-filejobs-test-") {
			t.Fatal("unowned cleanup path")
		}
		if err := os.RemoveAll(p); err != nil {
			t.Error(err)
		}
	})
	return p
}

func TestNativeCrossFilesystemMoveAndTrashRefusal(t *testing.T) {
	root := fixtureRoot(t, "FVWM_FILEJOBS_TEST_OTHER_FS", "other-fs")
	dir, dest := t.TempDir(), fixtureTemp(t, root)
	a, err := os.Stat(dir)
	if err != nil {
		t.Fatal(err)
	}
	b, err := os.Stat(dest)
	if err != nil {
		t.Fatal(err)
	}
	if stamp(a).Device == stamp(b).Device {
		t.Fatal("other-fs fixture must have a distinct device")
	}
	write(t, filepath.Join(dir, "move-item"), []byte("cross filesystem"))
	req := request(t, dir, "move", "move-item")
	req.Destination = []byte(dest)
	out := outcome(t, run(t, req, nil))
	if out.State != "completed" || out.SourceRemaining || !out.MetadataComplete || !strings.Contains(out.Detail, "copy/remove") {
		t.Fatal(out)
	}
	absent(t, filepath.Join(dir, "move-item"))
	if contents(t, filepath.Join(dest, "move-item")) != "cross filesystem" {
		t.Fatal(out)
	}
	t.Setenv("XDG_DATA_HOME", fixtureTemp(t, root))
	write(t, filepath.Join(dir, "trash-item"), []byte("must remain"))
	out = outcome(t, run(t, request(t, dir, "trash", "trash-item"), nil))
	if out.State != "failed" || !out.SourceRemaining || out.DestinationPresent || contents(t, filepath.Join(dir, "trash-item")) != "must remain" {
		t.Fatal(out)
	}
}

func TestNativeFullDestinationPreservesSource(t *testing.T) {
	parent := fixtureRoot(t, "FVWM_FILEJOBS_TEST_FULL_FS", "full-fs")
	dir, dest := t.TempDir(), fixtureTemp(t, parent)
	file, err := os.Create(filepath.Join(dir, "item"))
	if err != nil {
		t.Fatal(err)
	}
	block := bytes.Repeat([]byte("x"), ChunkBytes)
	for i := 0; i < 512; i++ {
		if _, err = file.Write(block); err != nil {
			file.Close()
			t.Fatal(err)
		}
	}
	if err = file.Close(); err != nil {
		t.Fatal(err)
	}
	req := request(t, dir, "copy", "item")
	req.Destination = []byte(dest)
	source, err := openObserved(req)
	if err != nil {
		t.Fatal(err)
	}
	defer source.Close()
	target, err := os.OpenRoot(dest)
	if err != nil {
		t.Fatal(err)
	}
	defer target.Close()
	r := runner{ctx: context.Background(), req: req, emit: func(Event) error { return nil }}
	nodes, err := plan(source, "item", &r)
	if err != nil {
		t.Fatal(err)
	}
	out, err := r.copy(source, target, nodes, "item")
	if !errors.Is(err, unix.ENOSPC) {
		t.Fatalf("fixture must produce actual ENOSPC, got %v", err)
	}
	if !out.SourceRemaining || !out.DestinationPresent || out.ContentComplete || stateFor(err, true) != "partial" {
		t.Fatal(out)
	}
	info, err := os.Stat(filepath.Join(dir, "item"))
	if err != nil || info.Size() != 32*1024*1024 {
		t.Fatal("source lost after full destination", err)
	}
}

func TestNativeReadOnlyFilesystemRefusal(t *testing.T) {
	root := fixtureRoot(t, "FVWM_FILEJOBS_TEST_READONLY_FS", "readonly-fs")
	dir := t.TempDir()
	write(t, filepath.Join(dir, "item"), []byte("source remains"))
	name := filepath.Base(dir) + "-read-only-probe"
	f, err := os.OpenFile(filepath.Join(root, name), os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if err == nil {
		f.Close()
		os.Remove(filepath.Join(root, name))
		t.Fatal("readonly-fs fixture was writable")
	}
	if !errors.Is(err, unix.EROFS) {
		t.Fatalf("fixture must produce EROFS rather than mode denial: %v", err)
	}
	req := request(t, dir, "copy", "item")
	req.Destination = []byte(root)
	req.Name = []byte(name)
	out := outcome(t, run(t, req, nil))
	if out.State != "failed" || out.DestinationPresent || !out.SourceRemaining || contents(t, filepath.Join(dir, "item")) != "source remains" {
		t.Fatal(out)
	}
}

func TestNativeCrossFilesystemMoveDestinationReplacement(t *testing.T) {
	parent := fixtureRoot(t, "FVWM_FILEJOBS_TEST_OTHER_FS", "other-fs")
	for _, replace := range []bool{true, false} {
		t.Run(fmt.Sprint(replace), func(t *testing.T) {
			dir, dest := t.TempDir(), fixtureTemp(t, parent)
			a, err := os.Stat(dir)
			if err != nil {
				t.Fatal(err)
			}
			b, err := os.Stat(dest)
			if err != nil {
				t.Fatal(err)
			}
			if stamp(a).Device == stamp(b).Device {
				t.Fatal("other-fs must be a different device")
			}
			original := bytes.Repeat([]byte("source"), ChunkBytes)
			write(t, filepath.Join(dir, "item"), original)
			req := request(t, dir, "move", "item")
			req.Destination = []byte(dest)
			changed := false
			when := time.Unix(1234567890, 0)
			out := outcome(t, run(t, req, func(e Event, _ chan<- Control) {
				if e.Kind == "progress" && e.Bytes > 0 && !changed {
					changed = true
					if err := os.Rename(filepath.Join(dest, "item"), filepath.Join(dest, "owned-partial")); err != nil {
						t.Fatal(err)
					}
					if replace {
						write(t, filepath.Join(dest, "item"), []byte("replacement"))
						if err := os.Chtimes(filepath.Join(dest, "item"), when, when); err != nil {
							t.Fatal(err)
						}
					}
				}
			}))
			if !changed || out.State != "partial" || !out.SourceRemaining || out.ContentComplete {
				t.Fatal(out)
			}
			if !bytes.Equal([]byte(contents(t, filepath.Join(dir, "item"))), original) {
				t.Fatal("source removed after destination replacement")
			}
			if replace {
				if contents(t, filepath.Join(dest, "item")) != "replacement" {
					t.Fatal("replacement touched")
				}
				info, err := os.Stat(filepath.Join(dest, "item"))
				if err != nil || !info.ModTime().Equal(when) {
					t.Fatal("replacement metadata touched", err)
				}
			} else {
				absent(t, filepath.Join(dest, "item"))
			}
		})
	}
}

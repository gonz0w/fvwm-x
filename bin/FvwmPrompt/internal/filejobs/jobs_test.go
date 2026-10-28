//go:build go1.25

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filejobs

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"strings"
	"syscall"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

func write(t *testing.T, path string, data []byte) {
	t.Helper()
	if err := os.WriteFile(path, data, 0640); err != nil {
		t.Fatal(err)
	}
}
func request(t *testing.T, dir, op string, names ...string) Request {
	t.Helper()
	s, err := filemodel.Scan(dir, true)
	if err != nil {
		t.Fatal(err)
	}
	r := Request{Schema: Schema, ID: "job-test", Generation: 1, Operation: op, Snapshot: s}
	for _, name := range names {
		found := false
		for _, item := range s.Items {
			if string(item.Name) == name {
				r.IDs = append(r.IDs, item.ID)
				found = true
				break
			}
		}
		if !found {
			t.Fatalf("missing fixture %q", name)
		}
	}
	return r
}
func run(t *testing.T, req Request, hook func(Event, chan<- Control)) []Event {
	t.Helper()
	if req.Operation == "delete" && !strings.Contains(string(req.Snapshot.Path), string(filepath.Separator)+"Trash"+string(filepath.Separator)+"files") {
		t.Setenv("XDG_DATA_HOME", t.TempDir())
	}
	controls := make(chan Control, 4)
	events := []Event{}
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	err := Run(ctx, req, controls, func(e Event) error {
		events = append(events, e)
		if hook != nil {
			hook(e, controls)
		} else if e.Kind == "decision" {
			controls <- Control{Schema: Schema, ID: req.ID, Kind: "decide", Decision: e.Decision, Choice: "skip"}
		}
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	for i, e := range events {
		if e.Schema != Schema || e.ID != req.ID || e.Generation != req.Generation || e.Sequence != uint64(i+1) {
			t.Fatalf("bad event envelope %#v", e)
		}
	}
	if len(events) < 2 || events[len(events)-1].Kind != "finished" {
		t.Fatal(events)
	}
	return events
}
func outcome(t *testing.T, events []Event) Outcome {
	t.Helper()
	for _, e := range events {
		if e.Kind == "item" && e.Outcome != nil {
			return *e.Outcome
		}
	}
	t.Fatal("missing item outcome", events)
	return Outcome{}
}
func contents(t *testing.T, path string) string {
	t.Helper()
	b, e := os.ReadFile(path)
	if e != nil {
		t.Fatal(e)
	}
	return string(b)
}
func absent(t *testing.T, path string) {
	t.Helper()
	if _, e := os.Lstat(path); !errors.Is(e, os.ErrNotExist) {
		t.Fatalf("expected absent %q: %v", path, e)
	}
}

func TestOperations(t *testing.T) {
	for _, op := range []string{"mkdir", "copy", "move", "rename", "link", "delete"} {
		t.Run(op, func(t *testing.T) {
			dir := t.TempDir()
			dest := t.TempDir()
			name := "a; quote'\n"
			write(t, filepath.Join(dir, name), []byte("original bytes"))
			req := request(t, dir, op, name)
			switch op {
			case "mkdir":
				req.IDs = nil
				req.Name = []byte("new folder")
			case "rename":
				req.Name = []byte("renamed")
			case "copy", "move", "link":
				req.Destination = []byte(dest)
			case "delete":
				req.Permanent = true
			}
			events := run(t, req, nil)
			out := outcome(t, events)
			if out.State != "completed" {
				t.Fatal(out)
			}
			switch op {
			case "mkdir":
				i, e := os.Stat(filepath.Join(dir, "new folder"))
				if e != nil || !i.IsDir() {
					t.Fatal(i, e)
				}
			case "rename":
				if contents(t, filepath.Join(dir, "renamed")) != "original bytes" {
					t.Fatal(out)
				}
				absent(t, filepath.Join(dir, name))
				if !strings.Contains(out.Detail, "link/unlink") {
					t.Fatal(out)
				}
			case "move":
				if contents(t, filepath.Join(dest, name)) != "original bytes" {
					t.Fatal(out)
				}
				absent(t, filepath.Join(dir, name))
			case "copy":
				if contents(t, filepath.Join(dest, name)) != "original bytes" || contents(t, filepath.Join(dir, name)) != "original bytes" {
					t.Fatal(out)
				}
			case "link":
				target, e := os.Readlink(filepath.Join(dest, name))
				if e != nil || target != filepath.Join(dir, name) {
					t.Fatal(target, e)
				}
			case "delete":
				absent(t, filepath.Join(dir, name))
			}
		})
	}
}

func TestDirectoryCopyMoveDeleteAndSymlinks(t *testing.T) {
	for _, op := range []string{"copy", "move", "rename", "delete"} {
		t.Run(op, func(t *testing.T) {
			dir, dest, outside := t.TempDir(), t.TempDir(), t.TempDir()
			if e := os.MkdirAll(filepath.Join(dir, "tree", "sub"), 0750); e != nil {
				t.Fatal(e)
			}
			write(t, filepath.Join(dir, "tree", "sub", "data"), []byte("data"))
			write(t, filepath.Join(outside, "untouched"), []byte("outside"))
			if e := os.Symlink(outside, filepath.Join(dir, "tree", "outside-link")); e != nil {
				t.Fatal(e)
			}
			req := request(t, dir, op, "tree")
			if op == "copy" || op == "move" {
				req.Destination = []byte(dest)
			}
			if op == "rename" {
				req.Name = []byte("newtree")
				dest = dir
			}
			if op == "delete" {
				req.Permanent = true
			}
			out := outcome(t, run(t, req, nil))
			if out.State != "completed" {
				t.Fatal(out)
			}
			if op != "delete" {
				name := "tree"
				if op == "rename" {
					name = "newtree"
				}
				if contents(t, filepath.Join(dest, name, "sub", "data")) != "data" {
					t.Fatal(out)
				}
				link, e := os.Readlink(filepath.Join(dest, name, "outside-link"))
				if e != nil || link != outside {
					t.Fatal(link, e)
				}
			}
			if op != "copy" {
				absent(t, filepath.Join(dir, "tree"))
			}
			if contents(t, filepath.Join(outside, "untouched")) != "outside" {
				t.Fatal("followed symlink")
			}
		})
	}
}

func TestConflictDecisionsDoNotOverwrite(t *testing.T) {
	for _, choice := range []string{"skip", "rename", "cancel"} {
		t.Run(choice, func(t *testing.T) {
			dir, dest := t.TempDir(), t.TempDir()
			write(t, filepath.Join(dir, "item"), []byte("new"))
			write(t, filepath.Join(dest, "item"), []byte("old"))
			req := request(t, dir, "copy", "item")
			req.Destination = []byte(dest)
			events := run(t, req, func(e Event, c chan<- Control) {
				if e.Kind == "decision" {
					c <- Control{Schema: Schema, ID: req.ID, Kind: "decide", Decision: e.Decision, Choice: choice, Name: []byte("alternate")}
				}
			})
			out := outcome(t, events)
			if contents(t, filepath.Join(dest, "item")) != "old" || contents(t, filepath.Join(dir, "item")) != "new" {
				t.Fatal("overwrote existing object")
			}
			want := map[string]string{"skip": "skipped", "rename": "completed", "cancel": "cancelled"}[choice]
			if out.State != want {
				t.Fatal(out)
			}
			if choice == "rename" && contents(t, filepath.Join(dest, "alternate")) != "new" {
				t.Fatal(out)
			}
		})
	}
}

func TestStaleIdentityAndRootRefuseBeforeMutation(t *testing.T) {
	for _, which := range []string{"item", "root"} {
		t.Run(which, func(t *testing.T) {
			base, dest := t.TempDir(), t.TempDir()
			dir := filepath.Join(base, "source")
			if e := os.Mkdir(dir, 0700); e != nil {
				t.Fatal(e)
			}
			write(t, filepath.Join(dir, "item"), []byte("old"))
			req := request(t, dir, "move", "item")
			req.Destination = []byte(dest)
			if which == "item" {
				if e := os.Rename(filepath.Join(dir, "item"), filepath.Join(dir, "saved")); e != nil {
					t.Fatal(e)
				}
				write(t, filepath.Join(dir, "item"), []byte("replaced"))
			} else {
				if e := os.Rename(dir, dir+"-old"); e != nil {
					t.Fatal(e)
				}
				if e := os.Mkdir(dir, 0700); e != nil {
					t.Fatal(e)
				}
				write(t, filepath.Join(dir, "item"), []byte("replacement root"))
			}
			events := run(t, req, nil)
			if events[len(events)-1].State != "failed" {
				t.Fatal(events)
			}
			absent(t, filepath.Join(dest, "item"))
			if !strings.Contains(contents(t, filepath.Join(dir, "item")), "replace") {
				t.Fatal("replacement changed")
			}
		})
	}
}

func TestConcurrentSourceChangeRetainsSourceAndPartial(t *testing.T) {
	dir, dest := t.TempDir(), t.TempDir()
	source := filepath.Join(dir, "item")
	write(t, source, bytes.Repeat([]byte("a"), 4*ChunkBytes))
	req := request(t, dir, "move", "item")
	req.Destination = []byte(dest)
	// Copy is used here to force the streaming path rather than same-FS link.
	req.Operation = "copy"
	changed := false
	events := run(t, req, func(e Event, _ chan<- Control) {
		if e.Kind == "progress" && e.Bytes > 0 && !changed {
			changed = true
			write(t, source, []byte("externally changed"))
		}
	})
	if !changed {
		t.Fatal("no streaming progress observed")
	}
	out := outcome(t, events)
	if out.State != "partial" || !out.SourceRemaining || !out.DestinationPresent || out.ContentComplete {
		t.Fatal(out)
	}
	if contents(t, source) != "externally changed" {
		t.Fatal("removed changed source")
	}
}

func TestCancellationRetainsSourceAndPartial(t *testing.T) {
	dir, dest := t.TempDir(), t.TempDir()
	write(t, filepath.Join(dir, "item"), bytes.Repeat([]byte("a"), 4*ChunkBytes))
	req := request(t, dir, "copy", "item")
	req.Destination = []byte(dest)
	sent := false
	events := run(t, req, func(e Event, c chan<- Control) {
		if e.Kind == "progress" && e.Bytes > 0 && !sent {
			sent = true
			c <- Control{Schema: Schema, ID: req.ID, Kind: "cancel"}
		}
	})
	out := outcome(t, events)
	if !sent || out.State != "partial" || !out.SourceRemaining || !out.DestinationPresent || events[len(events)-1].State != "partial" {
		t.Fatal(out, events[len(events)-1])
	}
	if len(contents(t, filepath.Join(dir, "item"))) != 4*ChunkBytes {
		t.Fatal("source changed")
	}
}

func TestTreeAndDestinationBounds(t *testing.T) {
	dir := t.TempDir()
	if e := os.Mkdir(filepath.Join(dir, "tree"), 0700); e != nil {
		t.Fatal(e)
	}
	write(t, filepath.Join(dir, "tree", "item"), []byte("x"))
	req := request(t, dir, "copy", "tree")
	req.Destination = []byte(filepath.Join(dir, "tree"))
	out := outcome(t, run(t, req, nil))
	if out.State != "failed" || out.DestinationPresent {
		t.Fatal(out)
	}
	deep := filepath.Join(dir, "deep")
	if e := os.Mkdir(deep, 0700); e != nil {
		t.Fatal(e)
	}
	for i := 0; i < MaxDepth+1; i++ {
		deep = filepath.Join(deep, "x")
		if e := os.Mkdir(deep, 0700); e != nil {
			t.Fatal(e)
		}
	}
	req = request(t, dir, "delete", "deep")
	req.Permanent = true
	events := run(t, req, nil)
	if events[len(events)-1].State != "failed" {
		t.Fatal(events)
	}
	if _, e := os.Stat(deep); e != nil {
		t.Fatal("removed overlimit tree", e)
	}
}

func TestRequestAdmission(t *testing.T) {
	dir := t.TempDir()
	write(t, filepath.Join(dir, "item"), []byte("x"))
	base := request(t, dir, "copy", "item")
	base.Destination = []byte(t.TempDir())
	cases := []func(*Request){func(r *Request) { r.Schema = 0 }, func(r *Request) { r.ID = "" }, func(r *Request) { r.Generation = 0 }, func(r *Request) { r.Operation = "shell" }, func(r *Request) { r.Destination = []byte("relative") }, func(r *Request) { r.Name = []byte("../escape") }, func(r *Request) { r.IDs = append(r.IDs, r.IDs[0]) }, func(r *Request) { r.Operation = "delete"; r.Destination = nil }, func(r *Request) { r.Permanent = true }, func(r *Request) { r.Snapshot = nil }}
	for i, change := range cases {
		r := base
		r.IDs = append([]string{}, base.IDs...)
		change(&r)
		if ValidateRequest(r) == nil {
			t.Fatalf("admitted invalid case %d", i)
		}
	}
}

func TestTrashRestorePrivateMetadataAndConflict(t *testing.T) {
	dir, data := t.TempDir(), t.TempDir()
	t.Setenv("XDG_DATA_HOME", data)
	name := "raw%\n"
	write(t, filepath.Join(dir, name), []byte("recover me"))
	req := request(t, dir, "trash", name)
	events := run(t, req, nil)
	out := outcome(t, events)
	if out.State != "completed" || out.SourceRemaining {
		t.Fatal(out)
	}
	absent(t, filepath.Join(dir, name))
	files := filepath.Join(data, "Trash", "files")
	entries, e := os.ReadDir(files)
	if e != nil || len(entries) != 1 {
		t.Fatal(entries, e)
	}
	token := entries[0].Name()
	raw, e := os.ReadFile(filepath.Join(data, "Trash", "info", token+".trashinfo"))
	if e != nil {
		t.Fatal(e)
	}
	original, e := parseTrash(raw)
	if e != nil || string(original) != filepath.Join(dir, name) {
		t.Fatal(original, e)
	}
	restore := request(t, files, "restore", token)
	out = outcome(t, run(t, restore, nil))
	if out.State != "completed" || out.SourceRemaining || contents(t, filepath.Join(dir, name)) != "recover me" {
		t.Fatal(out)
	}
	absent(t, filepath.Join(files, token))
	absent(t, filepath.Join(data, "Trash", "info", token+".trashinfo"))
	if _, e = parseTrash([]byte("[Trash Info]\nPath=/tmp/x\nDeletionDate=bad\n")); e == nil {
		t.Fatal("accepted invalid metadata")
	}
}

func TestTrashRefusesUnsafePrivateArea(t *testing.T) {
	dir, data := t.TempDir(), t.TempDir()
	t.Setenv("XDG_DATA_HOME", data)
	write(t, filepath.Join(dir, "item"), []byte("keep"))
	if e := os.Mkdir(filepath.Join(data, "Trash"), 0755); e != nil {
		t.Fatal(e)
	}
	out := outcome(t, run(t, request(t, dir, "trash", "item"), nil))
	if out.State != "failed" || contents(t, filepath.Join(dir, "item")) != "keep" {
		t.Fatal(out)
	}
}

func TestReadOnlyDestinationRefusal(t *testing.T) {
	if os.Geteuid() == 0 {
		t.Skip("root bypasses mode-bit denial; requires non-root native fixture")
	}
	dir, dest := t.TempDir(), t.TempDir()
	write(t, filepath.Join(dir, "item"), []byte("keep"))
	if e := os.Chmod(dest, 0500); e != nil {
		t.Fatal(e)
	}
	defer os.Chmod(dest, 0700)
	req := request(t, dir, "copy", "item")
	req.Destination = []byte(dest)
	out := outcome(t, run(t, req, nil))
	if out.State != "failed" || out.DestinationPresent || contents(t, filepath.Join(dir, "item")) != "keep" {
		t.Fatal(out)
	}
}

func TestStreamLimitsAndControlCancellation(t *testing.T) {
	for _, data := range [][]byte{[]byte("{}\n"), []byte("{\"unknown\":1}\n"), append(bytes.Repeat([]byte("x"), MaxRequest), '\n')} {
		if Serve(bytes.NewReader(data), io.Discard) == nil {
			t.Fatal("accepted malformed/overlimit record")
		}
	}
	dir, dest := t.TempDir(), t.TempDir()
	write(t, filepath.Join(dir, "item"), []byte("x"))
	write(t, filepath.Join(dest, "item"), []byte("existing"))
	req := request(t, dir, "copy", "item")
	req.Destination = []byte(dest)
	inputR, inputW := io.Pipe()
	outputR, outputW := io.Pipe()
	done := make(chan error, 1)
	go func() { defer outputW.Close(); done <- Serve(inputR, outputW) }()
	if e := json.NewEncoder(inputW).Encode(req); e != nil {
		t.Fatal(e)
	}
	decoder := json.NewDecoder(outputR)
	events := []Event{}
	for {
		var e Event
		if err := decoder.Decode(&e); err != nil {
			t.Fatal(err)
		}
		events = append(events, e)
		if e.Kind == "decision" {
			if err := json.NewEncoder(inputW).Encode(Control{Schema: Schema, ID: req.ID, Kind: "cancel"}); err != nil {
				t.Fatal(err)
			}
		}
		if e.Kind == "finished" {
			break
		}
	}
	if e := <-done; e != nil {
		t.Fatal(e)
	}
	inputW.Close()
	outputR.Close()
	if events[len(events)-1].State != "cancelled" {
		t.Fatal(events)
	}
	if contents(t, filepath.Join(dest, "item")) != "existing" {
		t.Fatal("changed conflict")
	}
}

func TestWriteLimitChild(t *testing.T) {
	if os.Getenv("FILEJOBS_WRITE_LIMIT_CHILD") == "" {
		return
	}
	dir := os.Getenv("FILEJOBS_WRITE_LIMIT_SOURCE")
	dest := os.Getenv("FILEJOBS_WRITE_LIMIT_DEST")
	if !strings.Contains(dir, "TestWriteLimitPreservesSource") || !strings.Contains(dest, "TestWriteLimitPreservesSource") {
		t.Fatal("unowned test fixture")
	}
	if e := syscall.Setrlimit(syscall.RLIMIT_FSIZE, &syscall.Rlimit{Cur: 1024, Max: 1024}); e != nil {
		t.Fatal(e)
	}
	req := request(t, dir, "copy", "item")
	req.Destination = []byte(dest)
	out := outcome(t, run(t, req, nil))
	if out.State != "partial" || !out.SourceRemaining || !out.DestinationPresent || out.ContentComplete {
		t.Fatal(out)
	}
	if len(contents(t, filepath.Join(dir, "item"))) != 4096 {
		t.Fatal("source lost")
	}
}

func TestWriteLimitPreservesSource(t *testing.T) {
	dir, dest := t.TempDir(), t.TempDir()
	write(t, filepath.Join(dir, "item"), bytes.Repeat([]byte("x"), 4096))
	cmd := exec.Command(os.Args[0], "-test.run=^TestWriteLimitChild$")
	cmd.Env = append(os.Environ(), "FILEJOBS_WRITE_LIMIT_CHILD=1", "FILEJOBS_WRITE_LIMIT_SOURCE="+dir, "FILEJOBS_WRITE_LIMIT_DEST="+dest)
	out, e := cmd.CombinedOutput()
	if e != nil {
		t.Fatalf("write-limit child: %v\n%s", e, out)
	}
}

func TestMetadataPreservation(t *testing.T) {
	dir, dest := t.TempDir(), t.TempDir()
	path := filepath.Join(dir, "item")
	write(t, path, []byte("x"))
	when := time.Unix(1234567890, 0)
	if e := os.Chtimes(path, when, when); e != nil {
		t.Fatal(e)
	}
	req := request(t, dir, "copy", "item")
	req.Destination = []byte(dest)
	out := outcome(t, run(t, req, nil))
	before, e := os.Stat(path)
	if e != nil {
		t.Fatal(e)
	}
	after, e := os.Stat(filepath.Join(dest, "item"))
	if e != nil {
		t.Fatal(e)
	}
	if !out.MetadataComplete || before.Mode().Perm() != after.Mode().Perm() || !reflect.DeepEqual(before.ModTime(), after.ModTime()) {
		t.Fatal(out, before, after)
	}
}

func TestInvalidUTF8FilenameBytes(t *testing.T) {
	dir, dest := t.TempDir(), t.TempDir()
	name := "bytes-\xff"
	path := filepath.Join(dir, name)
	if err := os.WriteFile(path, []byte("raw bytes"), 0600); err != nil {
		t.Skipf("fixture filesystem cannot store invalid UTF-8 names: %q", err)
	}
	req := request(t, dir, "copy", name)
	req.Destination = []byte(dest)
	out := outcome(t, run(t, req, nil))
	if out.State != "completed" || contents(t, filepath.Join(dest, name)) != "raw bytes" {
		t.Fatal(out)
	}
	original, err := parseTrash(trashRecord([]byte(path)))
	if err != nil || !bytes.Equal(original, []byte(path)) {
		t.Fatal(original, err)
	}
}

func TestDescriptorLimitChild(t *testing.T) {
	if os.Getenv("FILEJOBS_DESCRIPTOR_CHILD") == "" {
		return
	}
	dir, dest := os.Getenv("FILEJOBS_DESCRIPTOR_SOURCE"), os.Getenv("FILEJOBS_DESCRIPTOR_DEST")
	if !strings.Contains(dir, "TestDescriptorBudget") || !strings.Contains(dest, "TestDescriptorBudget") {
		t.Fatal("unowned test fixture")
	}
	if err := syscall.Setrlimit(syscall.RLIMIT_NOFILE, &syscall.Rlimit{Cur: 96, Max: 96}); err != nil {
		t.Fatal(err)
	}
	req := request(t, dir, "copy", "tree")
	req.Destination = []byte(dest)
	out := outcome(t, run(t, req, nil))
	if out.State != "completed" {
		t.Fatal(out)
	}
}

func TestDescriptorBudget(t *testing.T) {
	dir, dest := t.TempDir(), t.TempDir()
	tree := filepath.Join(dir, "tree")
	if err := os.Mkdir(tree, 0700); err != nil {
		t.Fatal(err)
	}
	for i := 0; i < 512; i++ {
		write(t, filepath.Join(tree, fmt.Sprintf("item-%04d", i)), []byte("x"))
	}
	cmd := exec.Command(os.Args[0], "-test.run=^TestDescriptorLimitChild$")
	cmd.Env = append(os.Environ(), "FILEJOBS_DESCRIPTOR_CHILD=1", "FILEJOBS_DESCRIPTOR_SOURCE="+dir, "FILEJOBS_DESCRIPTOR_DEST="+dest)
	output, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("descriptor child: %v\n%s", err, output)
	}
}

func TestOpenedInodeSurvivesPathReplacement(t *testing.T) {
	dir, dest := t.TempDir(), t.TempDir()
	source := filepath.Join(dir, "item")
	original := bytes.Repeat([]byte("a"), 4*ChunkBytes)
	write(t, source, original)
	req := request(t, dir, "copy", "item")
	req.Destination = []byte(dest)
	changed := false
	events := run(t, req, func(e Event, _ chan<- Control) {
		if e.Kind == "progress" && e.Bytes > 0 && !changed {
			changed = true
			if err := os.Rename(source, source+"-observed"); err != nil {
				t.Fatal(err)
			}
			write(t, source, []byte("replacement"))
		}
	})
	out := outcome(t, events)
	if !changed || out.State != "partial" || out.SourceRemaining || contents(t, source) != "replacement" {
		t.Fatal(out)
	}
	if !bytes.Equal([]byte(contents(t, filepath.Join(dest, "item"))), original) || !bytes.Equal([]byte(contents(t, source+"-observed")), original) {
		t.Fatal("copied replacement instead of the opened inode")
	}
}

func TestEmitterLossRetainsPartialAndSource(t *testing.T) {
	dir, dest := t.TempDir(), t.TempDir()
	write(t, filepath.Join(dir, "item"), bytes.Repeat([]byte("x"), 4*ChunkBytes))
	req := request(t, dir, "copy", "item")
	req.Destination = []byte(dest)
	err := Run(context.Background(), req, nil, func(e Event) error {
		if e.Kind == "progress" && e.Bytes > 0 {
			return io.ErrClosedPipe
		}
		return nil
	})
	if !errors.Is(err, io.ErrClosedPipe) {
		t.Fatal(err)
	}
	if len(contents(t, filepath.Join(dir, "item"))) != 4*ChunkBytes {
		t.Fatal("source lost after consumer disconnect")
	}
	info, err := os.Stat(filepath.Join(dest, "item"))
	if err != nil || info.Size() >= 4*ChunkBytes {
		t.Fatal("partial outcome unavailable", err)
	}
}

func TestTrashRestoreConflictAndPermanentErase(t *testing.T) {
	dir, data := t.TempDir(), t.TempDir()
	t.Setenv("XDG_DATA_HOME", data)
	write(t, filepath.Join(dir, "item"), []byte("original"))
	out := outcome(t, run(t, request(t, dir, "trash", "item"), nil))
	if out.State != "completed" {
		t.Fatal(out)
	}
	files := filepath.Join(data, "Trash", "files")
	entries, err := os.ReadDir(files)
	if err != nil || len(entries) != 1 {
		t.Fatal(entries, err)
	}
	token := entries[0].Name()
	write(t, filepath.Join(dir, "item"), []byte("new occupant"))
	out = outcome(t, run(t, request(t, files, "restore", token), nil))
	if out.State != "skipped" || !out.SourceRemaining || contents(t, filepath.Join(dir, "item")) != "new occupant" {
		t.Fatal(out)
	}
	req := request(t, files, "restore", token)
	out = outcome(t, run(t, req, func(e Event, c chan<- Control) {
		if e.Kind == "decision" {
			c <- Control{Schema: Schema, ID: req.ID, Kind: "decide", Decision: e.Decision, Choice: "rename", Name: []byte("restored")}
		}
	}))
	if out.State != "completed" || contents(t, filepath.Join(dir, "restored")) != "original" || contents(t, filepath.Join(dir, "item")) != "new occupant" {
		t.Fatal(out)
	}
	out = outcome(t, run(t, request(t, dir, "trash", "restored"), nil))
	if out.State != "completed" {
		t.Fatal(out)
	}
	entries, err = os.ReadDir(files)
	if err != nil || len(entries) != 1 {
		t.Fatal(entries, err)
	}
	token = entries[0].Name()
	req = request(t, files, "delete", token)
	req.Permanent = true
	out = outcome(t, run(t, req, nil))
	if out.State != "completed" {
		t.Fatal(out)
	}
	absent(t, filepath.Join(files, token))
	absent(t, filepath.Join(data, "Trash", "info", token+".trashinfo"))
}

func TestEntryLimitRefusesWholeTree(t *testing.T) {
	dir, dest := t.TempDir(), t.TempDir()
	tree := filepath.Join(dir, "tree")
	if err := os.Mkdir(tree, 0700); err != nil {
		t.Fatal(err)
	}
	for i := 0; i < MaxEntries; i++ {
		write(t, filepath.Join(tree, fmt.Sprintf("entry-%04d", i)), nil)
	}
	req := request(t, dir, "copy", "tree")
	req.Destination = []byte(dest)
	events := run(t, req, nil)
	last := events[len(events)-1]
	if last.State != "failed" || last.TotalKnown {
		t.Fatal(last)
	}
	absent(t, filepath.Join(dest, "tree"))
	if _, err := os.Stat(tree); err != nil {
		t.Fatal(err)
	}
}

func TestStaleConflictControlsCannotSelectANewName(t *testing.T) {
	dir, dest := t.TempDir(), t.TempDir()
	write(t, filepath.Join(dir, "item"), []byte("new"))
	write(t, filepath.Join(dest, "item"), []byte("old"))
	req := request(t, dir, "copy", "item")
	req.Destination = []byte(dest)
	out := outcome(t, run(t, req, func(e Event, c chan<- Control) {
		if e.Kind == "decision" {
			c <- Control{Schema: Schema, ID: "old-job", Kind: "decide", Decision: e.Decision, Choice: "rename", Name: []byte("wrong")}
			c <- Control{Schema: Schema, ID: req.ID, Kind: "decide", Decision: e.Decision + 1, Choice: "rename", Name: []byte("wrong")}
			c <- Control{Schema: Schema, ID: req.ID, Kind: "decide", Decision: e.Decision, Choice: "skip"}
		}
	}))
	if out.State != "skipped" {
		t.Fatal(out)
	}
	absent(t, filepath.Join(dest, "wrong"))
	if contents(t, filepath.Join(dest, "item")) != "old" {
		t.Fatal(out)
	}
}

func TestDestinationFileReplacementRefusesMetadataAndCompletion(t *testing.T) {
	for _, replace := range []bool{true, false} {
		t.Run(fmt.Sprint(replace), func(t *testing.T) {
			dir, dest := t.TempDir(), t.TempDir()
			original := bytes.Repeat([]byte("source"), ChunkBytes)
			write(t, filepath.Join(dir, "item"), original)
			req := request(t, dir, "copy", "item")
			req.Destination = []byte(dest)
			changed := false
			replacementTime := time.Unix(456789000, 0)
			out := outcome(t, run(t, req, func(e Event, _ chan<- Control) {
				if e.Kind == "progress" && e.Bytes > 0 && !changed {
					changed = true
					if err := os.Rename(filepath.Join(dest, "item"), filepath.Join(dest, "owned-partial")); err != nil {
						t.Fatal(err)
					}
					if replace {
						write(t, filepath.Join(dest, "item"), []byte("replacement"))
						if err := os.Chtimes(filepath.Join(dest, "item"), replacementTime, replacementTime); err != nil {
							t.Fatal(err)
						}
					}
				}
			}))
			if !changed || out.State != "partial" || !out.SourceRemaining || out.ContentComplete {
				t.Fatal(out)
			}
			if !bytes.Equal([]byte(contents(t, filepath.Join(dir, "item"))), original) {
				t.Fatal("original source changed")
			}
			if replace {
				if contents(t, filepath.Join(dest, "item")) != "replacement" {
					t.Fatal("replacement content changed")
				}
				info, err := os.Stat(filepath.Join(dest, "item"))
				if err != nil || !info.ModTime().Equal(replacementTime) {
					t.Fatal("replacement metadata changed", err)
				}
			} else {
				absent(t, filepath.Join(dest, "item"))
			}
		})
	}
}

func TestDestinationDirectoryReplacementCannotRedirectWrites(t *testing.T) {
	for _, op := range []string{"copy", "move", "rename"} {
		t.Run(op, func(t *testing.T) {
			dir, dest := t.TempDir(), t.TempDir()
			if err := os.Mkdir(filepath.Join(dir, "tree"), 0700); err != nil {
				t.Fatal(err)
			}
			write(t, filepath.Join(dir, "tree", "item"), []byte("source"))
			req := request(t, dir, op, "tree")
			name := "tree"
			if op == "rename" {
				dest = dir
				name = "renamed"
				req.Name = []byte(name)
			} else {
				req.Destination = []byte(dest)
			}
			if err := os.Mkdir(filepath.Join(dest, "redirect"), 0750); err != nil {
				t.Fatal(err)
			}
			write(t, filepath.Join(dest, "redirect", "sentinel"), []byte("untouched"))
			changed := false
			out := outcome(t, run(t, req, func(e Event, _ chan<- Control) {
				if e.Kind == "progress" && e.Done > 0 && !changed {
					changed = true
					if err := os.Rename(filepath.Join(dest, name), filepath.Join(dest, "owned-partial")); err != nil {
						t.Fatal(err)
					}
					if err := os.Symlink("redirect", filepath.Join(dest, name)); err != nil {
						t.Fatal(err)
					}
				}
			}))
			if !changed || out.State != "partial" || !out.SourceRemaining || out.ContentComplete {
				t.Fatal(out)
			}
			if contents(t, filepath.Join(dir, "tree", "item")) != "source" {
				t.Fatal("source removed after redirect")
			}
			absent(t, filepath.Join(dest, "redirect", "item"))
			if contents(t, filepath.Join(dest, "redirect", "sentinel")) != "untouched" {
				t.Fatal("redirect target changed")
			}
		})
	}
}

func TestTrashLocationPolicy(t *testing.T) {
	home := t.TempDir()
	absolute := t.TempDir()
	for _, tc := range []struct{ data, want string }{{"", filepath.Join(home, ".local", "share", "Trash", "files")}, {"relative/path", filepath.Join(home, ".local", "share", "Trash", "files")}, {absolute, filepath.Join(absolute, "Trash", "files")}} {
		actual, err := TrashFilesPath(home, tc.data)
		if err != nil || actual != tc.want {
			t.Fatal(tc, actual, err)
		}
	}
	if _, err := TrashFilesPath("relative-home", ""); err == nil {
		t.Fatal("admitted relative home")
	}
	if _, err := TrashFilesPath(home, "/tmp/\x00bad"); err == nil {
		t.Fatal("admitted NUL")
	}
	t.Setenv("HOME", home)
	t.Setenv("XDG_DATA_HOME", "relative/path")
	actual, err := dataHome()
	if err != nil || actual != filepath.Join(home, ".local", "share") {
		t.Fatal(actual, err)
	}
}

func TestCancellationReportsEveryAdmittedTarget(t *testing.T) {
	dir, dest := t.TempDir(), t.TempDir()
	for _, name := range []string{"first", "second", "third"} {
		write(t, filepath.Join(dir, name), bytes.Repeat([]byte(name), ChunkBytes))
	}
	req := request(t, dir, "copy", "first", "second", "third")
	req.Destination = []byte(dest)
	cancelled := false
	events := run(t, req, func(e Event, c chan<- Control) {
		if e.Kind == "progress" && e.Bytes > 0 && !cancelled {
			cancelled = true
			c <- Control{Schema: Schema, ID: req.ID, Kind: "cancel"}
		}
	})
	items := []Event{}
	for _, e := range events {
		if e.Kind == "item" {
			items = append(items, e)
		}
	}
	if len(items) != 3 {
		t.Fatal("missing known target outcomes", items)
	}
	if items[0].State != "partial" || !items[0].Outcome.SourceRemaining || !items[0].Outcome.DestinationPresent {
		t.Fatal(items[0])
	}
	for i, e := range items[1:] {
		if e.Item != i+1 || e.State != "cancelled" || !e.Outcome.SourceRemaining || e.Outcome.DestinationPresent || !strings.Contains(e.Detail, "Not attempted") {
			t.Fatal(e)
		}
		absent(t, filepath.Join(dest, []string{"second", "third"}[i]))
	}
	if events[len(events)-1].State != "partial" || !strings.Contains(events[len(events)-1].Detail, "3 other outcomes") {
		t.Fatal(events[len(events)-1])
	}
}

func TestCancellationBeforePlanningReportsTargets(t *testing.T) {
	dir, dest := t.TempDir(), t.TempDir()
	write(t, filepath.Join(dir, "item"), []byte("source"))
	req := request(t, dir, "copy", "item")
	req.Destination = []byte(dest)
	ctx, cancel := context.WithCancel(context.Background())
	events := []Event{}
	err := Run(ctx, req, nil, func(e Event) error {
		events = append(events, e)
		if e.Kind == "accepted" {
			cancel()
		}
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	out := outcome(t, events)
	if out.State != "cancelled" || !out.SourceRemaining || out.DestinationPresent {
		t.Fatal(out)
	}
	absent(t, filepath.Join(dest, "item"))
	if events[len(events)-1].State != "cancelled" {
		t.Fatal(events)
	}
}

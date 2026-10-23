package main

import (
	"bytes"
	"context"
	"encoding/base64"
	"encoding/json"
	"io"
	"net"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/transport"
)

func TestMain(m *testing.M) {
	if len(os.Args) == 2 && os.Args[1] == "--worker" {
		switch os.Getenv("FVWM_TEST_FILE_WORKER") {
		case "block":
			time.Sleep(time.Minute)
			os.Exit(0)
		case "oversize":
			os.Stdout.Write(bytes.Repeat([]byte("x"), filemodel.MaxWire+1))
			os.Exit(0)
		}
		if runWorker(os.Stdin, os.Stdout) != nil {
			os.Exit(1)
		}
		os.Exit(0)
	}
	os.Exit(m.Run())
}

func TestWorkerIsolationBoundsAndCancellation(t *testing.T) {
	executable, _ := os.Executable()
	root := t.TempDir()
	result := startWorker(context.Background(), executable, workRequest{Operation: "scan", Path: []byte(root)})
	if result.Status != "loaded" || string(result.Snapshot.Path) != root {
		t.Fatal(result)
	}
	t.Setenv("FVWM_TEST_FILE_WORKER", "block")
	ctx, cancel := context.WithTimeout(context.Background(), 50*time.Millisecond)
	defer cancel()
	start := time.Now()
	result = startWorker(ctx, executable, workRequest{Operation: "scan", Path: []byte(root)})
	if result.Status != "cancelled" || time.Since(start) > 3*time.Second {
		t.Fatal(result, time.Since(start))
	}
	t.Setenv("FVWM_TEST_FILE_WORKER", "oversize")
	result = startWorker(context.Background(), executable, workRequest{Operation: "scan", Path: []byte(root)})
	if result.Status != "failed" {
		t.Fatal("oversize accepted", result.Status)
	}
	for _, data := range []string{`{"operation":"scan","unknown":1}`, `{} {}`, strings.Repeat("x", filemodel.MaxWire+1)} {
		if runWorker(strings.NewReader(data), new(bytes.Buffer)) == nil {
			t.Fatal("invalid private request accepted")
		}
	}
}

func TestActionsValidateAndPassLiteralArguments(t *testing.T) {
	root := t.TempDir()
	name := "literal ' \" $[value];` \n"
	os.WriteFile(filepath.Join(root, name), nil, 0600)
	s, _ := filemodel.Scan(root, false)
	program := filepath.Join(root, "opener")
	report := filepath.Join(root, "argv")
	// The fixture shell sees only a positional argument. Production invokes
	// the selected executable directly and never constructs shell text.
	os.WriteFile(program, []byte("#!/bin/sh\nprintf '%s' \"$1\" > \"$FVWM_TEST_ARGV\"\n"), 0700)
	t.Setenv("FVWM_TEST_ARGV", report)
	result := perform(workRequest{Operation: "activate", Snapshot: s, ID: s.Items[0].ID, Program: program})
	if result.Status != "started" {
		t.Fatal(result)
	}
	deadline := time.Now().Add(3 * time.Second)
	for {
		data, _ := os.ReadFile(report)
		if string(data) == filepath.Join(root, name) {
			break
		}
		if time.Now().After(deadline) {
			t.Fatalf("literal argument lost: %q", data)
		}
		time.Sleep(time.Millisecond)
	}
	os.Rename(filepath.Join(root, name), filepath.Join(root, "moved"))
	if result := perform(workRequest{Operation: "activate", Snapshot: s, ID: s.Items[0].ID, Program: program}); result.Status != "failed" {
		t.Fatal("stale target dispatched")
	}
}

func TestControlsUseRenderedGenerationAndPageSelection(t *testing.T) {
	b := browser{instance: "owner", form: 3}
	b.model.Replace([]collection.Entry{{ID: "first", Label: "same"}, {ID: "second", Label: "same"}})
	picks := "x" + base64.RawURLEncoding.EncodeToString([]byte("first second"))
	if _, err := b.action("FVWM_FILES_1 owner 2 cancel - " + picks); err == nil || len(b.model.Selected) != 0 {
		t.Fatal("stale callback mutated state")
	}
	if _, err := b.action("FVWM_FILES_1 owner 3 cancel - " + picks); err != nil || len(b.model.Selected) != 2 {
		t.Fatal(err)
	}
	if close, err := b.action("FVWM_FILES_1 owner 3 close - x"); !close || err != nil {
		t.Fatal(close, err)
	}
	for _, value := range []string{"$[exec.x]", `quote" \ percent% &`, "line\nnext"} {
		if strings.ContainsAny(displayText(value, 64), "$%\\\"\r\n&") {
			t.Fatal("unsafe Form display", value)
		}
	}
}

func TestWorkerWirePreservesFilenameBytes(t *testing.T) {
	request := workRequest{Operation: "scan", Path: []byte{'/', 0xff}}
	data, _ := json.Marshal(request)
	var copy workRequest
	if decode(data, &copy) != nil || !bytes.Equal(copy.Path, request.Path) {
		t.Fatal("wire normalized filename")
	}
}

func TestOneWorkerLatestNavigationAndCancelledPublication(t *testing.T) {
	left, right := net.Pipe()
	defer left.Close()
	defer right.Close()
	go io.Copy(io.Discard, right)
	executable, _ := os.Executable()
	root := t.TempDir()
	b := browser{session: transport.New(left), executable: executable, instance: "fixture", results: make(chan completedWork, 1), form: 1}
	t.Setenv("FVWM_TEST_FILE_WORKER", "block")
	if err := b.navigate(root, "push"); err != nil {
		t.Fatal(err)
	}
	b.navigate(root+"/second", "push")
	b.navigate(root+"/latest", "push")
	if !b.active || b.pending == nil || string(b.pending.request.Path) != root+"/latest" || b.serial != 3 {
		t.Fatal("unbounded or missing supersession")
	}
	if err := b.queue(workRequest{Operation: "activate"}, "push"); err == nil || b.serial != 3 {
		t.Fatal("side effect queued behind busy worker")
	}
	if _, err := b.action("FVWM_FILES_1 fixture 1 cancel - x"); err != nil {
		t.Fatal(err)
	}
	if b.pending != nil || b.serial != 4 {
		t.Fatal("cancel retained a replacement")
	}
	select {
	case event := <-b.results:
		if err := b.completed(event); err != nil {
			t.Fatal(err)
		}
	case <-time.After(3 * time.Second):
		t.Fatal("cancelled worker not reaped")
	}
	if b.active || b.snapshot != nil || !strings.HasSuffix(b.alias, "-ready") {
		t.Fatal("cancelled output published")
	}
	t.Setenv("FVWM_TEST_FILE_WORKER", "")
	if err := b.navigate(root, "push"); err != nil {
		t.Fatal(err)
	}
	select {
	case event := <-b.results:
		if err := b.completed(event); err != nil {
			t.Fatal(err)
		}
	case <-time.After(3 * time.Second):
		t.Fatal("replacement failed")
	}
	if b.snapshot == nil || string(b.snapshot.Path) != root || b.model.Generation != 1 {
		t.Fatal("replacement not published")
	}
}

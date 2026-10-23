package main

import (
	"context"
	"io"
	"net"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/transport"
)

func TestTargetOptionsRejectConflictsBeforeFilesystemWork(t *testing.T) {
	for _, check := range []struct {
		dirSet             bool
		reveal, properties targetFlags
	}{
		{true, targetFlags{"/a"}, nil}, {true, nil, targetFlags{"/a"}}, {false, targetFlags{"/a"}, targetFlags{"/b"}}, {false, targetFlags{"file://remote/a"}, nil}, {false, targetFlags{""}, nil},
	} {
		o := options{dir: "/home"}
		if prepareOptions(&o, check.dirSet, check.reveal, check.properties) == nil || len(o.targets) != 0 {
			t.Fatal("invalid option request admitted")
		}
	}
	o := options{dir: "file:///home/a%20b"}
	if prepareOptions(&o, false, nil, nil) != nil || o.initialRequest().Operation != "scan" || o.dir != "/home/a b" {
		t.Fatal(o)
	}
	o = options{dir: "/home"}
	if prepareOptions(&o, false, nil, targetFlags{"file://localhost/tmp/%FF"}) != nil || !o.properties || string(o.targets[0]) != "/tmp/\xff" || o.initialRequest().Operation != "locate" {
		t.Fatal(o)
	}
	multi := options{}
	if prepareOptions(&multi, false, nil, targetFlags{"/tmp/a", "/tmp/b"}) != nil || !multi.properties || len(multi.targets) != 2 {
		t.Fatal("repeated properties not admitted", multi)
	}
	var flags targetFlags
	for i := 0; i < 256; i++ {
		if flags.Set("/a") != nil {
			t.Fatal(i)
		}
	}
	if flags.Set("/b") == nil || len(flags) != 256 {
		t.Fatal("flag count unbounded")
	}
	flags = nil
	if flags.Set(strings.Repeat("x", 3*4096+33)) == nil {
		t.Fatal("encoded target unbounded")
	}
}

func TestIsolatedLocateCarriesSelectionAndFailure(t *testing.T) {
	root := t.TempDir()
	target := filepath.Join(root, "a")
	os.WriteFile(target, nil, 0600)
	executable, _ := os.Executable()
	result := startWorker(context.Background(), executable, workRequest{Operation: "locate", Targets: [][]byte{[]byte(target)}})
	if result.Status != "loaded" || len(result.Selected) != 1 || result.Selected[0] != result.Snapshot.Items[0].ID {
		t.Fatal(result)
	}
	result = startWorker(context.Background(), executable, workRequest{Operation: "locate", Targets: [][]byte{[]byte(target), []byte(filepath.Join(root, "missing"))}})
	if result.Status != "failed" || result.Snapshot != nil || len(result.Selected) != 0 {
		t.Fatal("partial initial result", result)
	}
}

func TestInitialPropertiesOwnsSelectionAndViewport(t *testing.T) {
	left, right := net.Pipe()
	defer left.Close()
	defer right.Close()
	go io.Copy(io.Discard, right)
	root := t.TempDir()
	for i := 0; i < 20; i++ {
		os.WriteFile(filepath.Join(root, string(rune('a'+i))), nil, 0600)
	}
	r := perform(workRequest{Operation: "locate", Targets: [][]byte{[]byte(filepath.Join(root, "t"))}})
	if r.Status != "loaded" {
		t.Fatal(r)
	}
	b := browser{session: transport.New(left), instance: "initial"}
	if err := b.completed(completedWork{work: pendingWork{navigation: "properties"}, result: r}); err != nil {
		t.Fatal(err)
	}
	if len(b.model.SelectedIDs()) != 1 || (len(b.inspected) != 1 || b.inspected[0] != r.Selected[0]) || b.model.Focus != r.Selected[0] {
		t.Fatal("initial selection/property lost")
	}
}

func TestInitialMultiplePropertiesOwnsACompleteSelection(t *testing.T) {
	left, right := net.Pipe()
	defer left.Close()
	defer right.Close()
	go io.Copy(io.Discard, right)
	root := t.TempDir()
	for _, name := range []string{"a", "b"} {
		os.WriteFile(filepath.Join(root, name), nil, 0600)
	}
	r := perform(workRequest{Operation: "locate", Targets: [][]byte{[]byte(filepath.Join(root, "b")), []byte(filepath.Join(root, "a"))}})
	b := browser{session: transport.New(left), instance: "multi-properties"}
	if err := b.completed(completedWork{work: pendingWork{navigation: "properties"}, result: r}); err != nil || len(b.inspected) != 2 || len(b.model.Selected) != 2 || b.model.Focus != r.Selected[0] {
		t.Fatal(err, b.inspected, b.model.Selected)
	}
}

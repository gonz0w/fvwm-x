package main

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

func watchFixture(t *testing.T) watchRequest {
	t.Helper()
	snapshot, err := filemodel.Scan(t.TempDir(), false)
	if err != nil {
		t.Fatal(err)
	}
	return watchRequest{snapshot.Path, snapshot.Root}
}

func TestWatchWorkerChangesLossAndFiniteLease(t *testing.T) {
	for _, lost := range []bool{false, true} {
		t.Run(map[bool]string{false: "lease", true: "rename"}[lost], func(t *testing.T) {
			request := watchFixture(t)
			data, _ := json.Marshal(request)
			reader, writer := io.Pipe()
			defer reader.Close()
			states := make(chan string, 4)
			done := make(chan error, 1)
			go func() {
				done <- runWatchWorker(bytes.NewReader(data), writer, time.Second)
				writer.Close()
			}()
			go func() {
				scanner := bufio.NewScanner(reader)
				for scanner.Scan() {
					states <- scanner.Text()
				}
			}()
			expect := func(want string) {
				t.Helper()
				select {
				case got := <-states:
					if got != want {
						t.Fatalf("got %q, expected %q", got, want)
					}
				case <-time.After(4 * time.Second):
					t.Fatal("native watch timed out")
				}
			}
			expect("ready")
			if lost {
				if err := os.Rename(string(request.Path), string(request.Path)+"-moved"); err != nil {
					t.Fatal(err)
				}
				defer os.RemoveAll(string(request.Path) + "-moved")
				expect("lost")
			} else {
				name := filepath.Join(string(request.Path), "literal ' \n")
				if err := os.WriteFile(name, nil, 0600); err != nil {
					t.Fatal(err)
				}
				expect("changed")
				expect("expired")
			}
			if err := <-done; err != nil {
				t.Fatal(err)
			}
		})
	}
}

func TestWatchWorkerRefusesStaleAndMalformedRequests(t *testing.T) {
	request := watchFixture(t)
	request.Root.Inode++
	data, _ := json.Marshal(request)
	var output bytes.Buffer
	if err := runWatchWorker(bytes.NewReader(data), &output, time.Second); err != nil || output.String() != "lost\n" {
		t.Fatal(output.String(), err)
	}
	for _, input := range []string{`{}`, `{"unknown":true}`, `{}` + `{}`, strings.Repeat("x", watchWire+1)} {
		output.Reset()
		if runWatchWorker(strings.NewReader(input), &output, time.Second) == nil || output.Len() != 0 {
			t.Fatal("invalid request accepted")
		}
	}
	request = watchFixture(t)
	request.Path = append(request.Path, []byte("/missing")...)
	data, _ = json.Marshal(request)
	output.Reset()
	if err := runWatchWorker(bytes.NewReader(data), &output, time.Second); err != nil || output.String() != "unavailable\n" {
		t.Fatal(output.String(), err)
	}
}

func TestWatchProcessCancellationReapsBeforeDone(t *testing.T) {
	executable, _ := os.Executable()
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	events := make(chan watchEvent, 1)
	go watchProcess(ctx, executable, watchFixture(t), 7, events)
	select {
	case event := <-events:
		if event.serial != 7 || event.state != "ready" {
			t.Fatal(event)
		}
	case <-time.After(4 * time.Second):
		t.Fatal("watch process did not initialize")
	}
	cancel()
	select {
	case event := <-events:
		if event.serial != 7 || event.state != "done" {
			t.Fatal(event)
		}
	case <-time.After(4 * time.Second):
		t.Fatal("cancelled watch process was not reaped")
	}
}

func TestWatchProcessRejectsOversizeAndOutOfOrderOutput(t *testing.T) {
	executable, _ := os.Executable()
	for _, mode := range []string{"oversize", "out-of-order"} {
		t.Run(mode, func(t *testing.T) {
			t.Setenv("FVWM_TEST_WATCH_WORKER", mode)
			ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
			defer cancel()
			events := make(chan watchEvent, 1)
			go watchProcess(ctx, executable, watchFixture(t), 9, events)
			for _, want := range []string{"unavailable", "done"} {
				select {
				case event := <-events:
					if event.serial != 9 || event.state != want {
						t.Fatal(event, want)
					}
				case <-ctx.Done():
					t.Fatal("bad observer was not killed and reaped")
				}
			}
		})
	}
}

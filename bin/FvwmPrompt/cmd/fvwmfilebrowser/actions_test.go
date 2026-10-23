package main

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"runtime"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

func TestConfiguredActionOptionsAreBoundedLiteralPrograms(t *testing.T) {
	for _, value := range []string{"", "name", "=program", "name=", "  =program", "name=bad\x00", strings.Repeat("x", 33) + "=program", "name=" + strings.Repeat("x", filemodel.MaxPath+1)} {
		var actions actionFlags
		if actions.Set(value) == nil || len(actions) != 0 {
			t.Fatal("invalid action accepted", value)
		}
	}
	var actions actionFlags
	for i := 0; i < maxActions; i++ {
		if err := actions.Set(fmt.Sprintf("Action%d=/literal program=%d", i, i)); err != nil {
			t.Fatal(err)
		}
	}
	if actions.Set("extra=program") == nil || len(actions) != maxActions {
		t.Fatal("action count not bounded")
	}
	var duplicate actionFlags
	duplicate.Set("same=one")
	if duplicate.Set("same=two") == nil || duplicate[0].Program != "one" {
		t.Fatal("duplicate action replaced")
	}
}

func TestConfiguredWorkerActionChecksAllBeforeOneLiteralLaunch(t *testing.T) {
	executable, _ := os.Executable()
	root := t.TempDir()
	names := []string{"-dash '$[value] ;`", "line\nnext", string([]byte{'r', 'a', 'w', '-', 0xff})}
	if runtime.GOOS == "darwin" {
		names[2] = "raw-host"
	}
	for _, name := range names {
		if err := os.WriteFile(filepath.Join(root, name), []byte("old"), 0600); err != nil {
			t.Fatal(err)
		}
	}
	s, _ := filemodel.Scan(root, false)
	ids := []string{}
	want := []string{}
	for i := len(s.Items) - 1; i >= 0; i-- {
		ids = append(ids, s.Items[i].ID)
		want = append(want, filepath.Join(root, string(s.Items[i].Name)))
	}
	report := filepath.Join(t.TempDir(), "invoked")
	t.Setenv("FVWM_TEST_ACTION_REPORT", report)
	result := startWorker(context.Background(), executable, workRequest{Operation: "custom", Snapshot: s, IDs: ids, Program: executable})
	if result.Status != "started" {
		t.Fatal(result)
	}
	deadline := time.Now().Add(3 * time.Second)
	for {
		if data, err := os.ReadFile(report); err == nil {
			var got struct {
				Args [][]byte
				Cwd  []byte
			}
			if json.Unmarshal(data, &got) != nil {
				t.Fatal("invalid report")
			}
			paths := []string{}
			for _, arg := range got.Args {
				paths = append(paths, string(arg))
			}
			if !reflect.DeepEqual(paths, want) || string(got.Cwd) != root {
				t.Fatal(paths, want, string(got.Cwd))
			}
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("application did not run")
		}
		time.Sleep(time.Millisecond)
	}
	os.Remove(report)
	os.WriteFile(want[len(want)-1], []byte("changed size observation"), 0600)
	result = startWorker(context.Background(), executable, workRequest{Operation: "custom", Snapshot: s, IDs: ids, Program: executable})
	if result.Status != "failed" {
		t.Fatal("stale selection launched", result)
	}
	if _, err := os.Stat(report); !os.IsNotExist(err) {
		t.Fatal("application launched before whole selection validation")
	}
	for _, ids := range [][]string{nil, {"missing"}, {s.Items[0].ID, s.Items[0].ID}} {
		if result := perform(workRequest{Operation: "custom", Snapshot: s, IDs: ids, Program: executable}); result.Status != "failed" {
			t.Fatal(result)
		}
	}
	if result := perform(workRequest{Operation: "custom", Program: executable}); result.Status != "failed" {
		t.Fatal("missing snapshot accepted")
	}
}

func TestConfiguredActionUsesCurrentSelectionAndRefusesBusyWork(t *testing.T) {
	executable, _ := os.Executable()
	root := t.TempDir()
	for _, name := range []string{"a", "b"} {
		os.WriteFile(filepath.Join(root, name), nil, 0600)
	}
	s, _ := filemodel.Scan(root, false)
	b := browser{instance: "actions", form: 3, snapshot: s, executable: executable,
		actions: actionFlags{{Label: "Review", Program: "unused-fixture-program"}}, results: make(chan completedWork, 1)}
	b.model.Replace(s.Entries())
	t.Setenv("FVWM_TEST_FILE_WORKER", "block")
	message := "FVWM_FILES_2 actions 3 custom 0 xAAAD"
	if _, err := b.action(message); err != nil || !b.active || b.serial != 1 {
		t.Fatal(err, b.serial)
	}
	defer b.cancel()
	if _, err := b.action(message); err == nil || b.serial != 1 {
		t.Fatal("action queued behind busy worker", err)
	}
	if _, err := b.action("FVWM_FILES_2 actions 3 custom 1 xAAAD"); err == nil || b.serial != 1 {
		t.Fatal("unknown action admitted")
	}
	b.cancel()
	select {
	case event := <-b.results:
		if event.result.Status != "cancelled" || len(event.work.request.IDs) != 2 || event.work.request.Program != "unused-fixture-program" {
			t.Fatal(event)
		}
	case <-time.After(3 * time.Second):
		t.Fatal("cancelled action worker not reaped")
	}
}

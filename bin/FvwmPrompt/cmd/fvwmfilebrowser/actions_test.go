package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"os"
	"path/filepath"
	"reflect"
	"runtime"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/transport"
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
	message := testControl(&b, "custom", "0", []byte{0, 0, 3})
	if _, err := b.action(message); err != nil || !b.active || b.serial != 1 {
		t.Fatal(err, b.serial)
	}
	defer b.cancel()
	if _, err := b.action(message); err == nil || b.serial != 1 {
		t.Fatal("action queued behind busy worker", err)
	}
	if _, err := b.action(testControl(&b, "custom", "1", []byte{0, 0, 3})); err == nil || b.serial != 1 {
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

func TestUnavailableProgramsFailCorrelatedWorkAndAllowRefreshRetry(t *testing.T) {
	for _, operation := range []string{"open", "terminal", "custom"} {
		for _, unavailable := range []string{"missing", "nonexecutable"} {
			t.Run(operation+"/"+unavailable, func(t *testing.T) {
				executable, err := os.Executable()
				if err != nil {
					t.Fatal(err)
				}
				root, programs := t.TempDir(), t.TempDir()
				path := filepath.Join(root, "unchanged file")
				if err := os.WriteFile(path, []byte("unchanged bytes"), 0600); err != nil {
					t.Fatal(err)
				}
				snapshot, err := filemodel.Scan(root, false)
				if err != nil || len(snapshot.Items) != 1 {
					t.Fatal("valid target setup failed", err)
				}
				report := filepath.Join(programs, "launch-report")
				t.Setenv("FVWM_TEST_LAUNCH_REPORT", report)
				program := filepath.Join(programs, unavailable)
				script := []byte("#!/bin/sh\nprintf 'started\\n' >> \"$FVWM_TEST_LAUNCH_REPORT\"\n")
				if unavailable == "nonexecutable" {
					if err := os.WriteFile(program, script, 0600); err != nil {
						t.Fatal(err)
					}
				}
				left, right := net.Pipe()
				t.Cleanup(func() { left.Close(); right.Close() })
				go io.Copy(io.Discard, right)
				b := browser{instance: "unavailable", session: transport.New(left), executable: executable,
					snapshot: snapshot, results: make(chan completedWork, 1), opener: program, terminal: program,
					actions: actionFlags{{Label: "Test action", Program: program}}}
				if err := b.model.Replace(snapshot.Entries()); err != nil {
					t.Fatal(err)
				}
				id := snapshot.Items[0].ID
				if err := b.model.SelectIDs(b.model.Generation, []string{id}); err != nil {
					t.Fatal(err)
				}
				values := `{}`
				if operation == "open" {
					values = `{"id":"` + id + `"}`
				} else if operation == "custom" {
					values = `{"index":0}`
				}
				admit := func(op, fields, serial string) fileQuery {
					t.Helper()
					query := actionQuery(&b, op, fields)
					query.Serial = serial
					action, err := decodeProviderAction(query)
					if err != nil {
						t.Fatal(err)
					}
					if code := b.applyProviderAction(query, action); code != "" || !b.active ||
						b.providerWork.ID != serial || b.providerWork.Operation != op || b.providerWork.Status != "accepted" {
						t.Fatal("work admission/correlation failed", code, b.providerWork)
					}
					return query
				}
				complete := func(want string, query fileQuery) completedWork {
					t.Helper()
					select {
					case event := <-b.results:
						if !b.active || event.work.serial != b.serial || event.result.Schema != 1 || event.result.Status != want {
							t.Fatal("worker result/owned slot mismatch", event.result, b.active)
						}
						if err := b.completed(event); err != nil {
							t.Fatal(err)
						}
						if b.active || b.cancel != nil || b.activeSerial != 0 || b.pending != nil ||
							b.providerWork.ID != query.Serial || b.providerWork.Operation != query.Operation || b.providerWork.Status != want {
							t.Fatal("reaped result did not retire its correlated slot", b.providerWork)
						}
						return event
					case <-time.After(5 * time.Second):
						t.Fatal("worker was not reaped")
						return completedWork{}
					}
				}
				query := admit(operation, values, "0000000000000001")
				event := complete("failed", query)
				if event.result.Error != "Could not start the configured application" || event.result.ActionCount != 0 || b.providerWork.Detail == "" || b.dataReady() {
					t.Fatal("missing/nonexecutable application claimed a handoff", event.result, b.providerWork)
				}
				if _, err := os.Stat(report); !os.IsNotExist(err) {
					t.Fatal("failed application produced side effects", err)
				}
				if current, kind, err := snapshot.Validate(id); err != nil || current != path || kind != "file" {
					t.Fatal("launch test used a stale/invalid target", err)
				}
				if current, err := snapshot.RootPath(); err != nil || current != root {
					t.Fatal("terminal test used a stale/invalid root", err)
				}
				refresh := admit("refresh", `{}`, "0000000000000002")
				complete("loaded", refresh)
				if !b.dataReady() || !b.model.Selected[id] {
					t.Fatal("explicit refresh did not restore usable retained selection")
				}
				if _, err := os.Stat(report); !os.IsNotExist(err) {
					t.Fatal("refresh launched the refused application", err)
				}
				working := filepath.Join(programs, "working")
				if err := os.WriteFile(working, script, 0700); err != nil {
					t.Fatal(err)
				}
				b.opener, b.terminal, b.actions[0].Program = working, working, working
				retry := admit(operation, values, "0000000000000003")
				complete("started", retry)
				deadline := time.Now().Add(3 * time.Second)
				for {
					if data, err := os.ReadFile(report); err == nil {
						if string(data) != "started\n" {
							t.Fatal("retry did not launch exactly once", string(data))
						}
						break
					}
					if time.Now().After(deadline) {
						t.Fatal("valid retry did not start the application")
					}
					time.Sleep(time.Millisecond)
				}
				if data, err := os.ReadFile(path); err != nil || string(data) != "unchanged bytes" {
					t.Fatal("application failure/retry mutated the target", err)
				}
			})
		}
	}
}

package main

import (
	"bytes"
	"context"
	"encoding/json"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestFileManagerFramesAndReplyBounds(t *testing.T) {
	for _, input := range []string{
		`{"version":1,"kind":"request","id":"1","method":"ShowFolders","uris":["file:///a"],"startup_id":""}` + "\n",
		`{"version":1,"kind":"ready","owner":":1.4"}` + "\n",
		`{"version":1,"kind":"unavailable","detail":"disabled"}` + "\n",
	} {
		events, stop := make(chan fileManagerEvent, 2), make(chan struct{})
		readFileManagerFrames(strings.NewReader(input), events, stop, nil)
		if event := <-events; event.err != nil || event.frame.Version != 1 {
			t.Fatal(event)
		}
		if event := <-events; event.err != io.EOF {
			t.Fatal(event)
		}
	}
	for _, input := range []string{
		`{"version":2,"kind":"ready"}` + "\n",
		`{"version":1,"kind":"ready","extra":true}` + "\n",
		`{"version":1,"kind":"ready"} {}` + "\n",
		"{\"version\":1,\"kind\":\"\xff\"}\n",
		strings.Repeat("x", fileManagerMaxFrame+1) + "\n",
	} {
		events := make(chan fileManagerEvent, 2)
		readFileManagerFrames(strings.NewReader(input), events, make(chan struct{}), nil)
		if event := <-events; event.err == nil {
			t.Fatal("invalid frame admitted", input[:16])
		}
	}
	for _, id := range []string{"", "0", "01", "-1", "+1", "18446744073709551616"} {
		if _, ok := fileManagerRequestID(id); ok {
			t.Fatal("invalid id", id)
		}
	}
	for _, status := range []string{"accepted", "invalid", "refused", "unknown"} {
		data, err := fileManagerReply("18446744073709551615", status)
		var reply map[string]any
		if err != nil || len(data) > fileManagerMaxReply+1 || json.Unmarshal(data, &reply) != nil || len(reply) != 4 || reply["id"] != "18446744073709551615" || reply["status"] != status {
			t.Fatal(string(data), err)
		}
	}
	if _, err := fileManagerReply("1", "retry"); err == nil {
		t.Fatal("invalid status admitted")
	}
}

func TestFileManagerFramesRequireTerminatingNewline(t *testing.T) {
	for _, input := range []string{
		`{"version":1,"kind":"request","id":"1","method":"ShowFolders","uris":["file:///a"],"startup_id":""}`,
		`{"version":1,"kind":"ready","owner":":1.4"}`,
		`{"version":1,"kind":"unavailable","detail":"disabled"}`,
	} {
		events := make(chan fileManagerEvent, 2)
		failures := 0
		readFileManagerFrames(strings.NewReader(input), events, make(chan struct{}), func() {
			failures++
		})
		if failures != 1 || len(events) != 1 {
			t.Fatal("cut frame did not produce exactly one failure", failures, len(events))
		}
		if event := <-events; event.err != io.ErrUnexpectedEOF || event.frame.Kind != "" || event.frame.Version != 0 || len(event.frame.URIs) != 0 {
			t.Fatal("unterminated JSON was decoded or admitted", event)
		}
	}
}

func TestFileManagerFrameLengthIncludesOnlyJSONBytes(t *testing.T) {
	base := `{"version":1,"kind":"ready","owner":":1.4"}`
	input := base + strings.Repeat(" ", fileManagerMaxFrame-len(base)) + "\n"
	events := make(chan fileManagerEvent, 2)
	readFileManagerFrames(strings.NewReader(input), events, make(chan struct{}), nil)
	if event := <-events; event.err != nil || event.frame.Kind != "ready" {
		t.Fatal("maximum terminated frame refused", event)
	}
	if event := <-events; event.err != io.EOF {
		t.Fatal(event)
	}
	events = make(chan fileManagerEvent, 2)
	input = base + strings.Repeat(" ", fileManagerMaxFrame+1-len(base)) + "\n"
	readFileManagerFrames(strings.NewReader(input), events, make(chan struct{}), nil)
	if len(events) != 1 {
		t.Fatal("over-limit frame did not stop reader", len(events))
	}
	if event := <-events; event.err == nil || event.frame.Kind != "" {
		t.Fatal("over-limit frame decoded", event)
	}
}

func TestFileManagerHelperUsesFixedOrAbsolutePath(t *testing.T) {
	old := fileManagerBusHelperPath
	defer func() { fileManagerBusHelperPath = old }()
	fileManagerBusHelperPath = "/private/install/fvwm-session-bus"
	if path, err := fileManagerHelperPath(func(string) string { return "" }); err != nil || path != fileManagerBusHelperPath {
		t.Fatal(path, err)
	}
	for _, value := range []string{"helper", "./helper", "/bad\x00path"} {
		if _, err := fileManagerHelperPath(func(string) string { return value }); err == nil {
			t.Fatal("invalid helper override", value)
		}
	}
	if path, err := fileManagerHelperPath(func(string) string { return "/controlled/helper" }); err != nil || path != "/controlled/helper" {
		t.Fatal(path, err)
	}
}

func TestFileManagerChildSlotsRequireActualCompletion(t *testing.T) {
	children := newFileManagerChildren()
	ids, ok := children.reserve(8)
	if !ok || len(ids) != 8 {
		t.Fatal(ids, ok)
	}
	if _, ok := children.reserve(1); ok {
		t.Fatal("capacity exceeded")
	}
	children.done <- ids[0]
	if _, ok := children.reserve(2); ok || len(children.live) != 7 {
		t.Fatal("partial capacity reservation")
	}
	if _, ok := children.reserve(1); !ok || len(children.live) != 8 {
		t.Fatal("confirmed exit did not release slot")
	}
}

func TestFileManagerLiteralChildArgsAndReceipt(t *testing.T) {
	o := options{properties: true, targets: [][]byte{[]byte("/a/literal ;\n\xff")}}
	args := fileManagerChildArgs(o, "/socket with spaces")
	want := []string{"--_filemanager-child", "--reuse", "--socket", "/socket with spaces", "--properties", string(o.targets[0])}
	if strings.Join(args, "\x00") != strings.Join(want, "\x00") {
		t.Fatal(args)
	}
	for _, data := range []string{"", `{"version":1,"status":"accepted"}`, `{"version":2,"status":"accepted"}` + "\n", `{"version":1,"status":"unknown"}` + "\n", strings.Repeat("x", fileManagerMaxReply+1) + "\n"} {
		if readFileManagerReceipt(strings.NewReader(data)) != "unknown" {
			t.Fatal("invalid receipt admitted", data)
		}
	}
	if readFileManagerReceipt(bytes.NewBufferString("{\"version\":1,\"status\":\"accepted\"}\n")) != "accepted" {
		t.Fatal("valid receipt refused")
	}
	reader, writer, err := os.Pipe()
	if err != nil {
		t.Fatal(err)
	}
	reader.Close()
	// A disappeared observer is a diagnostic, never process termination.
	writeFileManagerReceipt(writer, "accepted")
	writeFileManagerReceipt(writer, "unknown")
}

// Private test-binary children exercise real exec/ExtraFiles/Wait without X or
// the native bridge. Normal tests and production binaries never use this mode.
func init() {
	if len(os.Args) < 2 || os.Args[1] != "--_filemanager-child" || os.Getenv("FVWM_TEST_FILEMANAGER_CHILD") == "" {
		return
	}
	file, err := openFileManagerReceipt()
	if err != nil {
		os.Exit(2)
	}
	defer file.Close()
	mode := os.Getenv("FVWM_TEST_FILEMANAGER_CHILD")
	if mode == "held" {
		gate := os.Getenv("FVWM_TEST_FILEMANAGER_RECEIPT_GATE")
		label := filepath.Base(os.Args[len(os.Args)-1])
		if os.WriteFile(filepath.Join(gate, "started-"+label), nil, 0600) != nil {
			os.Exit(2)
		}
		if label == "first" {
			deadline := time.Now().Add(5 * time.Second)
			for {
				if _, err := os.Stat(filepath.Join(gate, "admit-first")); err == nil {
					break
				}
				if time.Now().After(deadline) {
					os.Exit(3)
				}
				time.Sleep(5 * time.Millisecond)
			}
		}
	}
	if mode == "late" {
		time.Sleep(100 * time.Millisecond)
	}
	if mode == "lost" {
		file.Close()
	} else if mode != "silent" && !(mode == "mixed" && os.Args[len(os.Args)-1] == "/other") {
		status := "accepted"
		if mode == "held" && os.Args[len(os.Args)-1] == "/first" && os.Getenv("FVWM_TEST_FILEMANAGER_RECEIPT_STATUS") == "unknown" {
			status = "unknown"
		}
		writeFileManagerReceipt(file, status)
	}
	release := os.Getenv("FVWM_TEST_FILEMANAGER_RELEASE")
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		if _, err := os.Stat(release); err == nil {
			os.Exit(0)
		}
		time.Sleep(5 * time.Millisecond)
	}
	os.Exit(3)
}

func TestFileManagerAdmissionUnknownNeverKillsOrRetriesChild(t *testing.T) {
	executable, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	for _, mode := range []string{"accepted", "silent", "lost", "late", "mixed"} {
		t.Run(mode, func(t *testing.T) {
			release := filepath.Join(t.TempDir(), "release")
			t.Setenv("FVWM_TEST_FILEMANAGER_CHILD", mode)
			t.Setenv("FVWM_TEST_FILEMANAGER_RELEASE", release)
			children := newFileManagerChildren()
			defer func() {
				os.WriteFile(release, nil, 0600)
				deadline := time.Now().Add(3 * time.Second)
				for time.Now().Before(deadline) {
					children.drain()
					if len(children.live) == 0 {
						return
					}
					time.Sleep(5 * time.Millisecond)
				}
				t.Error("actual child exit did not release reservation")
			}()
			wait := 40 * time.Millisecond
			if mode == "accepted" || mode == "mixed" {
				wait = time.Second
			}
			ctx, cancel := context.WithTimeout(context.Background(), wait)
			defer cancel()
			groups := []options{{dir: "/tmp"}}
			if mode == "mixed" {
				groups = append(groups, options{dir: "/other"})
			}
			status := children.admit(ctx, executable, groups, "", "")
			want := "unknown"
			if mode == "accepted" {
				want = "accepted"
			}
			if status != want || len(children.live) != len(groups) || children.next != uint64(len(groups)) {
				t.Fatal(status, children.live, children.next)
			}
			// Let a late writer run after its observer closed, then confirm
			// the independent process still owns its one reservation.
			time.Sleep(150 * time.Millisecond)
			children.drain()
			if len(children.live) != len(groups) {
				t.Fatal("receipt uncertainty killed independent UI")
			}
		})
	}
	children := newFileManagerChildren()
	if status := children.admit(context.Background(), "/nonexistent/fvwm-file-browser", []options{{dir: "/tmp"}, {dir: "/other"}}, "", ""); status != "refused" || len(children.live) != 0 {
		t.Fatal("Start failure is not known refusal", status, children.live)
	}
}

func TestFileManagerCancelledAdmissionDoesNotStartChildren(t *testing.T) {
	children := newFileManagerChildren()
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if status := children.admit(ctx, "/must/not/execute", []options{{dir: "/tmp"}, {dir: "/other"}}, "", ""); status != "unknown" || len(children.live) != 0 || len(children.done) != 0 {
		t.Fatal(status, children.live)
	}
}

func TestFileManagerGroupsWaitForFirstAdmission(t *testing.T) {
	executable, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	for _, outcome := range []string{"accepted", "unknown", "unknown-receipt"} {
		t.Run(outcome, func(t *testing.T) {
			gate := t.TempDir()
			release := filepath.Join(gate, "release")
			admitFirst := filepath.Join(gate, "admit-first")
			t.Setenv("FVWM_TEST_FILEMANAGER_CHILD", "held")
			t.Setenv("FVWM_TEST_FILEMANAGER_RECEIPT_GATE", gate)
			status := "accepted"
			if outcome == "unknown-receipt" {
				status = "unknown"
			}
			t.Setenv("FVWM_TEST_FILEMANAGER_RECEIPT_STATUS", status)
			t.Setenv("FVWM_TEST_FILEMANAGER_RELEASE", release)
			children := newFileManagerChildren()
			ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
			result := make(chan string, 1)
			returned := false
			go func() {
				result <- children.admit(ctx, executable, []options{{dir: "/first"}, {dir: "/second"}}, "", "")
			}()
			defer func() {
				cancel()
				os.WriteFile(admitFirst, nil, 0600)
				os.WriteFile(release, nil, 0600)
				if !returned {
					select {
					case <-result:
					case <-time.After(3 * time.Second):
						t.Error("admission did not retire after cancellation")
						return
					}
				}
				deadline := time.Now().Add(3 * time.Second)
				for time.Now().Before(deadline) {
					children.drain()
					if len(children.live) == 0 {
						return
					}
					time.Sleep(5 * time.Millisecond)
				}
				t.Error("held test children did not actually exit")
			}()
			deadline := time.Now().Add(2 * time.Second)
			for {
				if _, err := os.Stat(filepath.Join(gate, "started-first")); err == nil {
					break
				}
				if time.Now().After(deadline) {
					t.Fatal("first actual child did not reach receipt gate")
				}
				time.Sleep(5 * time.Millisecond)
			}
			// The first child is known to have started and withholds its
			// receipt. The sibling must remain unstarted through this hold.
			deadline = time.Now().Add(100 * time.Millisecond)
			for time.Now().Before(deadline) {
				if _, err := os.Stat(filepath.Join(gate, "started-second")); !os.IsNotExist(err) {
					t.Fatal("second group started before first admission", err)
				}
				time.Sleep(5 * time.Millisecond)
			}
			if outcome == "accepted" || outcome == "unknown-receipt" {
				if err := os.WriteFile(admitFirst, nil, 0600); err != nil {
					t.Fatal(err)
				}
			} else {
				cancel()
			}
			select {
			case status := <-result:
				returned = true
				want := "unknown"
				if outcome == "accepted" {
					want = "accepted"
				}
				if status != want {
					t.Fatal(status, want)
				}
			case <-time.After(2 * time.Second):
				t.Fatal("held admission did not resolve")
			}
			children.drain()
			if outcome == "accepted" {
				if _, err := os.Stat(filepath.Join(gate, "started-second")); err != nil {
					t.Fatal("admitted first group did not allow second group", err)
				}
				if len(children.live) != 2 {
					t.Fatal("admitted children lost their reservations", children.live)
				}
			} else {
				if _, err := os.Stat(filepath.Join(gate, "started-second")); !os.IsNotExist(err) {
					t.Fatal("unknown admission started second group", err)
				}
				if len(children.live) != 1 || !children.live[1] || children.live[2] {
					t.Fatal("unknown child or unstarted reservations mishandled", children.live)
				}
				// All seven other slots are immediately available even while
				// the uncertain first child continues waiting independently.
				ids, ok := children.reserve(fileManagerMaxGroups - 1)
				if !ok {
					t.Fatal("unstarted group capacity was not released")
				}
				for _, id := range ids {
					delete(children.live, id)
				}
			}
		})
	}
}

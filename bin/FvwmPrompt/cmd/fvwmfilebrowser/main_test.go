package main

import (
	"bufio"
	"bytes"
	"context"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"fmt"
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

// Each invocation carries a native projection generation and Form ticket.
func testControl(b *browser, action, argument string, packet []byte) string {
	if b.nativeGeneration == 0 {
		b.nativeGeneration = 1
	}
	data := make([]byte, 4, 4+len(packet))
	binary.BigEndian.PutUint32(data, b.nativeGeneration)
	data = append(data, packet...)
	return fmt.Sprintf("FVWM_FILES_3 %s %d %s %s x%s %016x", b.instance,
		b.form, action, argument, base64.RawURLEncoding.EncodeToString(data), b.lastControl+1)
}

func TestMain(m *testing.M) {
	if len(os.Args) == 2 && os.Args[1] == "--job-worker" {
		if mode := os.Getenv("FVWM_TEST_JOB_WORKER"); mode != "" {
			os.Exit(jobProcessFixture(mode))
		}
		if runJobWorker() != nil {
			os.Exit(1)
		}
		os.Exit(0)
	}
	if path := os.Getenv("FVWM_TEST_ACTION_REPORT"); path != "" && len(os.Args) > 1 && os.Args[1] != "--worker" && os.Args[1] != "--watch-worker" {
		args := make([][]byte, 0, len(os.Args)-1)
		for _, arg := range os.Args[1:] {
			args = append(args, []byte(arg))
		}
		cwd, _ := os.Getwd()
		data, _ := json.Marshal(struct {
			Args [][]byte
			Cwd  []byte
		}{args, []byte(cwd)})
		if err := os.WriteFile(path+".part", data, 0600); err != nil {
			os.Exit(1)
		}
		if err := os.Rename(path+".part", path); err != nil {
			os.Exit(1)
		}
		os.Exit(0)
	}

	if len(os.Args) == 2 && os.Args[1] == "--watch-worker" {
		switch os.Getenv("FVWM_TEST_WATCH_WORKER") {
		case "oversize":
			os.Stdout.Write(bytes.Repeat([]byte("x"), 128*1024))
			os.Exit(0)
		case "out-of-order":
			fmt.Fprintln(os.Stdout, "changed")
			os.Exit(0)
		case "block":
			time.Sleep(time.Minute)
			os.Exit(0)
		}
		if runWatchWorker(os.Stdin, os.Stdout, watchLease) != nil {
			os.Exit(1)
		}
		os.Exit(0)
	}
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

func TestControlsUseRenderedGenerationAndNativeSelection(t *testing.T) {
	b := browser{instance: "owner", form: 3}
	b.model.Replace([]collection.Entry{{ID: "first", Label: "same"}, {ID: "second", Label: "same"}})
	message := testControl(&b, "cancel", "-", []byte{0, 0, 3})
	if _, err := b.action(strings.Replace(message, "owner 3", "owner 2", 1)); err == nil || len(b.model.Selected) != 0 {
		t.Fatal("stale callback mutated state")
	}
	if _, err := b.action(message); err != nil || len(b.model.Selected) != 2 {
		t.Fatal(err)
	}
	if close, err := b.action(testControl(&b, "close", "-", nil)); !close || err != nil {
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
	if _, err := b.action(testControl(&b, "cancel", "-", []byte{0, 0})); err != nil {
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

func TestViewControlsPreserveDomainSelectionAndObserveProperties(t *testing.T) {
	b := browser{instance: "owner", form: 7, snapshot: &filemodel.Snapshot{Schema: 1}}
	for i := 0; i < 20; i++ {
		b.snapshot.Items = append(b.snapshot.Items, filemodel.Item{ID: fmt.Sprintf("id%d", i), Name: []byte(fmt.Sprintf("file-%02d", i)), Kind: "file", Identity: filemodel.Identity{Size: int64(20 - i)}})
	}
	if err := b.reorder("name", false); err != nil {
		t.Fatal(err)
	}
	act := func(action, argument, picks string) error {
		entries := b.model.VisibleEntries()
		packet := make([]byte, 2+(len(entries)+7)/8)
		for i, entry := range entries {
			for _, id := range strings.Fields(picks) {
				if id == entry.ID {
					packet[2+i/8] |= 1 << uint(i%8)
				}
			}
		}
		_, err := b.action(testControl(&b, action, argument, packet))
		return err
	}
	filter := "x" + base64.RawURLEncoding.EncodeToString([]byte("file-19"))
	if act("filter", filter, "id0") != nil || b.model.VisibleCount() != 1 || b.model.HiddenSelectedCount() != 1 {
		t.Fatal("filter lost picked item")
	}
	if act("select-visible", "-", "") != nil || len(b.model.SelectedIDs()) != 2 {
		t.Fatal("page selection lost hidden item")
	}
	if act("properties", "-", "id19") != nil || len(b.inspected) != 2 {
		t.Fatal("properties lost filtered-out selection")
	}
	if act("clear-selection", "-", "id19") != nil || len(b.model.SelectedIDs()) != 0 {
		t.Fatal("clear missed hidden selection")
	}
	if act("properties", "-", "id19") != nil || (len(b.inspected) != 1 || b.inspected[0] != "id19") {
		t.Fatal("properties did not use selected ID")
	}
	if act("unfilter", "-", "id19") != nil || b.model.VisibleCount() != 20 {
		t.Fatal("clear filter")
	}
	if err := b.reorder("size", false); err != nil {
		t.Fatal(err)
	}
	page, _ := b.model.Page(0)
	if page[0].ID != "id19" || !b.model.Selected["id19"] || b.snapshot.Items[0].ID != "id0" {
		t.Fatal("sort changed target identity")
	}
	if act("filter", "x"+base64.RawURLEncoding.EncodeToString([]byte(strings.Repeat("x", 129))), "id19") == nil || b.model.Filter() != "" {
		t.Fatal("oversize filter accepted")
	}
}

func TestRefreshKeepsFocusedIdentityAndDropsRemovedSelection(t *testing.T) {
	left, right := net.Pipe()
	defer left.Close()
	defer right.Close()
	go io.Copy(io.Discard, right)
	s := &filemodel.Snapshot{Schema: 1, Path: []byte("/fixture")}
	for i := 0; i < 30; i++ {
		s.Items = append(s.Items, filemodel.Item{ID: fmt.Sprintf("id%d", i), Name: []byte(fmt.Sprintf("file-%02d", i)), Kind: "file"})
	}
	b := browser{session: transport.New(left), instance: "owner", snapshot: s}
	b.reorder("name", false)
	b.model.Focus = "id12"
	b.model.SelectPage(b.model.Generation, 12, []string{"id12", "id13"})
	next := *s
	next.Items = append([]filemodel.Item(nil), s.Items[1:]...)
	next.Items[11].Name = []byte("renamed")
	if err := completeBoundSnapshot(t, &b, workResult{Status: "loaded", Snapshot: &next}); err != nil {
		t.Fatal(err)
	}
	if b.model.Focus != "id12" || !b.model.Selected["id12"] {
		t.Fatal("refresh lost surviving focus/selection")
	}

	next.Items = next.Items[:10]
	if err := completeBoundSnapshot(t, &b, workResult{Status: "loaded", Snapshot: &next}); err != nil {
		t.Fatal(err)
	}
	if b.model.Focus == "id12" || len(b.model.SelectedIDs()) != 0 {
		t.Fatal("removed anchor/selection survived")
	}
}

func TestLargeNativeFormUsesAcknowledgedBatches(t *testing.T) {
	left, right := net.Pipe()
	defer left.Close()
	defer right.Close()
	b := browser{session: transport.New(left), instance: "owner", acks: make(chan string, 1)}
	entries := make([]collection.Entry, collection.MaxItems)
	for i := range entries {
		entries[i] = collection.Entry{ID: fmt.Sprintf("id%d", i), Label: "metadata"}
	}
	if err := b.model.Replace(entries); err != nil {
		t.Fatal(err)
	}
	done := make(chan error, 1)
	go func() {
		scanner := bufio.NewScanner(right)
		pending, rows := 0, 0
		for scanner.Scan() {
			line := scanner.Text()
			if strings.HasPrefix(line, "Echo FVWM_FILES_SYNC ") {
				if pending > 32 {
					done <- fmt.Errorf("unbounded batch: %d", pending)
					return
				}
				pending = 0
				b.acks <- strings.TrimPrefix(line, "Echo ")
				continue
			}
			pending++
			if strings.Contains(line, ": ListItem ") {
				rows++
			}
			if strings.HasPrefix(line, "Module FvwmForm ") {
				if rows != collection.MaxItems {
					done <- fmt.Errorf("rows: %d", rows)
					return
				}
				// show sends one final barrier after the map request.
				if !scanner.Scan() {
					done <- io.ErrUnexpectedEOF
					return
				}
				b.acks <- strings.TrimPrefix(scanner.Text(), "Echo ")
				if !scanner.Scan() || !strings.HasPrefix(scanner.Text(), "Echo FVWM_FILES_RENDER ") {
					done <- fmt.Errorf("missing render observation")
					return
				}
				if !scanner.Scan() || scanner.Text() != "Echo FVWM_FILES_WATCH owner off" {
					done <- fmt.Errorf("missing watch observation")
					return
				}
				done <- nil
				return
			}
		}
		done <- scanner.Err()
	}()
	if err := b.show(); err != nil {
		t.Fatal(err)
	}
	if err := <-done; err != nil {
		t.Fatal(err)
	}
}

func TestViewSwitchKeepsProjectionFocusAndSelection(t *testing.T) {
	b := browser{instance: "owner", form: 2}
	b.model.Replace([]collection.Entry{{ID: "a", Label: "first"}, {ID: "b", Label: "second"}})
	act := func(packet []byte) error {
		_, err := b.action(testControl(&b, "view", "-", packet))
		return err
	}
	if err := act([]byte{0, 1, 2}); err != nil || b.viewing() != "grid" || b.model.Focus != "b" || !b.model.Selected["b"] {
		t.Fatal(err, b.viewing())
	}
	generation := b.model.Generation
	if err := act([]byte{0, 1, 2}); err != nil || b.viewing() != "list" || b.model.Focus != "b" || !b.model.Selected["b"] || b.model.Generation != generation {
		t.Fatal("view switch changed model", err)
	}
	if err := act([]byte{0, 2, 2}); err == nil || b.viewing() != "list" {
		t.Fatal("malformed view change published")
	}
	for _, view := range []string{"", "list", "grid"} {
		o := options{dir: "/fixture", view: view}
		if err := prepareOptions(&o, true, nil, nil); err != nil {
			t.Fatal(err)
		}
	}
	o := options{dir: "/fixture", view: "unknown"}
	if prepareOptions(&o, true, nil, nil) == nil {
		t.Fatal("invalid view accepted")
	}
}

func TestCompletedRefreshPreservesNeighborsAndReportsChanges(t *testing.T) {
	left, right := net.Pipe()
	defer left.Close()
	defer right.Close()
	go io.Copy(io.Discard, right)
	root := t.TempDir()
	for _, name := range []string{"a", "b", "c"} {
		os.WriteFile(filepath.Join(root, name), []byte("old"), 0600)
	}
	before, _ := filemodel.Scan(root, false)
	b := browser{session: transport.New(left), instance: "refresh", snapshot: before, serial: 1}
	b.model.Replace(before.Entries())
	b.model.Focus = before.Items[1].ID
	b.model.SelectIDs(b.model.Generation, []string{before.Items[0].ID, before.Items[1].ID})
	b.inspected = []string{before.Items[1].ID}
	os.Remove(filepath.Join(root, "b"))
	os.WriteFile(filepath.Join(root, "a"), []byte("new larger metadata"), 0600)
	os.WriteFile(filepath.Join(root, "d"), nil, 0600)
	after, _ := filemodel.Scan(root, false)
	event := completedWork{work: pendingWork{serial: 1}, result: workResult{Status: "loaded", Snapshot: after}}
	if err := b.completed(event); err != nil {
		t.Fatal(err)
	}
	if b.model.Focus != before.Items[2].ID || len(b.model.Selected) != 1 || !strings.Contains(b.status, "1 added, 1 updated, 1 removed; 1 selected entries lost") || len(b.inspected) != 0 {
		t.Fatal(b.status, b.model.Focus, b.model.Selected, b.inspected)
	}
	generation := b.model.Generation
	if err := completeBoundSnapshot(t, &b, event.result); err != nil || b.model.Generation != generation || b.status != "Refreshed: no entry metadata changes." {
		t.Fatal(err, b.status)
	}
	// A superseded scan cannot publish a removed selection or old observation.
	event.work.serial = 0
	event.result.Snapshot = before
	if err := b.completed(event); err != nil || b.model.Generation != generation {
		t.Fatal("stale result published", err)
	}
}

func TestLiveCallbacksRefuseReplayAndOldNativeGeneration(t *testing.T) {
	b := browser{instance: "live", form: 1, nativeGeneration: 4}
	b.model.Replace([]collection.Entry{{ID: "one", Label: "same"}, {ID: "two", Label: "same"}})
	message := testControl(&b, "properties", "-", []byte{0, 1, 2})
	// A valid invocation can fail its domain action, but cannot be replayed.
	b.action(message)
	if b.lastControl != 1 || b.model.Focus != "two" {
		t.Fatal("fresh control not admitted")
	}
	if _, err := b.action(message); err == nil || b.lastControl != 1 {
		t.Fatal("replay admitted")
	}
	stale := testControl(&b, "clear-selection", "-", []byte{0, 0, 1})
	fields := strings.Fields(stale)
	old := []byte{0, 0, 0, 3, 0, 0, 1}
	fields[5] = "x" + base64.RawURLEncoding.EncodeToString(old)
	if _, err := b.action(strings.Join(fields, " ")); err == nil || !b.model.Selected["two"] || b.lastControl != 1 {
		t.Fatal("old projection admitted")
	}
	if _, err := b.action(testControl(&b, "clear-selection", "-", []byte{0, 1, 2})); err != nil || len(b.model.Selected) != 0 || !b.replaceSelection {
		t.Fatal("clear intent lost", err)
	}
}

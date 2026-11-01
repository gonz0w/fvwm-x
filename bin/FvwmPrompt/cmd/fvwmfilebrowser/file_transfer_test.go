// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filejobs"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/internal/filetransfer"
)

func transferSnapshot(t *testing.T, name string) (*filemodel.Snapshot, []string) {
	t.Helper()
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, name), []byte("fixture"), 0600); err != nil {
		t.Fatal(err)
	}
	s, err := filemodel.Scan(dir, true)
	if err != nil {
		t.Fatal(err)
	}
	return s, []string{s.Items[0].ID}
}
func transferBrowser(t *testing.T) browser {
	t.Helper()
	snapshot, _ := transferSnapshot(t, "destination-marker")
	b := browser{form: 3, nativeGeneration: 7, snapshot: snapshot, instance: strings.Repeat("a", 32)}
	entries, err := snapshot.OrderedEntries("name", false)
	if err != nil {
		t.Fatal(err)
	}
	if err = b.model.Replace(entries); err != nil {
		t.Fatal(err)
	}
	b.viewEntries = entries
	b.transfer = &browserTransfer{ready: true, epoch: 3, generation: 7, hostname: "local-host"}
	return b
}
func TestTransferNativeSelectionFreezesExactRowsAndBytes(t *testing.T) {
	b := transferBrowser(t)
	s, ids := transferSnapshot(t, string([]byte{'a', '%', 0xff}))
	b.snapshot = s
	entries, _ := s.OrderedEntries("name", false)
	b.model.Replace(entries)
	b.viewEntries = entries
	event := filetransfer.Event{Epoch: "3", Generation: 7, Rows: 1, Selection: base64.StdEncoding.EncodeToString([]byte{1})}
	text, err := b.selectedTransfer(&event)
	if err != nil {
		t.Fatal(err)
	}
	groups, err := filetransfer.Parse(text, "local-host")
	if err != nil {
		t.Fatal(err)
	}
	expected := []byte(filepath.Join(string(s.Path), string(s.Items[0].Name)))
	if len(groups) != 1 || !bytes.Equal(groups[0].Paths[0], expected) || !reflect.DeepEqual(ids, []string{entries[0].ID}) {
		t.Fatal("native selection changed identity/bytes")
	}
	// The model's selection is deliberately empty; exact native bits win.
	if len(b.model.SelectedIDs()) != 0 {
		t.Fatal("fixture selected")
	}
	event.Generation--
	if _, err = b.selectedTransfer(&event); err == nil {
		t.Fatal("stale native generation")
	}
	event.Generation++
	event.Rows++
	if _, err = b.selectedTransfer(&event); err == nil {
		t.Fatal("stale native row mapping")
	}
	b.transfer.epoch--
	if b.transferCurrent("3", 7) {
		t.Fatal("stale attachment")
	}
}
func TestTransferJobsRequireAdmitWaitAndSequentialComplete(t *testing.T) {
	b := transferBrowser(t)
	s1, ids1 := transferSnapshot(t, "one")
	s2, ids2 := transferSnapshot(t, "two")
	p := &transferPlan{proposal: filetransfer.Event{Epoch: "3", Generation: 7, Transfer: "1", Action: "copy"}, destination: bytes.Clone(b.snapshot.Path), destinationRoot: b.snapshot.Root, groups: []transferGroup{{s1, ids1}, {s2, ids2}}, deadline: time.Now().Add(time.Minute), count: 2}
	b.transfer.plan = p
	var requests []filejobs.Request
	b.transfer.launch = func(_ string, request filejobs.Request, _ chan<- jobNotice, _ chan jobNotice) *jobProcess {
		requests = append(requests, request)
		return &jobProcess{done: make(chan struct{})}
	}
	for _, phase := range []string{"observing", "confirming", "admitting"} {
		p.phase = phase
		if b.nextTransferJob() == nil {
			t.Fatalf("mutation before native admit in %s", phase)
		}
	}
	if len(requests) != 0 {
		t.Fatal("early launch")
	}
	p.phase = "running"
	if err := b.nextTransferJob(); err != nil {
		t.Fatal(err)
	}
	if len(requests) != 1 || requests[0].DestinationIdentity == nil || *requests[0].DestinationIdentity != p.destinationRoot {
		t.Fatal("missing captured destination")
	}
	if b.nextTransferJob() == nil || len(requests) != 1 {
		t.Fatal("started later group before actual Wait")
	}
	job := b.job
	job.state = "completed"
	job.terminal = &filejobs.Event{Kind: "finished", State: "completed"}
	job.outcomes = []filejobs.Event{{State: "completed", Outcome: &filejobs.Outcome{State: "completed", ContentComplete: true, MetadataComplete: true}}}
	if transferGroupCompleted(p, job, time.Now()) || p.index != 0 {
		t.Fatal("terminal without Wait advanced")
	}
	job.reaped = true
	if !transferGroupCompleted(p, job, time.Now()) || p.index != 1 {
		t.Fatal("completed group did not advance")
	}
	if len(requests) != 1 {
		t.Fatal("policy itself started next mutation")
	}
	if err := b.nextTransferJob(); err != nil {
		t.Fatal(err)
	}
	if len(requests) != 2 || requests[1].IDs[0] != ids2[0] || requests[0].ID == requests[1].ID || !bytes.Equal(requests[0].Destination, requests[1].Destination) {
		t.Fatal("group sequencing or identity")
	}
}
func TestTransferFailurePartialCancelUnknownAndDeadlineStopGroups(t *testing.T) {
	for _, state := range []string{"partial", "failed", "cancelled", "interrupted", "skipped", "completed"} {
		p := &transferPlan{deadline: time.Now().Add(time.Minute)}
		job := &jobState{reaped: true, state: state, request: filejobs.Request{IDs: []string{"one"}}, terminal: &filejobs.Event{Kind: "finished", State: state}, outcomes: []filejobs.Event{{State: state, Outcome: &filejobs.Outcome{State: state, ContentComplete: true, MetadataComplete: false}}}}
		if transferGroupCompleted(p, job, time.Now()) || p.index != 0 {
			t.Fatalf("advanced %s/metadata-incomplete", state)
		}
	}
	job := &jobState{reaped: true, state: "completed", request: filejobs.Request{IDs: []string{"one"}}, terminal: &filejobs.Event{Kind: "finished", State: "completed"}, outcomes: []filejobs.Event{{State: "completed", Outcome: &filejobs.Outcome{ContentComplete: true, MetadataComplete: true}}}}
	for _, p := range []*transferPlan{{deadline: time.Now().Add(-time.Second)}, {deadline: time.Now().Add(time.Minute), cancelled: true}} {
		if transferGroupCompleted(p, job, time.Now()) {
			t.Fatal("expired/cancelled advanced")
		}
	}
}
func TestTransferOptInServiceAndChild(t *testing.T) {
	o, err := parseOptions([]string{"--filemanager-service", "--file-transfer"}, t.TempDir(), os.Stderr)
	if err != nil || !o.fileTransfer || !o.fileManagerService {
		t.Fatal(o, err)
	}
	args := fileManagerChildArgs(options{dir: "/tmp", fileTransfer: true}, "")
	if !strings.Contains(strings.Join(args, " "), "--file-transfer") {
		t.Fatal("lost child opt-in", args)
	}
	if _, err := parseOptions([]string{"--reuse", "--file-transfer", "--dir", "/tmp"}, "/tmp", os.Stderr); err != nil {
		t.Fatal("opt-in reuse rejected", err)
	}
}

func TestTransferHelperAndCutLeaseSurviveFormReplacement(t *testing.T) {
	path := filepath.Join(t.TempDir(), "helper")
	script := `#!/bin/sh
printf '{"v":1,"event":"ready","hostname":"local-host"}\n'
n=0
while IFS= read -r command; do
 n=$((n+1))
 printf '{"v":1,"id":"%s","status":"ok"}\n' "$n"
done
`
	if err := os.WriteFile(path, []byte(script), 0700); err != nil {
		t.Fatal(err)
	}
	runtime := t.TempDir()
	client, err := filetransfer.Start(path, filepath.Join(runtime, "form"), strings.Repeat("a", 32))
	if err != nil {
		t.Fatal(err)
	}
	defer client.Close()
	b := transferBrowser(t)
	b.transfer = &browserTransfer{client: client, pending: map[string]transferCommand{}}
	if err := b.transferExpect(); err != nil {
		t.Fatal(err)
	}
	b.transfer.lease = filetransfer.Lease{ID: "10", Action: "move"}
	b.transfer.epoch, b.transfer.generation = b.form, b.nativeGeneration
	b.form++
	if err := b.transferExpect(); err != nil {
		t.Fatal(err)
	}
	if b.transfer.client != client || b.transfer.lease.ID != "10" || b.transfer.lease.Action != "move" {
		t.Fatal("Form navigation replaced helper or cut lease")
	}
	if b.transferCurrent("4", b.nativeGeneration) {
		t.Fatal("expected attachment before real native attach")
	}
}
func TestTransferUnavailableDoesNotDiscardBrowsing(t *testing.T) {
	b := transferBrowser(t)
	b.transfer = nil
	b.transferOptIn = true
	snapshot := b.snapshot
	t.Setenv("FVWM_FILE_TRANSFER_HELPER", "relative-helper")
	b.startTransfer()
	if b.transfer != nil || b.snapshot != snapshot || !b.dataReady() || !strings.Contains(b.transferSummary(), "unavailable") {
		t.Fatal("unavailable helper broke browsing")
	}
}

func TestTransferReadReceiptAndResultCorrelation(t *testing.T) {
	state := browserTransfer{readID: "7"}
	for _, event := range []filetransfer.Event{{ID: "8", Transfer: "100"}, {ID: "7", Transfer: "0100"}, {ID: "7"}} {
		if state.acceptReadReply(event) {
			t.Fatal("accepted unrelated or noncanonical read receipt")
		}
	}
	if !state.acceptReadReply(filetransfer.Event{ID: "7", Transfer: "100"}) {
		t.Fatal("current read receipt refused")
	}
	if state.retireRead("99") || state.readID != "7" || state.readTransfer != "100" {
		t.Fatal("source drag result changed pending clipboard read")
	}
	if !state.retireRead("100") || state.readID != "" || state.readTransfer != "" {
		t.Fatal("failed conversion retained busy read slot")
	}
	if state.retireRead("100") {
		t.Fatal("replayed result retired a later read")
	}
}

func TestNativePasteActionUsesDestinationWithoutSourceSelection(t *testing.T) {
	b := transferBrowser(t)
	if len(b.viewEntries) == 0 {
		t.Fatal("regression fixture needs a populated destination")
	}
	paste := filetransfer.Event{Event: "action", Kind: "paste", Transfer: "1", Epoch: "3", Generation: 7}
	if !b.validTransferAction(paste) {
		t.Fatal("zero-row Ctrl+V action refused for populated destination")
	}
	for _, invalid := range []filetransfer.Event{
		{Event: "action", Kind: "paste", Epoch: "2", Generation: 7},
		{Event: "action", Kind: "paste", Epoch: "3", Generation: 6},
		{Event: "action", Kind: "paste", Epoch: "3", Generation: 7, Rows: 1, Selection: "AQ=="},
		{Event: "action", Kind: "paste", Epoch: "3", Generation: 7, Selection: "AA=="},
	} {
		if b.validTransferAction(invalid) {
			t.Fatal("stale or source-bearing paste action admitted")
		}
	}
	for _, kind := range []string{"copy", "cut", "drag"} {
		source := paste
		source.Kind = kind
		if kind == "drag" {
			source.Event = "drag-begin"
		}
		if b.validTransferAction(source) {
			t.Fatalf("%s lost native source row guard", kind)
		}
		source.Rows = 1
		source.Selection = "AQ=="
		if !b.validTransferAction(source) {
			t.Fatalf("current exact native %s selection refused", kind)
		}
		source.Selection = "Ag=="
		if b.validTransferAction(source) {
			t.Fatalf("%s admitted nonzero selection padding", kind)
		}
	}
	b.transfer.epoch--
	if b.validTransferAction(paste) {
		t.Fatal("paste admitted through stale helper attachment")
	}
	b.transfer.epoch++
	b.viewEntries = nil
	if !b.validTransferAction(paste) {
		t.Fatal("paste refused for empty destination")
	}
}

// The fake helper only records its real stdin. Expectations come from the
// native command contract, not a second command builder or native dispatcher.
func TestTransferDragDataOmitsOfferTimestamp(t *testing.T) {
	directory := t.TempDir()
	capture := filepath.Join(directory, "commands.ndjson")
	t.Setenv("FVWM_TEST_FILE_TRANSFER_COMMANDS", capture)
	helper := filepath.Join(directory, "helper")
	script := `#!/bin/sh
while IFS= read -r command; do
 printf '%s\n' "$command" >> "$FVWM_TEST_FILE_TRANSFER_COMMANDS"
done
`
	if err := os.WriteFile(helper, []byte(script), 0700); err != nil {
		t.Fatal(err)
	}
	client, err := filetransfer.Start(helper, filepath.Join(directory, "form"), strings.Repeat("a", 32))
	if err != nil {
		t.Fatal(err)
	}
	defer client.Close()
	b := transferBrowser(t)
	b.transfer.client = client
	b.transfer.pending = map[string]transferCommand{}
	const nativeTime uint32 = 123456
	for _, kind := range []string{"copy", "cut", "drag"} {
		event := filetransfer.Event{Event: "action", Kind: kind, Transfer: "9001", Epoch: "3", Generation: 7, Time: nativeTime, Rows: 1, Selection: "AQ=="}
		if kind == "drag" {
			event.Event = "drag-begin"
			event.Kind = ""
		}
		if err := b.fileTransferAction(kind, &event); err != nil {
			t.Fatalf("%s action refused: %v", kind, err)
		}
		if event.Time != nativeTime {
			t.Fatal("native input timestamp changed")
		}
	}
	var data []byte
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) {
		data, err = os.ReadFile(capture)
		if err == nil && bytes.Count(data, []byte{'\n'}) == 3 {
			break
		}
		time.Sleep(5 * time.Millisecond)
	}
	if err != nil || bytes.Count(data, []byte{'\n'}) != 3 {
		t.Fatal("actual outgoing commands were not captured")
	}
	lines := bytes.Split(bytes.TrimSuffix(data, []byte{'\n'}), []byte{'\n'})
	for i, line := range lines {
		var fields map[string]json.RawMessage
		if err := json.Unmarshal(line, &fields); err != nil {
			t.Fatal("invalid emitted command JSON")
		}
		var command, action string
		if json.Unmarshal(fields["command"], &command) != nil || json.Unmarshal(fields["action"], &action) != nil {
			t.Fatal("missing command kind or action")
		}
		if i < 2 {
			var timestamp uint32
			if command != "offer" || json.Unmarshal(fields["time"], &timestamp) != nil || timestamp != nativeTime {
				t.Fatal("Copy/Cut offer lost its native timestamp")
			}
			expectedAction := "copy"
			if i == 1 {
				expectedAction = "move"
			}
			if action != expectedAction {
				t.Fatal("Copy/Cut offer changed operation")
			}
		} else {
			if command != "drag-data" || action != "copy" {
				t.Fatal("native drag command changed operation")
			}
			var transfer string
			if json.Unmarshal(fields["transfer"], &transfer) != nil || transfer != "9001" {
				t.Fatal("native drag command lost transfer correlation")
			}
			if _, present := fields["time"]; present {
				t.Fatal("drag-data emitted forbidden time field; native BEGIN already owns timestamp")
			}
		}
	}
}

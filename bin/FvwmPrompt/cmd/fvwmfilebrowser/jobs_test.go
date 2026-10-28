// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

package main

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filejobs"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

func jobFixtureView() *jobView {
	return &jobView{lifetime: 7, generation: 19, ids: []string{"first"}, snapshot: &filemodel.Snapshot{
		Schema: 1, Path: []byte("/fixture/$quoted"), Root: filemodel.Identity{Mode: os.ModeDir | 0700},
		Items: []filemodel.Item{{ID: "first", Name: []byte{'a', '\xff', '\n', '$'}, Kind: "file", Identity: filemodel.Identity{Mode: 0600}},
			{ID: "unselected", Name: []byte("other")}}}}
}

func TestJobRequestCapturesLiteralTargetsAndConfirmation(t *testing.T) {
	view := jobFixtureView()
	name := []byte("literal$%\\\"name")
	destination := []byte("/different/'$folder")
	request, err := jobRequest(view, "instance", 3, "copy", destination, name, false)
	if err != nil {
		t.Fatal(err)
	}
	if request.ID != "instance-3" || request.Generation != 3 || request.Operation != "copy" ||
		!bytes.Equal(request.Destination, destination) || len(request.Snapshot.Items) != 1 ||
		!bytes.Equal(request.Snapshot.Items[0].Name, []byte{'a', '\xff', '\n', '$'}) {
		t.Fatal("literal captured request lost identity", request)
	}
	destination[0], name[0], view.ids[0], view.snapshot.Path[0], view.snapshot.Items[0].Name[0] = 'x', 'x', "replaced", 'x', 'x'
	if request.Destination[0] != '/' || request.Name[0] != 'l' || request.IDs[0] != "first" ||
		request.Snapshot.Path[0] != '/' || request.Snapshot.Items[0].Name[0] != 'a' {
		t.Fatal("live input aliases admitted job request")
	}
	view = jobFixtureView()
	if _, err := jobRequest(view, "instance", 1, "delete", nil, nil, false); err == nil {
		t.Fatal("unconfirmed permanent delete admitted")
	}
	if request, err := jobRequest(view, "instance", 1, "delete", nil, nil, true); err != nil || !request.Permanent {
		t.Fatal("explicit delete refused", request, err)
	}
	view.ids = nil
	if _, err := jobRequest(view, "instance", 1, "mkdir", nil, []byte("new"), false); err != nil {
		t.Fatal("folder creation incorrectly requires a selection", err)
	}
	if _, err := jobRequest(view, "instance", 1, "copy", []byte("/destination"), nil, false); err == nil {
		t.Fatal("copy admitted without selection")
	}
	for _, bad := range [][]byte{nil, []byte(".."), []byte("a/b"), []byte("\x00"), bytes.Repeat([]byte{'a'}, 256)} {
		if _, err := jobRequest(view, "instance", 1, "mkdir", nil, bad, false); err == nil {
			t.Fatal("unsafe or unbounded destination name admitted", bad)
		}
	}
	for _, bad := range []string{"unknown", "Copy", ""} {
		if _, err := jobRequest(view, "instance", 1, bad, nil, nil, false); err == nil {
			t.Fatal("unknown operation admitted")
		}
	}
}

func TestJobUnusedDraftsAndIndependentGeneration(t *testing.T) {
	view := jobFixtureView()
	for _, operation := range []string{"trash", "restore", "delete"} {
		request, err := jobRequest(view, "instance", 8, operation,
			[]byte("/ignored-destination"), []byte("ignored-name"), operation == "delete")
		if err != nil || len(request.Destination) != 0 || len(request.Name) != 0 || request.Generation != 8 {
			t.Fatal(operation, request, err)
		}
	}
	request, err := jobRequest(view, "instance", 9, "copy", []byte("../other"), nil, false)
	if err != nil || string(request.Destination) != "/fixture/other" || request.Generation != 9 {
		t.Fatal("relative destination or independent job generation", request, err)
	}
	request, err = jobRequest(view, "instance", 10, "copy", nil, nil, false)
	if err != nil || !bytes.Equal(request.Destination, view.snapshot.Path) {
		t.Fatal("empty destination did not retain literal captured source", request, err)
	}
	b := &browser{jobs: view}
	view.admitted = true
	if err := b.startJob("copy", nil, nil, false); err == nil || b.job != nil {
		t.Fatal("same captured dialog implicitly replayed a second mutation")
	}
}

func TestJobEventIdentityAndCoalescing(t *testing.T) {
	request := filejobs.Request{Schema: 1, ID: "job", Generation: 7}
	valid := filejobs.Event{Schema: 1, ID: "job", Generation: 7, Sequence: 3, Kind: "progress", State: "running", Done: 2}
	if !validJobEvent(valid, request, 2) {
		t.Fatal("current bounded progress refused")
	}
	for _, mutate := range []func(*filejobs.Event){
		func(e *filejobs.Event) { e.ID = "retired" },
		func(e *filejobs.Event) { e.Generation++ },
		func(e *filejobs.Event) { e.Sequence = 2 },
		func(e *filejobs.Event) { e.Kind = "unknown" },
		func(e *filejobs.Event) { e.Kind, e.State = "finished", "unknown" },
		func(e *filejobs.Event) { e.Kind = "decision" },
		func(e *filejobs.Event) { e.TotalKnown, e.Total = true, 1 },
		func(e *filejobs.Event) { e.Path = bytes.Repeat([]byte{'x'}, filemodel.MaxPath+1) },
	} {
		candidate := valid
		mutate(&candidate)
		if validJobEvent(candidate, request, 2) {
			t.Fatal("stale or malformed event admitted", candidate)
		}
	}
	mailbox := make(chan jobNotice, 1)
	for i := uint64(1); i <= 100; i++ {
		event := valid
		event.Sequence = i
		offerJobProgress(mailbox, jobNotice{id: "job", event: &event})
	}
	if len(mailbox) != 1 || (<-mailbox).event.Sequence != 100 {
		t.Fatal("progress mailbox failed latest-only bound")
	}
}

func TestJobTerminalRetainsSlotUntilReapAndIgnoresLateProgress(t *testing.T) {
	request := filejobs.Request{Schema: 1, ID: "job", Generation: 7}
	b := &browser{job: &jobState{request: request, state: "running"}}
	terminal := filejobs.Event{Schema: 1, ID: "job", Generation: 7, Sequence: 8, Kind: "finished", State: "partial"}
	if err := b.jobNotice(jobNotice{id: "job", event: &terminal}); err != nil {
		t.Fatal(err)
	}
	if b.job.reaped || b.job.state != "finishing" || b.job.terminal == nil {
		t.Fatal("terminal record released an unreaped mutation slot")
	}
	late := terminal
	late.Kind, late.State, late.Sequence = "progress", "running", 4
	b.jobNotice(jobNotice{id: "job", event: &late})
	if b.job.state != "finishing" || b.job.lastSequence != 8 {
		t.Fatal("coalesced old progress overwrote terminal state")
	}
	b.jobNotice(jobNotice{id: "other", reaped: true})
	if b.job.reaped {
		t.Fatal("other child's Wait released job")
	}
	b.jobNotice(jobNotice{id: "job", reaped: true})
	if !b.job.reaped || b.job.state != "partial" {
		t.Fatal("reaped terminal outcome not retained")
	}
	b.jobNotice(jobNotice{id: "job", event: &late})
	if b.job.state != "partial" {
		t.Fatal("late child record changed retained outcome")
	}
	b = &browser{job: &jobState{request: request, state: "running"}}
	b.jobNotice(jobNotice{id: "job", reaped: true, err: "worker died"})
	if b.job.state != "interrupted" || !b.job.reaped || !strings.Contains(b.job.latest.Detail, "worker died") {
		t.Fatal("worker loss inferred completion or replay")
	}
}

func TestJobReliableOutcomesSurviveNewerCoalescedProgress(t *testing.T) {
	request := filejobs.Request{Schema: 1, ID: "job", Generation: 7}
	b := &browser{job: &jobState{request: request, state: "running"}}
	offer := func(sequence uint64, kind, state string, decision uint64) {
		t.Helper()
		event := filejobs.Event{Schema: 1, ID: "job", Generation: 7, Sequence: sequence,
			Kind: kind, State: state, Decision: decision, Done: sequence}
		if kind == "item" {
			event.Outcome = &filejobs.Outcome{State: state}
		}
		if err := b.jobNotice(jobNotice{id: "job", event: &event}); err != nil {
			t.Fatal(err)
		}
	}
	// Main-loop select may consume progress3 before reliable item2. Wire
	// order was already checked by the reader, so both must survive.
	offer(3, "progress", "running", 0)
	offer(2, "item", "completed", 0)
	offer(2, "item", "completed", 0)
	if len(b.job.outcomes) != 1 || b.job.lastReliable != 2 ||
		b.job.lastSequence != 3 || b.job.latest.Done != 3 {
		t.Fatal("reliable item lost, duplicated or rolled back progress", b.job)
	}
	offer(6, "progress", "running", 0)
	offer(4, "decision", "waiting", 42)
	if b.job.decision != 42 || b.job.state != "decision" || b.job.latest.Done != 6 {
		t.Fatal("reordered decision lost or display rolled back", b.job)
	}
	offer(4, "decision", "waiting", 43)
	if b.job.decision != 42 {
		t.Fatal("duplicate reliable decision replaced its token")
	}
	offer(7, "progress", "running", 0)
	if b.job.state != "decision" {
		t.Fatal("progress cleared a pending reliable decision")
	}
	offer(5, "item", "skipped", 0)
	if len(b.job.outcomes) != 2 || b.job.decision != 0 || b.job.state != "running" || b.job.latest.Done != 7 {
		t.Fatal("reliable result failed to resolve decision without display rollback")
	}
	offer(8, "finished", "partial", 0)
	offer(9, "progress", "running", 0)
	if b.job.state != "finishing" || b.job.reaped || b.job.lastSequence != 8 {
		t.Fatal("terminal did not retain ownership and refuse later progress")
	}
	b.jobNotice(jobNotice{id: "job", reaped: true})
	if !b.job.reaped || b.job.state != "partial" || len(b.job.outcomes) != 2 {
		t.Fatal("reap failed to retain every reliable outcome")
	}
}

func TestJobDeleteConfirmationPreviewsCapturedNamesWithoutAdmission(t *testing.T) {
	view := jobFixtureView()
	for i := 1; i < 10; i++ {
		id := fmt.Sprintf("target-%d", i)
		view.ids = append(view.ids, id)
		view.snapshot.Items = append(view.snapshot.Items, filemodel.Item{ID: id,
			Name: []byte(fmt.Sprintf("hidden-%d", i)), Kind: "file"})
	}
	view.hidden, view.page = 9, 2
	b := &browser{jobs: view}
	if err := b.armJobDelete(); err != nil {
		t.Fatal(err)
	}
	if b.job != nil || view.admitted || !view.confirmDelete || view.page != 0 || view.hidden != 9 {
		t.Fatal("arming admitted a mutation or changed the captured selection")
	}
	rows := jobResultRows(view, nil)
	if len(rows) != 10 || rows[0] != displayText("Delete target: a\xff\n$", 140) ||
		rows[9] != "Delete target: hidden-9" || strings.ContainsAny(rows[0], "\n\r") {
		t.Fatal("confirmation omitted hidden names or unsafe display bytes", rows)
	}
	first, page := jobRowsPage(rows, 0)
	last, lastPage := jobRowsPage(rows, 1)
	if len(first) != jobPageSize || page != 0 || len(last) != 2 || lastPage != 1 || last[1] != rows[9] {
		t.Fatal("captured confirmation target paging lost names")
	}
	// A List label/selection cannot retarget IDs or filename bytes. Only the
	// distinct confirmation path passes Permanent=true to admission.
	rows[0] = "different presentation"
	if _, err := jobRequest(view, "instance", 1, "delete", nil, nil, false); err == nil {
		t.Fatal("armed confirmation alone authorized permanent deletion")
	}
	request, err := jobRequest(view, "instance", 1, "delete", nil, nil, true)
	if err != nil || !reflect.DeepEqual(request.IDs, view.ids) ||
		!bytes.Equal(request.Snapshot.Items[0].Name, []byte{'a', '\xff', '\n', '$'}) {
		t.Fatal("confirmed request changed captured targets", request, err)
	}
	view.confirmDelete, view.admitted = false, true
	if rows := jobResultRows(view, &jobState{outcomes: []filejobs.Event{{State: "cancelled", Path: []byte("/fixture/a")}}}); len(rows) != 1 || rows[0] != "cancelled: a" {
		t.Fatal("admitted job did not replace confirmation names with honest outcomes", rows)
	}
}

func TestJobSharedTrashLocation(t *testing.T) {
	for _, value := range []string{"", "relative/path", "/absolute/data"} {
		want := "/home/fixture/.local/share/Trash/files"
		if filepath.IsAbs(value) {
			want = "/absolute/data/Trash/files"
		}
		if path, err := filejobs.TrashFilesPath("/home/fixture", value); err != nil || path != want {
			t.Fatal("native and worker trash locations diverged", value, path, err)
		}
	}
}

func TestJobControlsAreBoundedAndDecisionBound(t *testing.T) {
	controls := make(chan filejobs.Control, 1)
	b := &browser{job: &jobState{request: filejobs.Request{ID: "job"}, process: &jobProcess{controls: controls}, decision: 42}}
	if err := b.decideJob("rename", []byte("new-name")); err != nil {
		t.Fatal(err)
	}
	if len(controls) != 1 || b.job.decision != 0 {
		t.Fatal("decision failed to consume exact pending conflict")
	}
	control := <-controls
	if control.ID != "job" || control.Decision != 42 || control.Choice != "rename" || string(control.Name) != "new-name" {
		t.Fatal("decision lost opaque identity", control)
	}
	if err := b.decideJob("skip", nil); err == nil {
		t.Fatal("decision replay accepted")
	}
	b.job.decision = 43
	controls <- control
	if err := b.decideJob("skip", nil); err == nil || b.job.decision != 43 {
		t.Fatal("full control mailbox blocked or consumed pending conflict")
	}
}

func TestJobCallbacksRequireLifetimeAndExactBytes(t *testing.T) {
	encode := func(value []byte) string { return "x" + base64.RawURLEncoding.EncodeToString(value) }
	message := fmt.Sprintf("FVWM_FILES_JOBS_1 instance 7 copy %s %s %s %016x", encode([]byte("/tmp/$folder")), encode([]byte{'a', '\xff', '\n'}), detailPacket(1, 0), 1)
	callback, ok := parseJobCallback(message, "instance")
	if !ok || callback.lifetime != 7 || callback.action != "copy" || string(callback.destination) != "/tmp/$folder" || !reflect.DeepEqual(callback.name, []byte{'a', '\xff', '\n'}) {
		t.Fatal("typed callback changed bytes", callback, ok)
	}
	for _, bad := range []string{strings.Replace(message, "instance", "retired", 1), strings.Replace(message, " 7 ", " 07 ", 1), strings.Replace(message, " copy ", " unknown ", 1), message + " extra", strings.TrimSuffix(message, "1") + "0"} {
		if _, ok := parseJobCallback(bad, "instance"); ok {
			t.Fatal("malformed job callback admitted", bad)
		}
	}
	b := &browser{jobs: &jobView{lifetime: 8}}
	if accepted, err := b.jobAction(callback); accepted || err != nil {
		t.Fatal("retired view acted", accepted, err)
	}
	b.jobs.lifetime, b.jobs.lastControl = 7, 2
	if accepted, err := b.jobAction(callback); accepted || err != nil {
		t.Fatal("invocation replay acted", accepted, err)
	}
	b.jobs.lastControl, b.jobs.listGeneration = 0, 2
	if accepted, err := b.jobAction(callback); accepted || err != nil || b.jobs.lastControl != 0 {
		t.Fatal("prior conflict generation acted on a new native publication", accepted, err)
	}
}

// These modes exist only in this test executable, never the installed worker.
// The hold fixture makes terminal-before-Wait deterministic without a timing
// assertion: the test explicitly releases a child in its own temporary root.
func jobProcessFixture(mode string) int {
	var request filejobs.Request
	if json.NewDecoder(os.Stdin).Decode(&request) != nil {
		return 2
	}
	encoder := json.NewEncoder(os.Stdout)
	event := filejobs.Event{Schema: 1, ID: request.ID, Generation: request.Generation,
		Sequence: 1, Kind: "accepted", State: "accepted"}
	if encoder.Encode(event) != nil {
		return 3
	}
	if mode == "die" {
		return 9
	}
	event.Sequence, event.Kind, event.State = 2, "finished", "completed"
	if encoder.Encode(event) != nil {
		return 4
	}
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) {
		if _, err := os.Stat(string(request.Destination)); err == nil {
			return 0
		}
		time.Sleep(10 * time.Millisecond)
	}
	return 5
}

func TestJobProcessWaitAndLostHelperAreObserved(t *testing.T) {
	executable, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	for _, mode := range []string{"hold", "die"} {
		t.Run(mode, func(t *testing.T) {
			t.Setenv("FVWM_TEST_JOB_WORKER", mode)
			request := filejobs.Request{Schema: 1, ID: "process-fixture", Generation: 7,
				Destination: []byte(filepath.Join(t.TempDir(), "release"))}
			reliable, progress := make(chan jobNotice, 16), make(chan jobNotice, 1)
			process := launchJob(executable, request, reliable, progress)
			t.Cleanup(func() { process.cancel(); <-process.done })
			b := &browser{job: &jobState{request: request, process: process}}
			deadline := time.NewTimer(5 * time.Second)
			defer deadline.Stop()
			for !b.job.reaped {
				select {
				case notice := <-reliable:
					if err := b.jobNotice(notice); err != nil {
						t.Fatal(err)
					}
					if mode == "hold" && b.job.terminal != nil && !notice.reaped {
						if b.job.reaped || b.job.state != "finishing" {
							t.Fatal("terminal released an owned child")
						}
						select {
						case <-process.done:
							t.Fatal("child exited before test release")
						default:
						}
						if err := os.WriteFile(string(request.Destination), nil, 0600); err != nil {
							t.Fatal(err)
						}
					}
				case <-deadline.C:
					t.Fatal("owned worker did not reap")
				}
			}
			if mode == "hold" && b.job.state != "completed" {
				t.Fatal("reaped completed result lost", b.job.state)
			}
			if mode == "die" && b.job.state != "interrupted" {
				t.Fatal("lost helper inferred completion", b.job.state)
			}
		})
	}
}

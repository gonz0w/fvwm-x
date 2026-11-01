// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"context"
	"crypto/rand"
	"encoding/hex"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filejobs"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/internal/filetransfer"
)

// Meson supplies the installed private helper; no PATH fallback is used.
var fileTransferHelperInstallPath string

const transferConfirmLimit = 30 * time.Second
const transferJobLimit = 300 * time.Second

type transferCommand struct {
	kind, action, transfer string
	until                  time.Time
}
type transferGroup struct {
	snapshot *filemodel.Snapshot
	ids      []string
}
type transferPlan struct {
	proposal        filetransfer.Event
	destination     []byte
	destinationRoot filemodel.Identity
	groups          []transferGroup
	count, index    int
	phase           string
	deadline        time.Time
	outcomes        []filejobs.Event
	cancelled       bool
	cancelNotified  bool
}
type transferPrepared struct {
	plan *transferPlan
	err  bool
}
type transferObservation struct {
	cancel context.CancelFunc
	done   chan struct{}
}
type browserTransfer struct {
	launch                       func(string, filejobs.Request, chan<- jobNotice, chan jobNotice) *jobProcess
	client                       *filetransfer.Client
	dir, socket, token, hostname string
	ready                        bool
	exited                       bool
	epoch                        uint64
	generation                   uint32
	contextEpoch                 uint64
	contextGeneration            uint32
	contextEnabled               bool
	pending                      map[string]transferCommand
	lease                        filetransfer.Lease
	plan                         *transferPlan
	observation                  *transferObservation
	prepared                     chan transferPrepared
	readID                       string
	readTransfer                 string
	lastTransfer                 uint64
	lastProposal                 uint64
	deferred                     []filetransfer.Event
	startup                      time.Time
	summary                      string
}

func transferHelperPath(getenv func(string) string) (string, error) {
	path := getenv("FVWM_FILE_TRANSFER_HELPER")
	if path == "" {
		path = fileTransferHelperInstallPath
	}
	if !filepath.IsAbs(path) || strings.ContainsAny(path, "\x00\r\n") {
		return "", errors.New("File transfer needs its installed native helper")
	}
	return path, nil
}
func (b *browser) startTransfer() {
	path, err := transferHelperPath(os.Getenv)
	if err != nil {
		b.status = "File transfer unavailable; browsing remains available"
		return
	}
	nonce := make([]byte, 16)
	if _, err = rand.Read(nonce); err != nil {
		return
	}
	// A short /tmp directory avoids the smaller BSD sockaddr_un limit. MkdirTemp
	// creates 0700. Only socket/token/epoch metadata crosses core configuration;
	// URI payloads stay on the inherited private helper pipe.
	dir, err := os.MkdirTemp("/tmp", "fvwm-ft-")
	if err != nil {
		return
	}
	socket := filepath.Join(dir, "form")
	token := hex.EncodeToString(nonce)
	client, err := filetransfer.Start(path, socket, token)
	if err != nil {
		os.Remove(dir)
		b.status = "File transfer unavailable; browsing remains available"
		return
	}
	b.transfer = &browserTransfer{client: client, dir: dir, socket: socket, token: token, hostname: "", pending: map[string]transferCommand{}, prepared: make(chan transferPrepared, 1), launch: launchJob, startup: time.Now().Add(5 * time.Second), summary: "File transfer starting"}
}
func (b *browser) shutdownTransfer() {
	t := b.transfer
	if t == nil {
		return
	}
	t.client.Stop()
	if t.plan != nil {
		t.plan.cancelled = true
		t.plan.cancelNotified = true
		if t.plan.phase == "running" {
			b.cancelJob()
		}
	}
	if t.observation != nil {
		t.observation.cancel()
		<-t.observation.done
		t.observation = nil
	}
	t.client.Close()
	os.RemoveAll(t.dir)
	b.transfer = nil
}
func (b *browser) transferEvents() <-chan filetransfer.Event {
	if b.transfer == nil {
		return nil
	}
	return b.transfer.client.Events
}
func (b *browser) transferDone() <-chan struct{} {
	if b.transfer == nil || b.transfer.exited {
		return nil
	}
	return b.transfer.client.Done
}
func (b *browser) transferPrepared() <-chan transferPrepared {
	if b.transfer == nil {
		return nil
	}
	return b.transfer.prepared
}
func (b *browser) transferSummary() string {
	if b.transfer == nil {
		return "File transfer unavailable"
	}
	return b.transfer.summary
}

func (b *browser) transferSend(command filetransfer.Command, kind string) error {
	t := b.transfer
	if t == nil {
		return errors.New("File transfer unavailable")
	}
	if len(t.pending) >= filetransfer.MaxQueue {
		b.transferUnavailable()
		return errors.New("File transfer control queue full")
	}
	id, err := t.client.Send(command)
	if err != nil {
		return err
	}
	t.pending[id] = transferCommand{kind: kind, action: command.Action, transfer: command.Transfer, until: time.Now().Add(5 * time.Second)}
	if command.Command == "read" {
		t.readID = id
		t.readTransfer = ""
	}
	return nil
}

// The two control paths have independent scheduling. Observe the actual
// expect-form acknowledgement before asking core to start the new Form.
func (b *browser) transferExpect() error {
	t := b.transfer
	if t == nil || t.exited {
		return nil
	}
	t.epoch, t.generation = 0, 0
	t.contextEpoch, t.contextGeneration = 0, 0
	deadline := time.NewTimer(5 * time.Second)
	defer deadline.Stop()
	for !t.ready {
		select {
		case event := <-t.client.Events:
			t.client.Received(event)
			if event.Event != "ready" {
				return errors.New("Native file transfer startup unavailable")
			}
			t.ready = true
			t.hostname = event.Hostname
			t.summary = "File transfer waiting for native Form attachment"
		case <-t.client.Done:
			return errors.New("Native file transfer helper ended")
		case <-deadline.C:
			return errors.New("Native file transfer startup timed out")
		}
	}
	if err := b.transferSend(filetransfer.Command{Command: "expect-form", Epoch: strconv.FormatUint(b.form, 10)}, "expect-form"); err != nil {
		return err
	}
	id := ""
	for key, command := range t.pending {
		if command.kind == "expect-form" {
			id = key
		}
	}
	for {
		select {
		case event := <-t.client.Events:
			if event.Event == "" && event.ID == id {
				t.client.Received(event)
				delete(t.pending, id)
				if event.Status != "ok" {
					return errors.New("Native file transfer Form refused")
				}
				return nil
			}
			if len(t.deferred) == filetransfer.MaxQueue {
				return errors.New("Native file transfer event queue full")
			}
			t.deferred = append(t.deferred, event)
		case <-t.client.Done:
			return errors.New("Native file transfer helper ended")
		case <-deadline.C:
			return errors.New("Native file transfer Form acknowledgement timed out")
		}
	}
}

func (b *browser) transferContext() {
	t := b.transfer
	if t == nil || !t.ready || b.form == 0 || b.nativeGeneration == 0 {
		return
	}
	enabled := b.snapshot != nil && !b.active && b.pending == nil && t.epoch == b.form && t.generation == b.nativeGeneration
	if t.contextEpoch == b.form && t.contextGeneration == b.nativeGeneration && t.contextEnabled == enabled {
		return
	}
	if err := b.transferSend(filetransfer.Command{Command: "context", Epoch: strconv.FormatUint(b.form, 10), Generation: b.nativeGeneration, Enabled: &enabled}, "context"); err != nil {
		b.transferUnavailable()
		return
	}
	t.contextEpoch, t.contextGeneration, t.contextEnabled = b.form, b.nativeGeneration, enabled
}
func (b *browser) transferCurrent(epoch string, generation uint32) bool {
	t := b.transfer
	return t != nil && t.ready && t.epoch == b.form && t.generation == b.nativeGeneration && epoch == strconv.FormatUint(b.form, 10) && generation == b.nativeGeneration && b.dataReady()
}
func (b *browser) transferUnavailable() {
	t := b.transfer
	if t == nil {
		return
	}
	t.ready = false
	t.summary = "File transfer unavailable; browsing remains available"
	t.client.Stop()
	if t.observation != nil {
		t.observation.cancel()
	}
	if t.plan != nil {
		t.plan.cancelled = true
		if t.plan.phase == "running" {
			b.cancelJob()
		}
	}
}

// Paste names only the current destination: Form deliberately sends no source
// rows or selection bits. Copy, Cut and drag retain the exact native row map.
func (b *browser) validTransferAction(event filetransfer.Event) bool {
	if !b.transferCurrent(event.Epoch, event.Generation) {
		return false
	}
	if event.Event == "action" && event.Kind == "paste" {
		return event.Rows == 0 && event.Selection == ""
	}
	if int(event.Rows) != len(b.viewEntries) {
		return false
	}
	_, err := event.Selected()
	return err == nil
}

// selectedTransfer freezes exact current native rows against current file IDs;
// labels and filesystem lookups never participate in source drag admission.
func (b *browser) selectedTransfer(event *filetransfer.Event) (string, error) {
	if b.snapshot == nil {
		return "", errors.New("Folder is not ready")
	}
	ids := b.model.SelectedIDs()
	if event != nil {
		if !b.transferCurrent(event.Epoch, event.Generation) || int(event.Rows) != len(b.viewEntries) {
			return "", errors.New("Stale file selection")
		}
		rows, err := event.Selected()
		if err != nil {
			return "", err
		}
		ids = nil
		for _, row := range rows {
			ids = append(ids, b.viewEntries[row].ID)
		}
	}
	if len(ids) == 0 || len(ids) > filetransfer.MaxReferences {
		return "", errors.New("Select one to 256 files or folders")
	}
	selected := map[string]filemodel.Item{}
	for _, item := range b.snapshot.Items {
		selected[item.ID] = item
	}
	paths := make([][]byte, 0, len(ids))
	for _, id := range ids {
		item, ok := selected[id]
		if !ok || (item.Kind != "file" && item.Kind != "directory") || len(item.Name) == 0 || bytes.ContainsAny(item.Name, "/\x00") || string(item.Name) == "." || string(item.Name) == ".." {
			return "", errors.New("Selected file references are unavailable")
		}
		paths = append(paths, []byte(filepath.Join(string(b.snapshot.Path), string(item.Name))))
	}
	return filetransfer.Encode(paths, b.transfer.hostname)
}
func (b *browser) fileTransferAction(kind string, event *filetransfer.Event) error {
	t := b.transfer
	if t == nil || !b.transferCurrent(strconv.FormatUint(b.form, 10), b.nativeGeneration) {
		return errors.New("File transfer is not attached to a ready folder")
	}
	timestamp := uint32(0)
	if event != nil {
		timestamp = event.Time
	}
	if kind == "paste" {
		if t.plan != nil || t.readID != "" {
			return errors.New("A file transfer is already pending")
		}
		return b.transferSend(filetransfer.Command{Command: "read", Time: timestamp}, "read")
	}
	if kind != "copy" && kind != "cut" && kind != "drag" {
		return errors.New("Unsupported file transfer action")
	}
	uris, err := b.selectedTransfer(event)
	if err != nil {
		return err
	}
	action := "copy"
	if kind == "cut" {
		action = "move"
	}
	command := filetransfer.Command{Command: "offer", Action: action, URIs: uris, Time: timestamp}
	if kind == "drag" {
		command.Command = "drag-data"
		command.Transfer = event.Transfer
		// Native BEGIN already owns the genuine input timestamp.
		command.Time = 0
	}
	return b.transferSend(command, command.Command)
}

func (b *browser) transferEvent(event filetransfer.Event) error {
	t := b.transfer
	if t == nil {
		return nil
	}
	t.client.Received(event)
	if t.exited {
		return nil
	}
	if event.Event == "" {
		command, ok := t.pending[event.ID]
		if !ok {
			b.transferUnavailable()
			return nil
		}
		delete(t.pending, event.ID)
		if event.Transfer != "" && (command.kind != "read" || event.Status != "ok") || event.Lease != "" && (command.kind != "offer" || event.Status != "ok") {
			b.transferUnavailable()
			return nil
		}
		if event.Status != "ok" {
			if command.kind == "read" {
				t.readID, t.readTransfer = "", ""
			}
			if command.kind == "admit" {
				b.finishTransfer(false)
			}
			t.summary = "File transfer request refused"
			return b.show()
		}
		switch command.kind {
		case "read":
			if !t.acceptReadReply(event) {
				b.transferUnavailable()
				return nil
			}
		case "offer":
			if !filetransfer.CanonicalID(event.Lease) {
				b.transferUnavailable()
				return nil
			}
			t.lease = filetransfer.Lease{ID: event.Lease, Action: command.action}
			t.summary = "File references copied; Paste requires confirmation"
			if command.action == "move" {
				t.summary = "File references cut; navigate and Paste to confirm Move"
			}
		case "admit":
			if t.plan != nil && t.plan.proposal.Transfer == command.transfer && t.plan.phase == "admitting" {
				if !b.transferCurrent(t.plan.proposal.Epoch, t.plan.proposal.Generation) || time.Now().After(t.plan.deadline) || t.plan.cancelled {
					b.finishTransfer(false)
				} else {
					t.plan.phase = "running"
					t.plan.deadline = time.Now().Add(transferJobLimit)
					if err := b.openTransferJobs(); err != nil {
						b.finishTransfer(false)
					} else if err := b.nextTransferJob(); err != nil {
						b.finishTransfer(false)
					}
				}
			}
		}
		return b.show()
	}
	switch event.Event {
	case "ready":
		t.ready = true
		t.hostname = event.Hostname
		t.summary = "File Copy, Cut, Paste and drag enabled"
		b.transferContext()
	case "attached":
		if event.Epoch == strconv.FormatUint(b.form, 10) && event.Generation == b.nativeGeneration {
			t.epoch, t.generation = b.form, event.Generation
			if t.summary == "File transfer waiting for native Form attachment" || t.summary == "File transfer starting" {
				t.summary = "File Copy, Cut, Paste and drag enabled"
			}
			b.transferContext()
		}
	case "unavailable":
		b.transferUnavailable()
	case "lost":
		t.lease.Lost(event.Lease)
	case "result":
		if t.retireRead(event.Transfer) {
			t.summary = "File clipboard read ended without a usable proposal; no changes started"
		}
		if t.plan != nil && t.plan.proposal.Transfer == event.Transfer {
			t.plan.cancelled = true
			t.plan.cancelNotified = true
			if t.observation != nil {
				t.observation.cancel()
			}
			if t.plan.phase == "running" {
				b.cancelJob()
			} else if t.observation == nil {
				b.finishTransfer(false)
			}
		}
	case "action", "drag-begin":
		number, _ := strconv.ParseUint(event.Transfer, 10, 64)
		if number <= t.lastTransfer {
			return nil
		}
		t.lastTransfer = number
		if !b.validTransferAction(event) {
			_ = b.transferSend(filetransfer.Command{Command: "cancel", Transfer: event.Transfer}, "cancel")
			return nil
		}
		kind := event.Kind
		if event.Event == "drag-begin" {
			kind = "drag"
		}
		if err := b.fileTransferAction(kind, &event); err != nil {
			t.summary = "File transfer action refused"
			_ = b.transferSend(filetransfer.Command{Command: "cancel", Transfer: event.Transfer}, "cancel")
		}
	case "proposal":
		number, _ := strconv.ParseUint(event.Transfer, 10, 64)
		if number <= t.lastProposal {
			_ = b.transferSend(filetransfer.Command{Command: "cancel", Transfer: event.Transfer}, "cancel")
			return nil
		}
		t.lastProposal = number
		if event.Kind == "clipboard" {
			if event.ID == "" || event.ID != t.readID || event.Transfer != t.readTransfer {
				_ = b.transferSend(filetransfer.Command{Command: "cancel", Transfer: event.Transfer}, "cancel")
				return nil
			}
			t.readID, t.readTransfer = "", ""
		}
		if t.plan != nil || !b.transferCurrent(event.Epoch, event.Generation) || (b.job != nil && !b.job.reaped) {
			_ = b.transferSend(filetransfer.Command{Command: "cancel", Transfer: event.Transfer}, "cancel")
			return nil
		}
		groups, err := filetransfer.Parse(event.URIs, t.hostname)
		if err != nil {
			t.summary = "Invalid or unsupported file references"
			_ = b.transferSend(filetransfer.Command{Command: "cancel", Transfer: event.Transfer}, "cancel")
			return b.show()
		}
		// Drop URI text before retaining UI/coordinator state. Workers receive only
		// literal byte paths through private JSON pipes, as other file actions do.
		event.URIs = ""
		plan := &transferPlan{proposal: event, destination: bytes.Clone(b.snapshot.Path), destinationRoot: b.snapshot.Root, phase: "observing", deadline: time.Now().Add(transferConfirmLimit)}
		t.plan = plan
		t.summary = "Checking incoming file references; no changes started"
		b.observeTransfer(plan, groups)
	}
	return b.show()
}

func (b *browser) observeTransfer(plan *transferPlan, groups []filetransfer.Group) {
	t := b.transfer
	ctx, cancel := context.WithDeadline(context.Background(), plan.deadline)
	observation := &transferObservation{cancel: cancel, done: make(chan struct{})}
	t.observation = observation
	executable := b.executable
	go func() {
		defer close(observation.done)
		defer cancel()
		prepared := transferPrepared{plan: plan}
		// Only this worker owns prepared.plan's new groups until mailbox delivery.
		for _, group := range groups {
			result := startWorker(ctx, executable, workRequest{Operation: "locate", Targets: group.Paths, Hidden: true})
			if result.Status != "loaded" || result.Snapshot == nil || len(result.Selected) != len(group.Paths) || !bytes.Equal(result.Snapshot.Path, []byte(group.Parent)) {
				prepared.err = true
				break
			}
			prepared.plan.groups = append(prepared.plan.groups, transferGroup{snapshot: cloneJobSnapshot(result.Snapshot, result.Selected), ids: append([]string(nil), result.Selected...)})
			prepared.plan.count += len(result.Selected)
		}
		if ctx.Err() != nil {
			prepared.err = true
		}
		t.prepared <- prepared
	}()
}
func (b *browser) acceptTransferPrepared(prepared transferPrepared) error {
	t := b.transfer
	if t == nil {
		return nil
	}
	if t.observation != nil {
		<-t.observation.done
		t.observation = nil
	}
	if t.plan != prepared.plan {
		return nil
	}
	if prepared.err || t.plan.cancelled || !b.transferCurrent(t.plan.proposal.Epoch, t.plan.proposal.Generation) || time.Now().After(t.plan.deadline) {
		b.finishTransfer(false)
		return b.show()
	}
	for _, group := range t.plan.groups {
		view := &jobView{snapshot: group.snapshot, ids: group.ids, generation: b.model.Generation}
		if _, err := jobRequest(view, b.instance, 1, t.plan.proposal.Action, t.plan.destination, nil, false); err != nil {
			b.finishTransfer(false)
			return b.show()
		}
	}
	t.plan.phase = "confirming"
	operation := "Copy"
	if t.plan.proposal.Action == "move" {
		operation = "Move"
	}
	t.summary = fmt.Sprintf("Confirm %s: %d entries in %d folders to displayed directory (30 seconds)", operation, t.plan.count, len(t.plan.groups))
	return b.show()
}
func (b *browser) confirmTransfer() error {
	t := b.transfer
	if t == nil || t.plan == nil || t.plan.phase != "confirming" || !b.transferCurrent(t.plan.proposal.Epoch, t.plan.proposal.Generation) || time.Now().After(t.plan.deadline) {
		return errors.New("No current file transfer confirmation")
	}
	if b.job != nil && !b.job.reaped {
		return errors.New("Wait for the current file job")
	}
	t.plan.phase = "admitting"
	t.summary = "Confirmed; waiting for native transfer admission"
	return b.transferSend(filetransfer.Command{Command: "admit", Transfer: t.plan.proposal.Transfer}, "admit")
}
func (b *browser) cancelTransfer() {
	t := b.transfer
	if t == nil || t.plan == nil {
		return
	}
	t.plan.cancelled = true
	if !t.plan.cancelNotified {
		if b.transferSend(filetransfer.Command{Command: "cancel", Transfer: t.plan.proposal.Transfer}, "cancel") == nil {
			t.plan.cancelNotified = true
		}
	}
	if t.observation != nil {
		t.observation.cancel()
	}
	if t.plan.phase == "running" {
		b.cancelJob()
	} else if t.observation == nil {
		b.finishTransfer(false)
	}
}
func (b *browser) finishTransfer(success bool) {
	t := b.transfer
	if t == nil || t.plan == nil {
		return
	}
	plan := t.plan
	if !plan.cancelNotified {
		command := filetransfer.Command{Command: "complete", Transfer: plan.proposal.Transfer, Action: plan.proposal.Action, Success: &success}
		if plan.phase != "running" {
			command = filetransfer.Command{Command: "cancel", Transfer: plan.proposal.Transfer}
		}
		_ = b.transferSend(command, command.Command)
	}
	if t.lease.ClearAfter(plan.proposal.Lease, plan.proposal.Action, success) {
		_ = b.transferSend(filetransfer.Command{Command: "clear", Lease: plan.proposal.Lease}, "clear")
	}
	t.summary = fmt.Sprintf("File transfer stopped: %d of %d groups successful; inspect Jobs for outcomes", plan.index, len(plan.groups))
	if success {
		t.summary = fmt.Sprintf("File transfer completed: %d entries in %d folders", plan.count, len(plan.groups))
	}
	t.plan = nil
}
func (b *browser) transferTick(now time.Time) bool {
	t := b.transfer
	if t == nil {
		return false
	}
	if !t.ready && now.After(t.startup) {
		b.transferUnavailable()
		t.startup = now.Add(24 * time.Hour)
		return true
	}
	for _, command := range t.pending {
		if now.After(command.until) {
			b.transferUnavailable()
			t.pending = map[string]transferCommand{}
			return true
		}
	}
	if t.plan != nil && !t.plan.cancelled && now.After(t.plan.deadline) {
		b.cancelTransfer()
		return true
	}
	if t.plan != nil && t.plan.phase != "running" && !(t.plan.cancelled && t.observation != nil) && !b.transferCurrent(t.plan.proposal.Epoch, t.plan.proposal.Generation) {
		b.cancelTransfer()
		return true
	}
	return false
}

// Each group's request reuses existing typed file jobs and conflict handling.
// No next group can start before the previous child's terminal record AND Wait.
func (b *browser) nextTransferJob() error {
	t := b.transfer
	p := t.plan
	if p.phase != "running" || p.cancelled || (b.job != nil && !b.job.reaped) || p.index >= len(p.groups) || b.jobSerial == ^uint64(0) || !b.transferCurrent(p.proposal.Epoch, p.proposal.Generation) || !bytes.Equal(b.snapshot.Path, p.destination) || b.snapshot.Root.Device != p.destinationRoot.Device || b.snapshot.Root.Inode != p.destinationRoot.Inode {
		return errors.New("Transfer destination changed or transfer stopped")
	}
	group := p.groups[p.index]
	view := &jobView{snapshot: group.snapshot, ids: group.ids, generation: b.model.Generation}
	request, err := jobRequest(view, b.instance, b.jobSerial+1, p.proposal.Action, p.destination, nil, false)
	if err != nil {
		return err
	}
	request.DestinationIdentity = &p.destinationRoot
	// Clipboard 'move' uses exactly the existing Move request, never DELETE or
	// source-side XdndFinished deletion. File jobs own all source removal.
	b.jobSerial++
	b.job = &jobState{request: request, state: "starting", transfer: true, priorOutcomes: append([]filejobs.Event(nil), p.outcomes...)}
	if b.jobs != nil {
		b.jobs.admitted = true
		b.jobs.page = 0
		b.jobs.paintedItems = -1
	}
	b.job.process = t.launch(b.executable, request, b.jobEvents, b.jobProgress)
	t.summary = fmt.Sprintf("File transfer: group %d of %d; Jobs shows per-item results", p.index+1, len(p.groups))
	return nil
}
func fullyCompletedJob(job *jobState) bool {
	if job == nil || !job.reaped || job.cancelled || job.state != "completed" || job.terminal == nil || len(job.outcomes) != len(job.request.IDs) {
		return false
	}
	for _, event := range job.outcomes {
		if event.State != "completed" || event.Outcome == nil || !event.Outcome.ContentComplete || !event.Outcome.MetadataComplete {
			return false
		}
	}
	return true
}
func (b *browser) transferJobReaped() error {
	t := b.transfer
	if t == nil || t.plan == nil || t.plan.phase != "running" {
		return nil
	}
	p := t.plan
	p.outcomes = append(p.outcomes, b.job.outcomes...)
	success := transferGroupCompleted(p, b.job, time.Now())
	if !success || p.index == len(p.groups) {
		b.finishTransfer(success)
		b.refreshTransferDestination(p)
		return b.show()
	}
	if err := b.nextTransferJob(); err != nil {
		b.finishTransfer(false)
		b.refreshTransferDestination(p)
	}
	return b.show()
}

func (b *browser) openTransferJobs() error {
	p := b.transfer.plan
	if b.jobViewSerial == ^uint64(0) {
		return errors.New("Job dialog generation exhausted")
	}
	if err := b.retireJobs(); err != nil {
		return err
	}
	b.jobViewSerial++
	group := p.groups[0]
	b.jobs = &jobView{lifetime: b.jobViewSerial, snapshot: group.snapshot, ids: group.ids, generation: b.model.Generation, listGeneration: 1, paintedItems: -1, paintedPage: -1, admitted: true, transfer: true}
	b.jobs.alias = fmt.Sprintf("FvwmFiles-%s-jobs-%d", b.instance, b.jobs.lifetime)
	return b.renderJobs()
}

func (b *browser) transferBusy() bool { return b.transfer != nil && b.transfer.plan != nil }

// A terminal JSON record alone never advances the queue: actual child Wait,
// complete per-item outcomes and the job deadline are all admission barriers.
func transferGroupCompleted(plan *transferPlan, job *jobState, now time.Time) bool {
	if !fullyCompletedJob(job) || plan.cancelled || !now.Before(plan.deadline) {
		return false
	}
	plan.index++
	return true
}

func (b *browser) refreshTransferDestination(plan *transferPlan) {
	if b.dataReady() && bytes.Equal(b.snapshot.Path, plan.destination) {
		if err := b.navigate(b.currentPath(), "refresh"); err != nil {
			b.status = "File transfer ended; refresh to observe directory changes"
		}
	}
}

// A read conversion and an outgoing source drag can finish concurrently. The
// read-ok receipt supplies its transfer ID; a foreign result cannot clear it.
func (t *browserTransfer) acceptReadReply(event filetransfer.Event) bool {
	if event.ID != t.readID || !filetransfer.CanonicalID(event.Transfer) {
		return false
	}
	t.readTransfer = event.Transfer
	return true
}
func (t *browserTransfer) retireRead(transfer string) bool {
	if transfer == "" || transfer != t.readTransfer {
		return false
	}
	t.readID, t.readTransfer = "", ""
	return true
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

package main

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filejobs"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

const jobCancelGrace = 2 * time.Second

// Navigation owns a separate worker. A job retains this slot until its child
// has actually been waited, even after a terminal record or cancellation.
type jobProcess struct {
	controls chan filejobs.Control
	cancel   context.CancelFunc
	done     chan struct{}
}

type jobNotice struct {
	id     string
	event  *filejobs.Event
	reaped bool
	err    string
}

type jobState struct {
	request      filejobs.Request
	process      *jobProcess
	lastSequence uint64
	lastReliable uint64
	latest       filejobs.Event
	terminal     *filejobs.Event
	outcomes     []filejobs.Event
	state        string
	decision     uint64
	cancelled    bool
	reaped       bool
}

func offerJobProgress(channel chan jobNotice, notice jobNotice) {
	select {
	case channel <- notice:
	default:
		select {
		case <-channel:
		default:
		}
		select {
		case channel <- notice:
		default:
		}
	}
}

// Validate process identity and progression before a record reaches UI state.
// Progress may be coalesced; reliable item/decision/terminal records never are.
func validJobEvent(event filejobs.Event, request filejobs.Request, previous uint64) bool {
	if event.Schema != filejobs.Schema || event.ID != request.ID ||
		event.Generation != request.Generation || event.Sequence <= previous ||
		len(event.Path) > filemodel.MaxPath || len(event.Destination) > filemodel.MaxPath ||
		len(event.Detail) > 4096 || event.Item < 0 || event.Item > filejobs.MaxTargets ||
		(event.TotalKnown && event.Done > event.Total) {
		return false
	}
	switch event.Kind {
	case "accepted":
		return event.State == "accepted"
	case "progress":
		return event.State == "running"
	case "decision":
		return event.Decision != 0 && event.State == "waiting"
	case "item", "finished":
		switch event.State {
		case "completed", "skipped", "failed", "cancelled", "partial":
		case "interrupted":
			if event.Kind != "finished" {
				return false
			}
		default:
			return false
		}
		return event.Kind != "item" || (event.Outcome != nil && event.Outcome.State == event.State)
	default:
		return false
	}
}

func launchJob(executable string, request filejobs.Request,
	reliable chan<- jobNotice, progress chan jobNotice) *jobProcess {
	ctx, cancel := context.WithCancel(context.Background())
	process := &jobProcess{controls: make(chan filejobs.Control, 2), cancel: cancel, done: make(chan struct{})}
	go func() {
		defer close(process.done)
		defer cancel()
		notice := jobNotice{id: request.ID, reaped: true}
		defer func() { reliable <- notice }()
		data, err := json.Marshal(request)
		if err != nil || len(data) > filejobs.MaxRequest {
			notice.err = "File job request exceeds its limit. No job started."
			return
		}
		command := exec.CommandContext(ctx, executable, "--job-worker")
		command.WaitDelay = time.Second
		command.Stderr = io.Discard
		input, err := command.StdinPipe()
		if err != nil {
			notice.err = "Could not open the file job worker."
			return
		}
		defer input.Close()
		output, err := command.StdoutPipe()
		if err != nil {
			notice.err = "Could not read the file job worker."
			return
		}
		if err := command.Start(); err != nil {
			notice.err = "Could not start the file job worker."
			return
		}
		writerDone := make(chan struct{})
		stopWriter := make(chan struct{})
		go func() {
			defer close(writerDone)
			if _, err := input.Write(append(data, '\n')); err != nil {
				cancel()
				return
			}
			encoder := json.NewEncoder(input)
			for {
				select {
				case control := <-process.controls:
					if err := encoder.Encode(control); err != nil {
						select {
						case <-stopWriter:
						default:
							cancel()
						}
						return
					}
				case <-stopWriter:
					return
				case <-ctx.Done():
					return
				}
			}
		}()
		reader := bufio.NewScanner(output)
		reader.Buffer(make([]byte, 4096), filejobs.MaxRecord+1)
		var sequence uint64
		terminal := false
		for reader.Scan() {
			var event filejobs.Event
			if terminal || len(reader.Bytes()) > filejobs.MaxRecord ||
				decode(reader.Bytes(), &event) != nil || !validJobEvent(event, request, sequence) {
				notice.err = "File job worker returned an invalid record; outcome may be partial."
				cancel()
				break
			}
			sequence = event.Sequence
			if event.Kind == "progress" {
				offerJobProgress(progress, jobNotice{id: request.ID, event: &event})
			} else {
				reliable <- jobNotice{id: request.ID, event: &event}
			}
			if event.Kind == "finished" {
				terminal = true
				close(stopWriter)
				input.Close()
			}
		}
		if !terminal {
			input.Close()
			close(stopWriter)
			cancel()
		}
		waitErr := command.Wait()
		input.Close()
		<-writerDone
		if notice.err == "" && (reader.Err() != nil || waitErr != nil || !terminal) {
			notice.err = "File job worker stopped without a confirmed result; inspect partial changes before retrying."
		}
	}()
	return process
}

func cloneJobSnapshot(snapshot *filemodel.Snapshot, ids []string) *filemodel.Snapshot {
	copy := *snapshot
	copy.Path = bytes.Clone(snapshot.Path)
	copy.Items = nil
	selected := make(map[string]bool, len(ids))
	for _, id := range ids {
		selected[id] = true
	}
	for _, item := range snapshot.Items {
		if selected[item.ID] {
			item.Name = bytes.Clone(item.Name)
			copy.Items = append(copy.Items, item)
		}
	}
	return &copy
}

func jobRequest(view *jobView, instance string, serial uint64, operation string,
	destination, name []byte, permanent bool) (filejobs.Request, error) {
	if view == nil || view.snapshot == nil || view.generation == 0 || serial == 0 {
		return filejobs.Request{}, errors.New("Reopen Jobs from a ready folder.")
	}
	switch operation {
	case "mkdir", "rename", "copy", "move", "link", "trash", "restore", "delete":
	default:
		return filejobs.Request{}, errors.New("Unsupported file job operation.")
	}
	// Unused drafts stay in the dialog; they never become unintended fields
	// of a typed mutation request. An empty destination means captured source.
	if operation == "copy" || operation == "move" || operation == "link" {
		if len(destination) == 0 {
			destination = view.snapshot.Path
		} else if !filepath.IsAbs(string(destination)) {
			destination = []byte(filepath.Join(string(view.snapshot.Path), string(destination)))
		}
	} else {
		destination = nil
	}
	if operation == "trash" || operation == "restore" || operation == "delete" {
		name = nil
	}
	ids := append([]string(nil), view.ids...)
	if operation == "mkdir" {
		ids = nil
	} else if len(ids) == 0 || len(ids) > filejobs.MaxTargets {
		return filejobs.Request{}, errors.New("Select between one and 256 entries, then reopen Jobs.")
	}
	if operation == "rename" && len(ids) != 1 {
		return filejobs.Request{}, errors.New("Rename needs exactly one selected entry.")
	}
	if operation == "delete" && !permanent {
		return filejobs.Request{}, errors.New("Permanent deletion needs a separate confirmation.")
	}
	if (operation == "mkdir" || operation == "rename") &&
		(len(name) == 0 || len(name) > 255 || bytes.ContainsAny(name, "/\x00") || string(name) == "." || string(name) == "..") {
		return filejobs.Request{}, errors.New("Enter one file or folder name without a slash.")
	}
	if len(destination) > filemodel.MaxPath || bytes.IndexByte(destination, 0) >= 0 || len(name) > 255 {
		return filejobs.Request{}, errors.New("Destination or name exceeds its supported limit.")
	}
	request := filejobs.Request{Schema: filejobs.Schema,
		ID: fmt.Sprintf("%s-%d", instance, serial), Generation: serial,
		Operation: operation, Snapshot: cloneJobSnapshot(view.snapshot, ids), IDs: ids,
		Destination: bytes.Clone(destination), Name: bytes.Clone(name), Permanent: permanent}
	if err := filejobs.ValidateRequest(request); err != nil {
		return filejobs.Request{}, err
	}
	return request, nil
}

func (b *browser) startJob(operation string, destination, name []byte, permanent bool) error {
	if b.job != nil && !b.job.reaped {
		return errors.New("A file job is still active. Wait for it or cancel it first.")
	}
	if b.jobs != nil && b.jobs.admitted {
		return errors.New("Reopen Jobs for a fresh selection before starting another job.")
	}
	if b.jobSerial == ^uint64(0) {
		return errors.New("file job generation exhausted")
	}
	request, err := jobRequest(b.jobs, b.instance, b.jobSerial+1, operation, destination, name, permanent)
	if err != nil {
		return err
	}
	b.jobSerial++
	b.job = &jobState{request: request, state: "starting"}
	b.jobs.confirmDelete, b.jobs.admitted = false, true
	b.jobs.page, b.jobs.paintedItems = 0, -1
	b.job.process = launchJob(b.executable, request, b.jobEvents, b.jobProgress)
	return nil
}

func (b *browser) cancelJob() error {
	job := b.job
	if job == nil || job.reaped || job.cancelled || job.terminal != nil {
		return nil
	}
	control := filejobs.Control{Schema: filejobs.Schema, ID: job.request.ID, Kind: "cancel"}
	select {
	case job.process.controls <- control:
	default:
		// Cancellation still reaches the owned process through its escalation.
	}
	job.cancelled, job.state = true, "cancelling"
	process := job.process
	go func() {
		timer := time.NewTimer(jobCancelGrace)
		defer timer.Stop()
		select {
		case <-process.done:
		case <-timer.C:
			process.cancel()
		}
	}()
	return nil
}

// Shutdown never abandons a mutation child or replays an ambiguous request.
// A kernel-blocked call can delay Wait even after Kill; the slot stays owned.
func (b *browser) shutdownJob() {
	if b.job == nil || b.job.reaped || b.job.process == nil {
		return
	}
	b.cancelJob()
	for {
		select {
		case notice := <-b.jobEvents:
			if notice.id == b.job.request.ID && notice.reaped {
				<-b.job.process.done
				return
			}
		case <-b.job.process.done:
			return
		}
	}
}

func (b *browser) jobNotice(notice jobNotice) error {
	job := b.job
	if job == nil || notice.id != job.request.ID || job.reaped {
		return nil
	}
	if notice.reaped {
		job.reaped = true
		if notice.err != "" || job.terminal == nil {
			job.state = "interrupted"
			job.latest.Detail = notice.err
			if job.latest.Detail == "" {
				job.latest.Detail = "No confirmed final result; inspect partial changes before retrying."
			}
		} else {
			job.state = job.terminal.State
			job.latest = *job.terminal
		}
		job.decision = 0
		// Refresh only this still-visible source, never supersede navigation
		// or another pending intent. The mutation worker has already reaped.
		if b.snapshot != nil && bytes.Equal(b.snapshot.Path, job.request.Snapshot.Path) &&
			!b.active && b.pending == nil {
			if err := b.navigate(b.currentPath(), "refresh"); err != nil {
				b.status = "File job ended; refresh the folder to observe changes."
			}
			if err := b.show(); err != nil {
				return err
			}
		}
	} else {
		if notice.event == nil || job.terminal != nil {
			return nil
		}
		event := *notice.event
		previous := job.lastReliable
		if event.Kind == "progress" {
			previous = job.lastSequence
		}
		if !validJobEvent(event, job.request, previous) {
			return nil
		}
		// The reader checks wire order. Its separate mailboxes can deliver a
		// newer coalesced progress record before an older reliable outcome.
		// Retain reliable records once, without rolling back displayed progress.
		newest := event.Sequence > job.lastSequence
		if newest {
			job.lastSequence, job.latest = event.Sequence, event
		}
		if event.Kind != "progress" {
			job.lastReliable = event.Sequence
		}
		if event.Kind == "decision" {
			if !job.cancelled {
				job.decision, job.state = event.Decision, "decision"
			}
		} else if event.Kind == "item" {
			if len(job.outcomes) >= filejobs.MaxTargets {
				return errors.New("file job outcome limit exceeded")
			}
			job.outcomes = append(job.outcomes, event)
			job.decision = 0
			if !job.cancelled {
				job.state = "running"
			}
		} else if event.Kind == "finished" {
			job.terminal, job.state, job.decision = &event, "finishing", 0
		} else if newest && !job.cancelled && job.decision == 0 {
			job.state = event.State
		}
	}
	if b.jobs != nil {
		return b.updateJobs()
	}
	return nil
}

func (b *browser) decideJob(choice string, name []byte) error {
	job := b.job
	if job == nil || job.reaped || job.decision == 0 || job.cancelled {
		return errors.New("There is no current conflict to decide.")
	}
	if choice != "skip" && choice != "rename" && choice != "cancel" {
		return errors.New("invalid file conflict choice")
	}
	if choice == "rename" && (len(name) == 0 || len(name) > 255 || bytes.ContainsAny(name, "/\x00") || string(name) == "." || string(name) == "..") {
		return errors.New("Enter one new name without a slash.")
	}
	control := filejobs.Control{Schema: filejobs.Schema, ID: job.request.ID,
		Kind: "decide", Decision: job.decision, Choice: choice, Name: bytes.Clone(name)}
	select {
	case job.process.controls <- control:
		job.decision = 0
		job.state = "running"
		return nil
	default:
		return errors.New("A file job control is still pending; retry the decision.")
	}
}

func (b *browser) jobSummary() map[string]any {
	result := map[string]any{"active": false, "state": "idle", "queue_limit": 0,
		"retention": "latest_job"}
	if b.jobs != nil {
		result["dialog_lifetime"], result["dialog_generation"] = b.jobs.lifetime, b.jobs.listGeneration
	}
	if b.job == nil {
		return result
	}
	job := b.job
	result["id"], result["generation"], result["operation"] = job.request.ID, job.request.Generation, job.request.Operation
	result["active"], result["state"] = !job.reaped, job.state
	result["sequence"], result["decision"], result["items"] = job.lastSequence, job.decision, len(job.outcomes)
	result["done"], result["total"], result["total_known"] = job.latest.Done, job.latest.Total, job.latest.TotalKnown
	result["bytes"], result["detail"] = job.latest.Bytes, displayText(job.latest.Detail, 160)
	result["cancel_requested"] = job.cancelled
	return result
}

func jobStateText(state string) string {
	return strings.ReplaceAll(state, "-", " ")
}

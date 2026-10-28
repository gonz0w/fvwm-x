// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filejobs"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

const jobPageSize = 8

type jobView struct {
	lifetime, lastControl uint64
	alias                 string
	snapshot              *filemodel.Snapshot
	generation            uint64
	ids                   []string
	hidden                int
	confirmDelete         bool
	admitted              bool
	paintedDecision       uint64
	paintedDelete         bool
	page                  int
	listGeneration        uint32
	paintedItems          int
	paintedPage           int
	errorText             string
}

type jobCallback struct {
	lifetime, ticket  uint64
	action            string
	packet            string
	destination, name []byte
}

func parseJobCallback(message, instance string) (jobCallback, bool) {
	if len(message) > 12000 {
		return jobCallback{}, false
	}
	parts := strings.Fields(message)
	if len(parts) != 8 || parts[0] != "FVWM_FILES_JOBS_1" || parts[1] != instance {
		return jobCallback{}, false
	}
	life, ok := canonicalDecimal(parts[2])
	if !ok {
		return jobCallback{}, false
	}
	callback := jobCallback{lifetime: life, action: parts[3]}
	if parts[3] == "closed" && parts[4] == "-" && parts[5] == "-" && parts[6] == "-" && parts[7] == "-" {
		return callback, true
	}
	callback.ticket, ok = invocationTicket(parts[7])
	if !ok {
		return jobCallback{}, false
	}
	switch callback.action {
	case "mkdir", "rename", "copy", "move", "link", "trash", "restore", "delete", "confirm-delete", "skip", "new-name", "cancel", "close", "previous", "next", "open-trash":
	default:
		return jobCallback{}, false
	}
	callback.packet = parts[6]
	if len(callback.packet) > 64 {
		return jobCallback{}, false
	}
	var err error
	if callback.action == "copy" || callback.action == "move" || callback.action == "link" {
		callback.destination, err = decodeField(parts[4])
		if err != nil || len(callback.destination) > filemodel.MaxPath || bytes.IndexByte(callback.destination, 0) >= 0 {
			return jobCallback{}, false
		}
	}
	if callback.action == "copy" || callback.action == "move" || callback.action == "link" ||
		callback.action == "mkdir" || callback.action == "rename" || callback.action == "new-name" {
		callback.name, err = decodeField(parts[5])
		if err != nil || len(callback.name) > 255 || bytes.IndexByte(callback.name, 0) >= 0 {
			return jobCallback{}, false
		}
	}
	return callback, true
}

func (b *browser) routeJobCallback(message string) {
	callback, ok := parseJobCallback(message, b.instance)
	if !ok {
		return
	}
	select {
	case b.jobCallbacks <- callback:
	default:
		// Bounded local controls may be retried; no operation is admitted here.
	}
}

func (b *browser) routeJobReply(message string) bool {
	if strings.HasPrefix(message, "FVWM_FILES_JOBS_SYNC "+b.instance+" ") {
		if b.jobAckReplies != nil {
			b.jobAckReplies.offer(message, message)
		}
		return true
	}
	fields := strings.Fields(message)
	if len(fields) >= 2 && fields[0] == "FVWM_FORM_LIST" && strings.HasPrefix(fields[1], b.instance+"_j") {
		if b.jobListReplies != nil {
			b.jobListReplies.offer(fields[1], message)
		}
		return true
	}
	return false
}

func (b *browser) sendJobCommands(commands []string) error {
	if b.jobSendSerial == ^uint64(0) {
		return errors.New("job view send generation exhausted")
	}
	b.jobSendSerial++
	return b.sendCommandBatch(commands, "FVWM_FILES_JOBS_SYNC", b.jobSendSerial,
		nil, b.jobAckReplies, time.Now().Add(workerLimit))
}

func (b *browser) waitJobList(token string, generation uint32, send func() error) error {
	replies := b.jobListReplies.begin(token)
	defer b.jobListReplies.retire(token)
	if err := send(); err != nil {
		return err
	}
	timer := time.NewTimer(workerLimit)
	defer timer.Stop()
	select {
	case reply := <-replies:
		fields := strings.Fields(reply)
		if len(fields) != 6 || fields[0] != "FVWM_FORM_LIST" || fields[1] != token ||
			fields[2] != strconv.FormatUint(uint64(generation), 10) || fields[3] != "0" || fields[4] != "0" {
			return errors.New("file job results publication refused")
		}
		if _, err := nativePacket(fields[5], generation); err != nil {
			return errors.New("file job results publication invalid")
		}
		return nil
	case <-b.session.Done():
		return errors.New("file job view connection closed")
	case <-timer.C:
		return errors.New("file job view publication timed out")
	}
}

func (b *browser) retireJobs() error {
	if b.jobs == nil {
		return nil
	}
	alias := b.jobs.alias
	b.jobs = nil
	if alias == "" {
		return nil
	}
	return b.sendJobCommands([]string{"KillModule FvwmForm " + alias,
		"DestroyModuleConfig " + alias + ":*"})
}

func (b *browser) openJobs() error {
	if b.job != nil && !b.job.reaped {
		if b.jobs == nil {
			return errors.New("A file job is cancelling; wait for its worker before reopening Jobs.")
		}
		return b.sendJobCommands([]string{"All (" + b.jobs.alias + ") Iconify false",
			"All (" + b.jobs.alias + ") Focus", "All (" + b.jobs.alias + ") Raise"})
	}
	if b.snapshot == nil || !b.dataReady() {
		return errors.New("Wait for a ready folder before opening Jobs.")
	}
	if b.jobViewSerial == ^uint64(0) {
		return errors.New("file job view lifetime exhausted")
	}
	if err := b.retireJobs(); err != nil {
		return err
	}
	b.jobViewSerial++
	ids := b.model.SelectedIDs()
	b.jobs = &jobView{lifetime: b.jobViewSerial, snapshot: cloneJobSnapshot(b.snapshot, ids),
		generation: b.model.Generation, ids: append([]string(nil), ids...),
		listGeneration: 1, paintedItems: -1, paintedPage: -1}
	visible := make(map[string]bool)
	for _, item := range b.model.VisibleEntries() {
		visible[item.ID] = true
	}
	for _, id := range ids {
		if !visible[id] {
			b.jobs.hidden++
		}
	}
	b.jobs.alias = fmt.Sprintf("FvwmFiles-%s-jobs-%d", b.instance, b.jobs.lifetime)
	return b.renderJobs()
}

func (b *browser) renderJobs() error {
	view := b.jobs
	alias := view.alias
	callback := fmt.Sprintf("Echo FVWM_FILES_JOBS_1 %s %d ", b.instance, view.lifetime)
	commands := []string{}
	line := func(value string) { commands = append(commands, "*"+alias+": "+value) }
	button := func(label, key, action string) {
		destination, name := "x", "x"
		if action == "copy" || action == "move" || action == "link" {
			destination = "x$(destination:Base64)"
		}
		if action == "copy" || action == "move" || action == "link" ||
			action == "mkdir" || action == "rename" || action == "new-name" {
			name = "x$(name:Base64)"
		}
		line(`Button continue "` + label + `" ` + key)
		line("Command " + callback + action + " " + destination + " " + name + " x$(results:Base64) $(FormJob:Token)")
	}
	label := func(name, value string) {
		line(`Label ` + name + ` 56 "` + displayText(value, 360) + `"`)
		line("LabelViewport " + name)
	}
	line("UIScale monitor")
	line(`Title "File jobs"`)
	line("Line left")
	label("job-source", "Source: "+string(view.snapshot.Path))
	line("Line left")
	label("job-selection", fmt.Sprintf("Captured: %d selected; %d hidden by filter. Reopen F9 to capture changes.", len(view.ids), view.hidden))
	line("Line left")
	label("job-status", "Ready. Choose an operation for the captured selection.")
	line("Line left")
	label("job-progress", "No job running.")
	line("Line left")
	label("job-decision", "Existing destinations are never overwritten.")
	line("Line left")
	line(`Text "Destination folder (empty: source):"`)
	line(`Input destination 40 ""`)
	line("Line left")
	line(`Text "Name (new/rename or one output):"`)
	line(`Input name 32 ""`)
	line("Line left")
	button("New folder (F1)", "F1", "mkdir")
	button("Rename (F2)", "F2", "rename")
	button("Copy (F3)", "F3", "copy")
	button("Move (F4)", "F4", "move")
	line("Line left")
	button("Link (F5)", "F5", "link")
	button("Trash (F6)", "F6", "trash")
	button("Restore (F7)", "F7", "restore")
	button("Delete (F8)", "F8", "delete")
	line("Line left")
	line("List results 56 4")
	line("ListViewport 18 2")
	line("ListVersioned")
	line(`ListItem off "No completed items."`)
	token := fmt.Sprintf("%s_j%d_start", b.instance, view.lifetime)
	line("ListNotify " + token)
	line("Line left")
	button("Earlier results (Alt-P)", "Alt-P", "previous")
	button("Later results (Alt-N)", "Alt-N", "next")
	button("Open trash (Alt-T)", "Alt-T", "open-trash")
	line("Line left")
	button("Skip conflict (F11)", "F11", "skip")
	button("Use new name (F12)", "F12", "new-name")
	line("Line left")
	button("Confirm permanent delete (F10)", "F10", "confirm-delete")
	line("Line left")
	button("Cancel job (Alt-X)", "Alt-X", "cancel")
	button("Close (Esc)", "^[", "close")
	line("Line left")
	line(`Text "Delete: F8, then F10 confirms permanent removal. No undo."`)
	line("Line left")
	line(`Text "Close cancels active work; partial results can remain."`)
	line("CloseCommand " + callback + "closed - - - -")
	line(`Timeout 3000 "KillModule FvwmForm ` + alias + `" ""`)
	commands = append(commands, "Module FvwmForm "+alias)
	if err := b.waitJobList(token, 1, func() error { return b.sendJobCommands(commands) }); err != nil {
		return err
	}
	return b.updateJobs()
}

// Confirmation presents the captured IDs' names, including filtered entries.
// List selection is presentation only and never supplies mutation targets.
func jobResultRows(view *jobView, job *jobState) []string {
	rows := []string{}
	if view.confirmDelete {
		names := make(map[string][]byte, len(view.snapshot.Items))
		for _, item := range view.snapshot.Items {
			names[item.ID] = item.Name
		}
		for _, id := range view.ids {
			rows = append(rows, displayText("Delete target: "+string(names[id]), 140))
		}
		return rows
	}
	if job == nil {
		return rows
	}
	for _, event := range job.outcomes {
		value := event.State + ": " + filepath.Base(string(event.Path))
		if event.Outcome != nil {
			value = event.Outcome.State + ": " + filepath.Base(string(event.Path))
			if event.Outcome.ContentComplete && !event.Outcome.MetadataComplete {
				value += " (metadata incomplete)"
			} else if event.Outcome.DestinationPresent && !event.Outcome.ContentComplete {
				value += " (content incomplete)"
			}
			if event.Outcome.SourceRemaining && event.Outcome.DestinationPresent {
				value += " (source and destination remain)"
			}
		}
		rows = append(rows, displayText(value, 140))
	}
	return rows
}

func jobRowsPage(rows []string, page int) ([]string, int) {
	start := page * jobPageSize
	if page < 0 || start >= len(rows) {
		start, page = 0, 0
	}
	end := start + jobPageSize
	if end > len(rows) {
		end = len(rows)
	}
	return rows[start:end], page
}

func (b *browser) armJobDelete() error {
	view := b.jobs
	if view == nil || view.admitted {
		return errors.New("Reopen Jobs for a fresh selection before deleting.")
	}
	if b.job != nil && !b.job.reaped {
		return errors.New("Wait for the active file job before deleting.")
	}
	if len(view.ids) == 0 {
		return errors.New("Select entries before opening Jobs to delete them.")
	}
	view.confirmDelete, view.page = true, 0
	return nil
}

func (b *browser) updateJobs() error {
	view := b.jobs
	if view == nil {
		return nil
	}
	status, progress, decision := "Ready. Choose an operation for the captured selection.", "No job running.", "Existing destinations are never overwritten."
	rows := jobResultRows(view, b.job)
	count := len(rows)
	if job := b.job; job != nil {
		status = fmt.Sprintf("%s: %s", job.request.Operation, jobStateText(job.state))
		if job.latest.Detail != "" {
			status += ". " + job.latest.Detail
		}
		if job.latest.TotalKnown {
			progress = fmt.Sprintf("Progress: %d/%d; %d bytes copied.", job.latest.Done, job.latest.Total, job.latest.Bytes)
		} else {
			progress = fmt.Sprintf("Progress: %d items; total not known; %d bytes copied.", job.latest.Done, job.latest.Bytes)
		}
		if job.decision != 0 {
			decision = "Conflict: Skip, enter a new Name then F12, or cancel."
		} else if job.cancelled && !job.reaped {
			decision = "Cancellation requested; waiting for the worker to stop."
		}
	}
	if view.confirmDelete {
		decision = fmt.Sprintf("PERMANENT DELETE targets below: %d captured (%d hidden). F10 confirms; Esc or another operation cancels. Alt-P/N pages.", len(view.ids), view.hidden)
	}
	if view.errorText != "" {
		status = view.errorText
	}
	commands := []string{}
	for _, label := range []nativeLabel{{"job-status", status}, {"job-progress", progress}, {"job-decision", decision}} {
		commands = append(commands, "SendToModule "+view.alias+" SetLabel "+label.name+` "`+displayText(label.value, 360)+`"`)
	}
	commands = append(commands, "SendToModule "+view.alias+" RenewTimeout 3000")
	currentDecision := uint64(0)
	if b.job != nil {
		currentDecision = b.job.decision
	}
	if view.paintedItems != count || view.paintedPage != view.page ||
		view.paintedDecision != currentDecision || view.paintedDelete != view.confirmDelete {
		if view.listGeneration == ^uint32(0) {
			return errors.New("file job result generation exhausted")
		}
		next := view.listGeneration + 1
		commands = append(commands, fmt.Sprintf("SendToModule %s ListBegin results %d %d", view.alias, view.listGeneration, next))
		pageRows, page := jobRowsPage(rows, view.page)
		view.page = page
		if count == 0 {
			commands = append(commands, fmt.Sprintf(`SendToModule %s ListEntry results %d 0 "No completed items." other off`, view.alias, next))
		} else {
			for _, value := range pageRows {
				commands = append(commands, fmt.Sprintf(`SendToModule %s ListEntry results %d 0 "%s" other off`, view.alias, next, value))
			}
		}
		commands = append(commands, fmt.Sprintf("SendToModule %s ListCommit results %d", view.alias, next))
		view.listGeneration = next
		view.paintedItems, view.paintedPage = count, view.page
		view.paintedDecision, view.paintedDelete = currentDecision, view.confirmDelete
	}
	token := fmt.Sprintf("%s_j%d_%d", b.instance, view.lifetime, b.jobSendSerial+1)
	commands = append(commands, "SendToModule "+view.alias+" ListStatus results "+token+" selection")
	return b.waitJobList(token, view.listGeneration, func() error { return b.sendJobCommands(commands) })
}

func (b *browser) jobAction(callback jobCallback) (bool, error) {
	view := b.jobs
	if view == nil || callback.lifetime != view.lifetime {
		return false, nil
	}
	if callback.action == "closed" {
		b.cancelJob()
		return true, b.retireJobs()
	}
	if callback.ticket == 0 || callback.ticket <= view.lastControl {
		return false, nil
	}
	if callback.action != "close" && callback.action != "cancel" {
		if _, err := nativePacket(callback.packet, view.listGeneration); err != nil {
			return false, nil
		}
	}
	view.lastControl = callback.ticket
	view.errorText = ""
	if callback.action != "confirm-delete" && callback.action != "previous" && callback.action != "next" {
		view.confirmDelete = false
	}
	var err error
	switch callback.action {
	case "close":
		b.cancelJob()
		return true, b.retireJobs()
	case "cancel":
		err = b.cancelJob()
	case "skip":
		err = b.decideJob("skip", nil)
	case "new-name":
		err = b.decideJob("rename", callback.name)
	case "open-trash":
		if b.job != nil && !b.job.reaped {
			err = errors.New("Wait for the active file job before opening Trash.")
		} else {
			var path string
			path, err = filejobs.TrashFilesPath(b.home, os.Getenv("XDG_DATA_HOME"))
			if err == nil {
				err = b.navigate(path, "push")
			}
			if err == nil {
				if err = b.retireJobs(); err != nil {
					return true, err
				}
				return true, b.show()
			}
		}
	case "previous":
		if view.page > 0 {
			view.page--
		}
	case "next":
		if (view.page+1)*jobPageSize < len(jobResultRows(view, b.job)) {
			view.page++
		}
	case "delete":
		err = b.armJobDelete()
	case "confirm-delete":
		if !view.confirmDelete {
			err = errors.New("Choose Delete (F8) before confirming permanent removal.")
		} else {
			err = b.startJob("delete", nil, nil, true)
		}
	case "mkdir", "rename", "copy", "move", "link", "trash", "restore":
		err = b.startJob(callback.action, callback.destination, callback.name, false)
	default:
		return false, nil
	}
	if err != nil {
		view.errorText = err.Error()
	}
	return true, b.updateJobs()
}

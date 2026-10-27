// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import "path/filepath"

type providerWork struct {
	ID        string `json:"id"`
	Operation string `json:"operation"`
	Status    string `json:"status"`
	Detail    string `json:"detail,omitempty"`
	serial    uint64
}

// Admission is metadata-only. The existing owned worker revalidates paths and
// item identities immediately before enumeration or a literal process handoff.
func (b *browser) planProviderWork(operation string, action providerAction) (workRequest, string, string) {
	request := workRequest{Snapshot: b.snapshot, Hidden: b.hidden}
	navigation := "push"
	switch operation {
	case "reveal", "inspect":
		request.Operation, request.Targets = "locate", action.targets
		if operation == "inspect" {
			navigation = "properties"
		}
	case "hidden":
		request.Operation, request.Path, request.Hidden = "scan", b.snapshot.Path, action.enabled
		navigation = "refresh"
	case "refresh":
		request.Operation, request.Path = "scan", b.snapshot.Path
		navigation = "refresh"
	case "navigate":
		request.Operation = "scan"
		switch action.mode {
		case "home":
			request.Path = []byte(b.home)
		case "up":
			request.Path = []byte(filepath.Dir(string(b.snapshot.Path)))
		case "back":
			if len(b.history) == 0 {
				return request, "", "no_history"
			}
			request.Path, navigation = []byte(b.history[len(b.history)-1]), "back"
		case "path":
			request.Path = action.path
		case "item":
			request.Operation, request.ID = "browse", action.id
		}
	case "open":
		request.Operation, request.ID, request.Program = "activate", action.id, b.opener
	case "terminal":
		if b.terminal == "" {
			return request, "", "application_unavailable"
		}
		request.Operation, request.Program = "terminal", b.terminal
	case "custom":
		if action.index >= len(b.actions) {
			return request, "", "action_unavailable"
		}
		request.Operation, request.Program, request.IDs = "custom", b.actions[action.index].Program, b.model.SelectedIDs()
		if len(request.IDs) == 0 {
			return request, "", "selection_unavailable"
		}
	}
	if request.ID != "" {
		found := false
		for _, item := range b.snapshot.Items {
			if item.ID == request.ID {
				found = true
				if operation == "navigate" && item.Kind != "directory" {
					return request, "", "not_directory"
				}
				if operation == "open" && item.Kind != "directory" && b.opener == "" {
					return request, "", "application_unavailable"
				}
				break
			}
		}
		if !found {
			return request, "", "invalid_target"
		}
	}
	return request, navigation, ""
}

func (b *browser) startProviderWork(query fileQuery, action providerAction) string {
	if b.active || b.pending != nil {
		return "busy"
	}
	request, navigation, code := b.planProviderWork(query.Operation, action)
	if code != "" {
		return code
	}
	if b.queue(request, navigation) != nil {
		return "operation_refused"
	}
	b.providerWork = &providerWork{ID: query.Serial, Operation: query.Operation, Status: "accepted", serial: b.serial}
	return ""
}

func (b *browser) cancelProviderWork(id string) string {
	work := b.providerWork
	if work == nil || work.ID != id {
		return "operation_unavailable"
	}
	if work.Status != "accepted" || work.serial != b.serial || !b.active {
		return "not_cancellable"
	}
	if b.serial == ^uint64(0) {
		return "operation_refused"
	}
	// Kill is best effort. Retain the worker slot until Wait, even after cancel.
	if b.invalidateIntent() != nil {
		return "operation_refused"
	}
	b.serial++
	b.cancel()
	work.Status = "cancelling"
	b.status = "Cancellation requested; waiting for current work to stop"
	return ""
}

func (b *browser) finishProviderWork(event completedWork) {
	work := b.providerWork
	if work == nil || work.serial != event.work.serial {
		return
	}
	switch {
	case work.Status == "cancelling":
		work.Status = "cancelled"
		b.status = "File work cancelled; any application handoff cannot be undone"
	case event.work.serial != b.serial:
		work.Status = "superseded"
	case event.result.Status == "loaded" && event.result.Snapshot != nil:
		work.Status = "loaded"
	case event.result.Status == "started":
		work.Status = "started"
	case event.result.Status == "cancelled":
		work.Status = "cancelled"
	default:
		work.Status = "failed"
		work.Detail = displayText(event.result.Error, 160)
	}
	// started is only a process handoff; cancellation cannot undo that handoff.
}

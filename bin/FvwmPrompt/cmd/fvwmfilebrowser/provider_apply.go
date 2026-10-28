// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

func (b *browser) applyProviderAction(query fileQuery, action providerAction) string {
	if b.snapshot == nil || b.snapshot.Schema != 1 {
		return "not_ready"
	}
	if query.Generation != b.providerGeneration() {
		return "stale_generation"
	}
	if query.Operation == "cancel" {
		return b.cancelProviderWork(action.id)
	}
	if providerWorkerAction(query.Operation) {
		if query.Operation != "navigate" && query.Operation != "refresh" && query.Operation != "hidden" && query.Operation != "reveal" && query.Operation != "inspect" && query.Operation != "directory-roles" && !b.dataReady() {
			return "not_ready"
		}
		return b.startProviderWork(query, action)
	}
	// Collection policy publishes new slices/maps only after full admission.
	// A refused candidate cannot change live selection, order or projection.
	candidate := b.model
	switch query.Operation {
	case "selection":
		if err := candidate.EditSelection(candidate.Generation, action.mode, action.ids); err != nil {
			return "invalid_selection"
		}
		b.model, b.replaceSelection, b.inspected = candidate, true, nil
	case "filter":
		if err := candidate.SetFilter(action.text); err != nil {
			return "operation_refused"
		}
		b.model = candidate
	case "sort":
		if action.key != b.ordering() || action.reverse != b.reverse {
			entries, err := b.snapshot.OrderedEntries(action.key, action.reverse)
			if err != nil || candidate.Replace(entries) != nil {
				return "operation_refused"
			}
			b.model, b.order, b.reverse = candidate, action.key, action.reverse
		}
	case "view":
		b.view = action.mode
	case "present":
		// Publication and managed-window commands are acknowledged by the caller.
	case "directory-roles-close":
		if b.retireDirectoryRoles() != nil {
			return "operation_refused"
		}
	case "watch":
		if b.watch == nil {
			b.watch = &directoryWatch{events: make(chan watchEvent, 1)}
		}
		b.watch.enabled = action.enabled
		b.watch.desire(b.snapshot, true)
	case "properties":
		ids := candidate.SelectedIDs()
		if _, err := b.snapshot.ObservedSelection(ids); err != nil {
			return "selection_unavailable"
		}
		b.inspected = append([]string(nil), ids...)
	default:
		return "invalid_request"
	}
	return ""
}

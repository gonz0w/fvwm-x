// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"errors"
	"fmt"
	"strconv"

	"github.com/fvwmorg/FvwmPrompt/internal/viewbinding"
)

// These are the browser's only two bindings. They own no application values:
// the existing worker slot, pending intent and admitted collection keep ownership.
func (b *browser) ensureBindings() error {
	if b.intentBinding != nil {
		if b.intentBinding.State() == viewbinding.Closed {
			return errors.New("browser binding is closed")
		}
		return nil
	}
	intent, err := viewbinding.New("intent")
	if err != nil {
		return err
	}
	projection, err := viewbinding.New("model")
	if err != nil {
		intent.Close()
		return err
	}
	b.intentBinding, b.projectionBinding = intent, projection
	return nil
}

func (b *browser) bindingProducer() string {
	if b.instance == "" {
		return "browser"
	}
	return b.instance
}

func (b *browser) observeIntent(serial uint64) error {
	if err := b.ensureBindings(); err != nil {
		return err
	}
	return b.intentBinding.Observe("intent", viewbinding.Version{
		Producer: b.bindingProducer(), Revision: strconv.FormatUint(serial, 10)})
}

func (b *browser) invalidateIntent() error {
	if b.intentBinding == nil {
		return nil
	}
	return b.intentBinding.Invalidate()
}

// Connection loss and destruction retire both lifetimes. A replacement process
// registers a fresh provider and full view; no ticket or uncertain action replays.
func (b *browser) closeBindings() {
	if b.intentBinding != nil {
		b.intentBinding.Close()
		b.projectionBinding.Close()
	}
	b.pending = nil
}

func (b *browser) dataReady() bool {
	if b.snapshot == nil || b.snapshot.Schema != 1 || b.active || b.pending != nil {
		return false
	}
	return b.intentBinding == nil || b.intentBinding.State() == viewbinding.Current
}

type browserProgress struct {
	State string `json:"state"`
	Known bool   `json:"known"`
	Count int    `json:"completed"`
	Total *int   `json:"total"`
}

func (b *browser) progress() browserProgress {
	state := "waiting"
	if b.intentBinding != nil {
		state = string(b.intentBinding.State())
	}
	if b.active {
		return browserProgress{State: "loading"}
	}
	if b.dataReady() {
		count := len(b.model.Entries)
		return browserProgress{State: "current", Known: true, Count: count, Total: &count}
	}
	return browserProgress{State: state}
}

func (b *browser) progressText() string {
	p := b.progress()
	if p.Known {
		return fmt.Sprintf("Progress: complete; %d entries loaded.", p.Count)
	}
	if b.active {
		return "Progress: checking or enumerating; total unknown."
	}
	return "Progress: " + p.State + "; Refresh for a current snapshot."
}

// Availability is observed metadata for this model/provider generation, never
// permission to bypass backend identity/type checks immediately before an effect.
type browserAvailability struct {
	Ready      bool `json:"ready"`
	Open       int  `json:"open_visible"`
	Properties bool `json:"properties"`
	Terminal   bool `json:"terminal"`
	Custom     bool `json:"custom"`
	Cancel     bool `json:"cancel"`
}

func (b *browser) availability() browserAvailability {
	a := browserAvailability{Ready: b.dataReady(), Cancel: b.active}
	if !a.Ready {
		return a
	}
	for _, entry := range b.model.VisibleEntries() {
		if entry.Kind == "directory" || entry.Kind == "file" && b.opener != "" {
			a.Open++
		}
	}
	a.Terminal = b.terminal != ""
	ids := b.model.SelectedIDs()
	if len(ids) != 0 {
		_, err := b.snapshot.ObservedSelection(ids)
		a.Properties = err == nil
		// The worker additionally rejects symlinks/special files and revalidates
		// current filesystem identities. No filesystem work runs in this query.
		a.Custom = len(b.actions) != 0 && a.Properties
		for _, item := range b.snapshot.Items {
			if b.model.Selected[item.ID] && item.Kind != "file" && item.Kind != "directory" {
				a.Custom = false
			}
		}
	}
	return a
}

func (b *browser) readinessText() string {
	a := b.availability()
	if !a.Ready {
		return "Actions: unavailable; Refresh after work stops."
	}
	yesNo := func(value bool) string {
		if value {
			return "yes"
		}
		return "no"
	}
	return fmt.Sprintf("Open %d; Properties %s; Terminal %s; Custom %s",
		a.Open, yesNo(a.Properties), yesNo(a.Terminal), yesNo(a.Custom))
}

// Diagnostics are an opt-in state query, never a refresh or authority input.
// Report the actual event-loop ticker lifetime, not the desired watch state.
func (b *browser) bindingStats() map[string]any {
	result := map[string]any{"intent": viewbinding.Stats{State: viewbinding.Waiting},
		"projection":         viewbinding.Stats{State: viewbinding.Waiting},
		"watch_timer_active": b.watchTimerActive}
	if b.intentBinding != nil {
		result["intent"] = b.intentBinding.Stats()
		result["projection"] = b.projectionBinding.Stats()
	}
	return result
}

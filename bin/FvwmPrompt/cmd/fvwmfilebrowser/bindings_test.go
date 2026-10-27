// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"io"
	"net"
	"reflect"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/internal/viewbinding"
	"github.com/fvwmorg/FvwmPrompt/transport"
)

// Admit an intent without executing a child. Snapshot-policy tests still use
// the real ticket retirement path; existing worker tests cover child ownership.
func boundWork(t *testing.T, b *browser) pendingWork {
	t.Helper()
	if err := b.observeIntent(b.serial + 1); err != nil {
		t.Fatal(err)
	}
	b.serial++
	ticket, ok := b.intentBinding.Begin()
	if !ok {
		t.Fatal("intent did not begin")
	}
	b.active, b.activeSerial = true, b.serial
	b.cancel = func() {}
	return pendingWork{serial: b.serial, ticket: ticket}
}

func completeBoundSnapshot(t *testing.T, b *browser, result workResult) error {
	t.Helper()
	return b.completed(completedWork{work: boundWork(t, b), result: result})
}

func boundBrowser(t *testing.T) browser {
	t.Helper()
	b, _ := providerFixture(t)
	left, right := net.Pipe()
	t.Cleanup(func() { left.Close(); right.Close() })
	go io.Copy(io.Discard, right)
	b.session = transport.New(left)
	return b
}

func TestBrowserBindingSameContentInvalidationAndProgress(t *testing.T) {
	b := boundBrowser(t)
	generation, snapshot := b.model.Generation, b.snapshot
	work := boundWork(t, &b)
	if p := b.progress(); p.Known || p.Total != nil || p.Count != 0 || !strings.Contains(b.progressText(), "unknown") || b.availability().Ready {
		t.Fatal("enumeration advertised invented progress/readiness", p)
	}
	if err := b.completed(completedWork{work: work, result: workResult{Status: "loaded", Snapshot: snapshot}}); err != nil {
		t.Fatal(err)
	}
	before := b.providerGeneration()
	if p := b.progress(); !p.Known || p.Total == nil || *p.Total != len(b.model.Entries) || p.Count != *p.Total || !b.availability().Ready {
		t.Fatal("completed count missing", p)
	}
	if err := completeBoundSnapshot(t, &b, workResult{Status: "loaded", Snapshot: snapshot}); err != nil {
		t.Fatal(err)
	}
	if b.model.Generation != generation || b.providerGeneration() == before || b.intentBinding.State() != viewbinding.Current || b.projectionBinding.State() != viewbinding.Current {
		t.Fatal("same-content refresh failed to commit without replacing collection")
	}
}

func TestBrowserBindingFailureCancelAndLossRetainData(t *testing.T) {
	for _, mode := range []string{"failed", "cancelled", "lost"} {
		t.Run(mode, func(t *testing.T) {
			b := boundBrowser(t)
			snapshot, generation := b.snapshot, b.model.Generation
			work := boundWork(t, &b)
			result := workResult{Status: "loaded", Snapshot: &filemodel.Snapshot{Schema: 1, Path: []byte("/late")}}
			switch mode {
			case "failed":
				result = workResult{Status: "failed", Error: "could not enumerate"}
			case "cancelled":
				if err := b.invalidateIntent(); err != nil {
					t.Fatal(err)
				}
				b.serial++
			case "lost":
				b.closeBindings()
			}
			if err := b.completed(completedWork{work: work, result: result}); err != nil {
				t.Fatal(err)
			}
			if b.active || b.snapshot != snapshot || b.model.Generation != generation || b.availability().Ready || b.progress().Known {
				t.Fatal("late/failed work replaced data or enabled actions")
			}
			if mode == "lost" {
				if b.alias != "" || b.ensureBindings() == nil {
					t.Fatal("dead view reopened")
				}
				fresh := boundBrowser(t)
				fresh.instance = strings.Repeat("b", 32)
				freshWork := boundWork(t, &fresh)
				if fresh.intentBinding.Complete(work.ticket, true) || !fresh.intentBinding.InFlight() {
					t.Fatal("old ticket retired fresh provider work")
				}
				if err := fresh.completed(completedWork{work: freshWork, result: workResult{Status: "loaded", Snapshot: fresh.snapshot}}); err != nil || !fresh.availability().Ready {
					t.Fatal("fresh snapshot failed", err)
				}
				return
			}
			if err := b.queue(workRequest{Operation: "terminal", Snapshot: b.snapshot, Program: "/unused"}, "action"); err == nil {
				t.Fatal("stale data admitted file action")
			}
			if err := completeBoundSnapshot(t, &b, workResult{Status: "loaded", Snapshot: snapshot}); err != nil || !b.availability().Ready {
				t.Fatal("explicit refresh failed", err)
			}
		})
	}
}

func TestBrowserBindingBurstKeepsLatestAndRejectsDuplicate(t *testing.T) {
	b := boundBrowser(t)
	work := boundWork(t, &b)
	intent, projection := b.intentBinding, b.projectionBinding
	for i := 0; i < 10000; i++ {
		if err := b.navigate("/newest", "refresh"); err != nil {
			t.Fatal(err)
		}
		if b.pending == nil || b.pending.serial != b.serial || !b.intentBinding.Pending() || !b.intentBinding.InFlight() || b.intentBinding != intent || b.projectionBinding != projection {
			t.Fatal("fixed owner/latest pending bound lost")
		}
	}
	if err := b.invalidateIntent(); err != nil {
		t.Fatal(err)
	}
	b.serial++
	b.pending = nil
	if err := b.completed(completedWork{work: work, result: workResult{Status: "loaded", Snapshot: b.snapshot}}); err != nil {
		t.Fatal(err)
	}
	if b.active || b.availability().Ready || b.intentBinding.InFlight() {
		t.Fatal("invalidated child did not retire")
	}
	current := boundWork(t, &b)
	if err := b.completed(completedWork{work: work, result: workResult{Status: "failed"}}); err != nil || !b.active || !b.intentBinding.InFlight() {
		t.Fatal("old completion released current slot", err)
	}
	if err := b.completed(completedWork{work: current, result: workResult{Status: "loaded", Snapshot: b.snapshot}}); err != nil {
		t.Fatal(err)
	}
}

func TestBrowserAvailabilityGenerationAndBackendRevalidation(t *testing.T) {
	b := boundBrowser(t)
	if err := completeBoundSnapshot(t, &b, workResult{Status: "loaded", Snapshot: b.snapshot}); err != nil {
		t.Fatal(err)
	}
	q := fileQuery{Version: 1, Provider: b.instance, Serial: "0000000000000001", Scope: "state", Limit: 1}
	result, code := b.providerResult(q)
	if code != "" || result["availability_generation"] != b.providerGeneration() || result["availability"].(browserAvailability).Open != 0 {
		t.Fatal(result, code)
	}
	before := b.providerGeneration()
	b.opener, b.terminal = "/configured", "/configured"
	b.actions = actionFlags{{Label: `hostile $[exec.touch] " &`, Program: "/configured"}}
	if b.providerGeneration() == before || b.availability().Open != 12 || !b.availability().Terminal {
		t.Fatal("program readiness ignored generation")
	}
	b.model.SelectIDs(b.model.Generation, []string{b.snapshot.Items[0].ID})
	if !b.availability().Custom || !b.availability().Properties {
		t.Fatal("observed selection not ready")
	}
	before = b.providerGeneration()
	if err := b.model.SetFilter("missing"); err != nil {
		t.Fatal(err)
	}
	if b.availability().Open != 0 || b.providerGeneration() == before {
		t.Fatal("visible availability ignored generation")
	}
	request, _, code := b.planProviderWork("custom", providerAction{})
	if code != "" || perform(request).Status != "failed" {
		t.Fatal("availability bypassed worker identity checks", code)
	}
	b.closeBindings()
	if b.availability().Ready || b.projectionBinding.State() != viewbinding.Closed {
		t.Fatal("destroyed view remained ready")
	}
}

func TestWatchSamplerOnlyWhileActiveOrRestartable(t *testing.T) {
	var absent *directoryWatch
	if absent.needsTick() || (&directoryWatch{}).needsTick() || (&directoryWatch{failed: true, desired: &watchRequest{}}).needsTick() {
		t.Fatal("inactive watch wakes idle browser")
	}
	if !(&directoryWatch{desired: &watchRequest{}}).needsTick() || !(&directoryWatch{active: true}).needsTick() || !(&directoryWatch{active: true, failed: true}).needsTick() {
		t.Fatal("live/cancelled observer lost sampler before Wait")
	}
}

func TestBrowserNativePublicationLossClosesBothBindings(t *testing.T) {
	b := boundBrowser(t)
	work := boundWork(t, &b)
	b.session.Close()
	if err := b.completed(completedWork{work: work, result: workResult{Status: "loaded", Snapshot: b.snapshot}}); err == nil {
		t.Fatal("native connection loss was accepted as a publication")
	}
	if b.intentBinding.State() != viewbinding.Closed || b.projectionBinding.State() != viewbinding.Closed || b.availability().Ready || b.pending != nil {
		t.Fatal("native publication loss left live work/actions")
	}
	if err := b.show(); err == nil {
		t.Fatal("failed native lifetime reopened")
	}
}

func TestBrowserBindingDiagnosticsReadsAreStable(t *testing.T) {
	b := boundBrowser(t)
	if err := completeBoundSnapshot(t, &b, workResult{Status: "loaded", Snapshot: b.snapshot}); err != nil {
		t.Fatal(err)
	}
	query := fileQuery{Version: 1, Provider: b.instance, Serial: "0000000000000001", Scope: "state", Limit: 1}
	before := b.bindingStats()
	generation := b.providerGeneration()
	for i := 0; i < 100; i++ {
		result, code := b.providerResult(query)
		if code != "" || !reflect.DeepEqual(result["bindings"], before) || result["generation"] != generation {
			t.Fatal("read-only diagnostics mutated work/counters/generation", result, code)
		}
	}
	// Desired/active observer state is not evidence that its loop owns a ticker.
	b.watch = &directoryWatch{active: true}
	if b.bindingStats()["watch_timer_active"] != false {
		t.Fatal("watch timer metadata inferred ownership from watch state")
	}
	b.watchTimerActive = true
	if b.bindingStats()["watch_timer_active"] != true || b.providerGeneration() != generation {
		t.Fatal("timer diagnostics lost actual ownership or changed authority")
	}
}

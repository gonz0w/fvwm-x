// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package viewbinding

import (
	"encoding/json"
	"reflect"
	"strings"
	"testing"
)

func mustBinding(t *testing.T, sources ...string) *Binding {
	t.Helper()
	b, err := New(sources...)
	if err != nil {
		t.Fatal(err)
	}
	return b
}

func observe(t *testing.T, b *Binding, source, revision string) {
	t.Helper()
	if err := b.Observe(source, Version{"producer", revision}); err != nil {
		t.Fatal(err)
	}
}

func begin(t *testing.T, b *Binding) Ticket {
	t.Helper()
	ticket, ok := b.Begin()
	if !ok {
		t.Fatalf("Begin refused in %s", b.State())
	}
	return ticket
}

func check(t *testing.T, b *Binding, state State, pending, active bool) {
	t.Helper()
	if b.State() != state || b.Pending() != pending || b.InFlight() != active {
		t.Fatalf("got (%s,%t,%t), want (%s,%t,%t)", b.State(),
			b.Pending(), b.InFlight(), state, pending, active)
	}
}

func TestCommittedSources(t *testing.T) {
	b := mustBinding(t, "settings", "provider")
	check(t, b, Waiting, false, false)
	observe(t, b, "settings", "9")
	check(t, b, Waiting, true, false)
	if _, ok := b.Begin(); ok {
		t.Fatal("began before all sources were available")
	}
	observe(t, b, "provider", "1")
	check(t, b, Dirty, true, false)
	ticket := begin(t, b)
	check(t, b, Loading, false, true)
	if _, ok := b.Begin(); ok {
		t.Fatal("overlapping work admitted")
	}
	if !b.Complete(ticket, true) {
		t.Fatal("current successful completion refused")
	}
	check(t, b, Current, false, false)
	observe(t, b, "settings", "9")
	check(t, b, Current, false, false)
	// Revisions are opaque, so an apparently older revision is a change.
	observe(t, b, "settings", "2")
	check(t, b, Dirty, true, false)
}

func TestCoalescingRetiresOnlyItsOwnWork(t *testing.T) {
	b := mustBinding(t, "source")
	observe(t, b, "source", "first")
	old := begin(t, b)
	for i := 0; i < 10000; i++ {
		if err := b.Invalidate(); err != nil {
			t.Fatal(err)
		}
	}
	check(t, b, Dirty, true, true)
	if b.Complete(old, true) {
		t.Fatal("invalidated work published")
	}
	check(t, b, Dirty, true, false)
	latest := begin(t, b)
	if b.Complete(old, true) || b.Complete(Ticket{}, true) {
		t.Fatal("old or fabricated completion published")
	}
	check(t, b, Loading, false, true)
	if !b.Complete(latest, true) || b.Complete(latest, true) {
		t.Fatal("current completion must publish exactly once")
	}
}

func TestFailureWaitsForExplicitRetry(t *testing.T) {
	b := mustBinding(t, "source")
	observe(t, b, "source", "1")
	if b.Complete(begin(t, b), false) {
		t.Fatal("failed work published")
	}
	check(t, b, Failed, false, false)
	observe(t, b, "source", "1")
	if _, ok := b.Begin(); ok {
		t.Fatal("failure automatically retried")
	}
	if err := b.Invalidate(); err != nil {
		t.Fatal(err)
	}
	active := begin(t, b)
	observe(t, b, "source", "2")
	if b.Complete(active, false) {
		t.Fatal("failed stale work published")
	}
	check(t, b, Dirty, true, false)
	if !b.Complete(begin(t, b), true) {
		t.Fatal("latest revision did not recover")
	}
}

func TestLossAndReconnectEpoch(t *testing.T) {
	b := mustBinding(t, "a", "b")
	observe(t, b, "a", "1")
	observe(t, b, "b", "1")
	old := begin(t, b)
	if err := b.Lost("a"); err != nil {
		t.Fatal(err)
	}
	check(t, b, Unavailable, true, true)
	before := *b
	if err := b.Lost("a"); err != nil || !reflect.DeepEqual(*b, before) {
		t.Fatal("repeated loss did not coalesce")
	}
	if b.Complete(old, true) {
		t.Fatal("unavailable producer published")
	}
	check(t, b, Unavailable, true, false)
	observe(t, b, "b", "2")
	check(t, b, Unavailable, true, false)
	observe(t, b, "a", "1")
	if b.Complete(old, true) {
		t.Fatal("pre-loss work survived reconnect with equal version")
	}
	if !b.Complete(begin(t, b), true) {
		t.Fatal("reconnected work failed")
	}
	if err := b.Observe("b", Version{"replacement", "2"}); err != nil {
		t.Fatal(err)
	}
	check(t, b, Dirty, true, false)
}

func TestForeignLifetimeAndClosure(t *testing.T) {
	a, b := mustBinding(t, "a"), mustBinding(t, "a")
	observe(t, a, "a", "1")
	observe(t, b, "a", "1")
	old, current := begin(t, a), begin(t, b)
	if b.Complete(old, true) {
		t.Fatal("ticket crossed lifetime")
	}
	check(t, b, Loading, false, true)
	a.Close()
	a.Close()
	check(t, a, Closed, false, false)
	if a.Complete(old, true) || a.Invalidate() == nil || a.Lost("a") == nil ||
		a.Observe("a", Version{"p", "r"}) == nil {
		t.Fatal("closed binding reopened")
	}
	if _, ok := a.Begin(); ok {
		t.Fatal("closed binding admitted work")
	}
	if !b.Complete(current, true) {
		t.Fatal("foreign ticket cleared current work")
	}
	var zero Binding
	check(t, &zero, Closed, false, false)
}

func TestInvalidArgumentsAreAtomic(t *testing.T) {
	for _, sources := range [][]string{
		nil, {""}, {"same", "same"}, {"with space"}, {"a/b"},
		{"\x00"}, {"é"}, {strings.Repeat("a", 65)},
		{"a", "b", "c", "d", "e", "f", "g", "h", "i"},
	} {
		if b, err := New(sources...); err == nil || b != nil {
			t.Fatalf("New(%q) accepted invalid arguments", sources)
		}
	}
	b := mustBinding(t, "source")
	observe(t, b, "source", "1")
	begin(t, b)
	before := *b
	for _, version := range []Version{
		{}, {"p", ""}, {"", "r"}, {"p\n", "r"}, {"p", "r r"},
		{"é", "r"}, {"p", strings.Repeat("r", 129)},
	} {
		if err := b.Observe("source", version); err == nil {
			t.Fatalf("accepted invalid version %#v", version)
		}
	}
	if b.Observe("unknown", Version{"p", "r"}) == nil || b.Lost("unknown") == nil {
		t.Fatal("accepted unknown source")
	}
	if !reflect.DeepEqual(*b, before) {
		t.Fatal("invalid arguments mutated binding")
	}
}

func TestBoundsAndEpochExhaustion(t *testing.T) {
	b := mustBinding(t, strings.Repeat("a", 64), "b", "c", "d", "e", "f", "g", "h")
	for i := 0; i < b.count; i++ {
		if err := b.Observe(b.sources[i].name, Version{
			strings.Repeat("p", 128), strings.Repeat("r", 128),
		}); err != nil {
			t.Fatal(err)
		}
	}
	if !b.Complete(begin(t, b), true) {
		t.Fatal("maximum admitted metadata failed to publish")
	}
	for _, trigger := range []struct {
		name string
		run  func(*Binding) error
	}{
		{"invalidate", (*Binding).Invalidate},
		{"observe", func(b *Binding) error {
			return b.Observe("source", Version{"new", "revision"})
		}},
		{"lost", func(b *Binding) error { return b.Lost("source") }},
	} {
		t.Run(trigger.name, func(t *testing.T) {
			b := mustBinding(t, "source")
			observe(t, b, "source", "1")
			old := begin(t, b)
			b.epoch = ^uint64(0)
			before := *b
			if b.Observe("source", Version{}) == nil || !reflect.DeepEqual(*b, before) {
				t.Fatal("invalid input changed an exhausted binding")
			}
			if err := trigger.run(b); err == nil {
				t.Fatal("epoch exhaustion was not explicit")
			}
			check(t, b, Closed, false, false)
			if b.Complete(old, true) {
				t.Fatal("exhausted lifetime published")
			}
		})
	}
}

func TestReconnectWhileOldWorkRemainsActive(t *testing.T) {
	b := mustBinding(t, "source")
	observe(t, b, "source", "1")
	old := begin(t, b)
	if err := b.Lost("source"); err != nil {
		t.Fatal(err)
	}
	observe(t, b, "source", "1")
	check(t, b, Dirty, true, true)
	if b.Complete(old, true) {
		t.Fatal("equal committed identity reused a pre-loss ticket")
	}
	if !b.Complete(begin(t, b), true) {
		t.Fatal("reconnected latest refresh did not publish")
	}
}

func TestStatsTrackCausalActivityAndIdleReads(t *testing.T) {
	b := mustBinding(t, "source")
	if got := b.Stats(); got != (Stats{State: Waiting, Sources: 1, IdentityBytes: 6}) {
		t.Fatalf("initial stats: %#v", got)
	}
	observe(t, b, "source", "1")
	old := begin(t, b)
	observe(t, b, "source", "1")
	if _, ok := b.Begin(); ok {
		t.Fatal("overlapping work began")
	}
	if err := b.Invalidate(); err != nil {
		t.Fatal(err)
	}
	b.Complete(Ticket{}, true)
	b.Complete(old, true)
	current := begin(t, b)
	if !b.Complete(current, true) {
		t.Fatal("current work did not publish")
	}
	b.Complete(current, true)
	want := Stats{State: Current, Sources: 1, IdentityBytes: 15,
		Invalidations: 2, Started: 2, Published: 1, Rejected: 3}
	before := *b
	for i := 0; i < 1000; i++ {
		if got := b.Stats(); got != want {
			t.Fatalf("idle stats: got %#v, want %#v", got, want)
		}
	}
	if !reflect.DeepEqual(*b, before) {
		t.Fatal("idle observation changed policy")
	}
	encoded, err := json.Marshal(b.Stats())
	if err != nil || string(encoded) != `{"state":"current","sources":1,"identity_bytes":15,"in_flight":false,"pending":false,"invalidations":2,"started":2,"published":1,"rejected":3}` {
		t.Fatalf("diagnostic JSON: %s, %v", encoded, err)
	}
	if err := b.Lost("source"); err != nil {
		t.Fatal(err)
	}
	if got := b.Stats(); got.State != Unavailable || got.IdentityBytes != 6 ||
		got.Invalidations != 3 || !got.Pending {
		t.Fatalf("loss stats: %#v", got)
	}
	b.Close()
	if got := b.Stats(); got != (Stats{State: Closed}) {
		t.Fatalf("closed stats: %#v", got)
	}
}

func TestDiagnosticCountersSaturateWithoutBlockingWork(t *testing.T) {
	b := mustBinding(t, "source")
	b.invalidations, b.started, b.published, b.rejected =
		^uint64(0), ^uint64(0), ^uint64(0), ^uint64(0)
	observe(t, b, "source", "1")
	ticket := begin(t, b)
	b.Complete(Ticket{}, true)
	if !b.Complete(ticket, true) {
		t.Fatal("diagnostic saturation blocked publication")
	}
	got := b.Stats()
	if got.State != Current || got.Invalidations != ^uint64(0) ||
		got.Started != ^uint64(0) || got.Published != ^uint64(0) ||
		got.Rejected != ^uint64(0) {
		t.Fatalf("saturated stats: %#v", got)
	}
}

func TestStatsReportsMaximumRetainedIdentityBytes(t *testing.T) {
	sources := make([]string, 8)
	for i := range sources {
		sources[i] = strings.Repeat("s", 63) + string(byte('a'+i))
	}
	b := mustBinding(t, sources...)
	for _, source := range sources {
		if err := b.Observe(source, Version{
			strings.Repeat("p", 128), strings.Repeat("r", 128),
		}); err != nil {
			t.Fatal(err)
		}
	}
	if got := b.Stats(); got.Sources != 8 || got.IdentityBytes != 2560 {
		t.Fatalf("maximum retained metadata: %#v", got)
	}
}

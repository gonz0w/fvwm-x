// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package viewbinding tracks committed producer-source to native-view refreshes.
// Owners serialize calls and perform all work themselves. Sources are fixed
// domain identifiers, never derived views: this is not an expression graph.
package viewbinding

import (
	"errors"
	"strings"
)

// Version is an opaque committed identity, compared only for equality. Its two
// components must each contain 1..128 printable ASCII bytes without spaces.
type Version struct{ Producer, Revision string }

type identity struct{ marker byte }

// Ticket belongs to exactly one binding lifetime and one admitted refresh.
type Ticket struct {
	owner *identity
	epoch uint64
}

type State string

const (
	Waiting     State = "waiting"
	Dirty       State = "dirty"
	Loading     State = "loading"
	Current     State = "current"
	Unavailable State = "unavailable"
	Failed      State = "failed"
	Closed      State = "closed"
)

// Stats is an opt-in diagnostic snapshot. Counters saturate, never wrap or
// control refresh admission. Close resets the lifetime and its counters.
type Stats struct {
	State         State  `json:"state"`
	Sources       uint64 `json:"sources"`
	IdentityBytes uint64 `json:"identity_bytes"`
	InFlight      bool   `json:"in_flight"`
	Pending       bool   `json:"pending"`
	Invalidations uint64 `json:"invalidations"`
	Started       uint64 `json:"started"`
	Published     uint64 `json:"published"`
	Rejected      uint64 `json:"rejected"`
}

type source struct {
	name      string
	version   Version
	available bool
	lost      bool
}

// Binding retains bounded identity metadata, one active ticket and one pending
// bit. It never retains application data. Its zero value is closed; use New.
// A Binding must not be copied after construction.
type Binding struct {
	owner   *identity
	sources [8]source
	count   int
	epoch   uint64
	active  Ticket
	pending bool
	failed  bool

	invalidations, started, published, rejected uint64
}

// New accepts 1..8 unique source IDs of 1..64 ASCII letters, digits, _, ., : or
// -. Callers select fixed producer sources; view outputs cannot be sources.
func New(sources ...string) (*Binding, error) {
	if len(sources) == 0 || len(sources) > 8 {
		return nil, errors.New("binding requires 1..8 sources")
	}
	for i, name := range sources {
		if !validToken(name, 64, true) {
			return nil, errors.New("invalid binding source ID")
		}
		for _, prior := range sources[:i] {
			if prior == name {
				return nil, errors.New("duplicate binding source ID")
			}
		}
	}
	b := &Binding{owner: &identity{}, count: len(sources)}
	for i, name := range sources {
		b.sources[i].name = strings.Clone(name)
	}
	return b, nil
}

func validToken(value string, limit int, sourceID bool) bool {
	if len(value) == 0 || len(value) > limit {
		return false
	}
	for i := 0; i < len(value); i++ {
		c := value[i]
		if c < '!' || c > '~' || sourceID && !(c >= 'a' && c <= 'z' ||
			c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' ||
			c == '_' || c == '.' || c == ':' || c == '-') {
			return false
		}
	}
	return true
}

func (b *Binding) sourceIndex(name string) (int, error) {
	if b.owner == nil {
		return 0, errors.New("binding is closed")
	}
	for i := 0; i < b.count; i++ {
		if b.sources[i].name == name {
			return i, nil
		}
	}
	return 0, errors.New("unknown binding source")
}

// Invalidate requests an explicit resnapshot, coalescing pending work while
// making any active ticket stale. Exhaustion closes the lifetime before wrap.
func (b *Binding) Invalidate() error {
	if b.owner == nil {
		return errors.New("binding is closed")
	}
	if b.epoch == ^uint64(0) {
		b.Close()
		return errors.New("binding epoch exhausted")
	}
	b.epoch++
	increment(&b.invalidations)
	b.pending, b.failed = true, false
	return nil
}

func (b *Binding) Observe(name string, version Version) error {
	index, err := b.sourceIndex(name)
	if err != nil {
		return err
	}
	if !validToken(version.Producer, 128, false) ||
		!validToken(version.Revision, 128, false) {
		return errors.New("invalid committed binding version")
	}
	s := &b.sources[index]
	if s.available && s.version == version {
		return nil
	}
	if err := b.Invalidate(); err != nil {
		return err
	}
	s.version = Version{strings.Clone(version.Producer), strings.Clone(version.Revision)}
	s.available, s.lost = true, false
	return nil
}

func (b *Binding) Lost(name string) error {
	index, err := b.sourceIndex(name)
	if err != nil {
		return err
	}
	if b.sources[index].lost {
		return nil
	}
	if err := b.Invalidate(); err != nil {
		return err
	}
	b.sources[index].version = Version{}
	b.sources[index].available, b.sources[index].lost = false, true
	return nil
}

func (b *Binding) Begin() (Ticket, bool) {
	if b.State() != Dirty || b.InFlight() {
		return Ticket{}, false
	}
	b.active = Ticket{b.owner, b.epoch}
	b.pending = false
	increment(&b.started)
	return b.active, true
}

// Complete always retires its matching active slot, even when stale. Foreign or
// duplicate tickets cannot clear another refresh. Only current success publishes.
func (b *Binding) Complete(ticket Ticket, success bool) bool {
	if !b.InFlight() || ticket != b.active {
		increment(&b.rejected)
		return false
	}
	b.active = Ticket{}
	if ticket.epoch != b.epoch {
		increment(&b.rejected)
		return false
	}
	b.failed = !success
	if success && b.State() == Current {
		increment(&b.published)
		return true
	}
	increment(&b.rejected)
	return false
}

func (b *Binding) Close() { *b = Binding{} }

func (b *Binding) State() State {
	if b.owner == nil {
		return Closed
	}
	missing := false
	for i := 0; i < b.count; i++ {
		if b.sources[i].lost {
			return Unavailable
		}
		missing = missing || !b.sources[i].available
	}
	if missing {
		return Waiting
	}
	if b.pending {
		return Dirty
	}
	if b.InFlight() {
		return Loading
	}
	if b.failed {
		return Failed
	}
	return Current
}

func (b *Binding) Pending() bool  { return b.pending }
func (b *Binding) InFlight() bool { return b.active.owner != nil }

func increment(counter *uint64) {
	if *counter != ^uint64(0) {
		*counter += 1
	}
}

// Stats reads only bounded metadata and performs no scheduling or I/O.
func (b *Binding) Stats() Stats {
	stats := Stats{State: b.State(), Sources: uint64(b.count),
		InFlight: b.InFlight(), Pending: b.Pending(),
		Invalidations: b.invalidations, Started: b.started,
		Published: b.published, Rejected: b.rejected}
	for i := 0; i < b.count; i++ {
		s := &b.sources[i]
		stats.IdentityBytes += uint64(len(s.name) +
			len(s.version.Producer) + len(s.version.Revision))
	}
	return stats
}

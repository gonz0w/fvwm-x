// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package collection

import (
	"errors"
	"slices"
)

const MaxChanges = 256

// Delta changes metadata only. Order is the complete resulting ID order, never
// a partial projection. The caller's generation scopes every ID in the change.
type Delta struct {
	Remove []string
	Upsert []Entry
	Order  []string
}

type Changes struct {
	Added, Updated, Removed int
	SelectionLost           int
	Replaced                bool
}

// Apply validates a bounded transaction completely before publishing anything.
// A remove/upsert conflict is refused rather than depending on operation order.
func (m *Model) Apply(generation uint64, delta Delta) (Changes, error) {
	if generation != m.Generation || len(delta.Remove) > MaxChanges ||
		len(delta.Upsert) > MaxChanges-len(delta.Remove) || len(delta.Order) > MaxItems {
		return Changes{}, errors.New("stale or over-limit collection update")
	}
	if _, err := validateEntries(delta.Upsert); err != nil {
		return Changes{}, err
	}
	next := make(map[string]Entry, len(m.Entries)+len(delta.Upsert))
	for _, entry := range m.Entries {
		next[entry.ID] = entry
	}
	removed := make(map[string]bool, len(delta.Remove))
	for _, id := range delta.Remove {
		if id == "" || len(id) > 64 {
			return Changes{}, errors.New("invalid removed identity")
		}
		if _, exists := next[id]; !exists || removed[id] {
			return Changes{}, errors.New("unknown or duplicate removed identity")
		}
		removed[id] = true
		delete(next, id)
	}
	for _, entry := range delta.Upsert {
		if removed[entry.ID] {
			return Changes{}, errors.New("conflicting collection update")
		}
		next[entry.ID] = entry
	}
	if len(next) > MaxItems || len(next) != len(delta.Order) {
		return Changes{}, errors.New("incomplete collection order")
	}
	entries := make([]Entry, 0, len(next))
	for _, id := range delta.Order {
		if id == "" || len(id) > 64 {
			return Changes{}, errors.New("invalid ordered identity")
		}
		entry, exists := next[id]
		if !exists {
			return Changes{}, errors.New("unknown or duplicate ordered identity")
		}
		entries = append(entries, entry)
		delete(next, id)
	}
	return m.reconcile(entries, false)
}

// Reconcile adapts a full admitted snapshot to the same atomic update policy.
// It uses a small transaction or one bounded replacement for larger changes.
func (m *Model) Reconcile(generation uint64, entries []Entry) (Changes, error) {
	if generation != m.Generation {
		return Changes{}, errors.New("stale collection snapshot")
	}
	index, err := validateEntries(entries)
	if err != nil {
		return Changes{}, err
	}
	old := make(map[string]Entry, len(m.Entries))
	delta := Delta{Order: make([]string, 0, len(entries))}
	for _, entry := range m.Entries {
		old[entry.ID] = entry
		if _, exists := index[entry.ID]; !exists {
			delta.Remove = append(delta.Remove, entry.ID)
		}
	}
	for _, entry := range entries {
		delta.Order = append(delta.Order, entry.ID)
		if previous, exists := old[entry.ID]; !exists || previous != entry {
			delta.Upsert = append(delta.Upsert, entry)
		}
	}
	if len(delta.Remove)+len(delta.Upsert) > MaxChanges {
		return m.reconcile(entries, true)
	}
	return m.Apply(generation, delta)
}

func (m *Model) reconcile(entries []Entry, replaced bool) (Changes, error) {
	index, err := validateEntries(entries)
	if err != nil {
		return Changes{}, err
	}
	if slices.Equal(m.Entries, entries) {
		return Changes{}, nil
	}
	old := make(map[string]Entry, len(m.Entries))
	for _, entry := range m.Entries {
		old[entry.ID] = entry
	}
	result := Changes{Replaced: replaced}
	for _, entry := range entries {
		if previous, exists := old[entry.ID]; !exists {
			result.Added++
		} else if previous != entry {
			result.Updated++
		}
	}
	for _, entry := range m.Entries {
		if _, exists := index[entry.ID]; !exists {
			result.Removed++
			if m.Selected[entry.ID] {
				result.SelectionLost++
			}
		}
	}
	if err := m.publish(entries, index); err != nil {
		return Changes{}, err
	}
	return result, nil
}

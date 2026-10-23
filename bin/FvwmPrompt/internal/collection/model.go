// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package collection owns bounded metadata/selection, never domain payloads.
package collection

import (
	"errors"
	"fmt"
)

const MaxItems = 4096
const MaxSelected = 256
const PageSize = 12

type Entry struct {
	ID    string `json:"id"`
	Label string `json:"label"`
	Kind  string `json:"kind"`
}

type Model struct {
	Generation uint64
	Entries    []Entry
	Selected   map[string]bool
	Focus      string
	index      map[string]int
}

// Replace validates before publication. IDs come from the domain owner, not
// labels/positions. A refresh retains selection/focus only for surviving IDs.
func (m *Model) Replace(entries []Entry) error {
	if len(entries) > MaxItems || m.Generation == ^uint64(0) {
		return errors.New("collection limit reached")
	}
	index := make(map[string]int, len(entries))
	for i, entry := range entries {
		if entry.ID == "" || len(entry.ID) > 64 || len(entry.Label) > 512 || len(entry.Kind) > 32 {
			return errors.New("invalid collection entry")
		}
		if _, exists := index[entry.ID]; exists {
			return errors.New("duplicate collection identity")
		}
		index[entry.ID] = i
	}
	selected := make(map[string]bool)
	for id := range m.Selected {
		if _, exists := index[id]; exists {
			selected[id] = true
		}
	}
	m.Entries = append([]Entry(nil), entries...)
	m.index, m.Selected = index, selected
	m.Generation++
	if _, exists := index[m.Focus]; !exists {
		m.Focus = ""
		if len(entries) != 0 {
			m.Focus = entries[0].ID
		}
	}
	return nil
}

func (m *Model) Page(offset int) ([]Entry, error) {
	if offset < 0 || offset%PageSize != 0 || (offset >= len(m.Entries) && offset != 0) {
		return nil, errors.New("stale collection page")
	}
	end := offset + PageSize
	if end > len(m.Entries) {
		end = len(m.Entries)
	}
	return append([]Entry(nil), m.Entries[offset:end]...), nil
}

// SelectPage replaces only the visible page's selection. Reject duplicate,
// stale and off-page controls atomically; selection cannot grow past its cap.
func (m *Model) SelectPage(generation uint64, offset int, ids []string) error {
	if generation != m.Generation {
		return errors.New("stale collection generation")
	}
	page, err := m.Page(offset)
	if err != nil {
		return err
	}
	allowed := make(map[string]bool, len(page))
	selected := make(map[string]bool, len(m.Selected))
	for id := range m.Selected {
		selected[id] = true
	}
	for _, entry := range page {
		allowed[entry.ID] = true
		delete(selected, entry.ID)
	}
	for _, id := range ids {
		if !allowed[id] || selected[id] {
			return errors.New("invalid page selection")
		}
		selected[id] = true
	}
	if len(selected) > MaxSelected {
		return fmt.Errorf("select at most %d items", MaxSelected)
	}
	m.Selected = selected
	return nil
}

func (m *Model) SelectedIDs() []string {
	ids := make([]string, 0, len(m.Selected))
	for _, entry := range m.Entries {
		if m.Selected[entry.ID] {
			ids = append(ids, entry.ID)
		}
	}
	return ids
}

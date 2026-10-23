// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package collection owns bounded metadata/selection, never domain payloads.
package collection

import (
	"errors"
	"fmt"
	"strings"
	"unicode/utf8"
)

const MaxItems = 4096
const MaxSelected = 256
const PageSize = 12
const MaxFilter = 128

type Entry struct {
	ID       string `json:"id"`
	Label    string `json:"label"`
	Kind     string `json:"kind"`
	Revision string `json:"revision,omitempty"`
}

type Model struct {
	Generation uint64
	Entries    []Entry
	Selected   map[string]bool
	Focus      string
	filter     string
	visible    []int
}

// Projections contain metadata only. A hidden selection remains owned until
// explicitly cleared or its domain ID disappears on replacement.
func projection(entries []Entry, filter string) []int {
	query := strings.ToLower(filter)
	visible := make([]int, 0, len(entries))
	for i, entry := range entries {
		if strings.Contains(strings.ToLower(entry.Label), query) {
			visible = append(visible, i)
		}
	}
	return visible
}

func (m *Model) repairFocus() {
	for _, i := range m.visible {
		if m.Entries[i].ID == m.Focus {
			return
		}
	}
	m.Focus = ""
	if len(m.visible) != 0 {
		m.Focus = m.Entries[m.visible[0]].ID
	}
}

func (m *Model) Filter() string    { return m.filter }
func (m *Model) VisibleCount() int { return len(m.visible) }

// SetFilter publishes a complete bounded literal-label projection. Changing a
// projection invalidates its controls even when the domain snapshot is intact.
func (m *Model) SetFilter(filter string) error {
	if len(filter) > MaxFilter || !utf8.ValidString(filter) || strings.ContainsRune(filter, 0) {
		return errors.New("filter must be valid text of at most 128 bytes")
	}
	if filter == m.filter {
		return nil
	}
	if m.Generation == ^uint64(0) {
		return errors.New("collection generation exhausted")
	}
	m.visible = projection(m.Entries, filter)
	m.filter = filter
	m.Generation++
	m.repairFocus()
	return nil
}

// OffsetFor keeps a surviving visible anchor on its page after sort/refresh.
func (m *Model) OffsetFor(id string) int {
	for position, i := range m.visible {
		if m.Entries[i].ID == id {
			return position / PageSize * PageSize
		}
	}
	return 0
}

func (m *Model) HiddenSelectedCount() int {
	count := len(m.Selected)
	for _, i := range m.visible {
		if m.Selected[m.Entries[i].ID] {
			count--
		}
	}
	return count
}

// Replace validates before publication. IDs come from the domain owner, not
// labels/positions. A refresh retains selection/focus only for surviving IDs.
func (m *Model) Replace(entries []Entry) error {
	if len(entries) > MaxItems || m.Generation == ^uint64(0) {
		return errors.New("collection limit reached")
	}
	index, err := validateEntries(entries)
	if err != nil {
		return err
	}
	return m.publish(entries, index)
}

func validateEntries(entries []Entry) (map[string]int, error) {
	if len(entries) > MaxItems {
		return nil, errors.New("collection limit reached")
	}
	index := make(map[string]int, len(entries))
	for i, entry := range entries {
		if entry.ID == "" || len(entry.ID) > 64 || len(entry.Label) > 512 || len(entry.Kind) > 32 || len(entry.Revision) > 64 {
			return nil, errors.New("invalid collection entry")
		}
		if _, exists := index[entry.ID]; exists {
			return nil, errors.New("duplicate collection identity")
		}
		index[entry.ID] = i
	}
	return index, nil
}

// publish owns all new slices/maps. Prefer a surviving old visible neighbor if
// focus disappears; insertion or sort never changes a surviving focused ID.
func (m *Model) publish(entries []Entry, index map[string]int) error {
	if m.Generation == ^uint64(0) {
		return errors.New("collection generation exhausted")
	}
	visible := projection(entries, m.filter)
	visibleIDs := make(map[string]bool, len(visible))
	for _, i := range visible {
		visibleIDs[entries[i].ID] = true
	}
	focus := m.Focus
	if !visibleIDs[focus] {
		focus = ""
		position := -1
		for i, old := range m.visible {
			if m.Entries[old].ID == m.Focus {
				position = i
				break
			}
		}
		if position >= 0 {
			for _, old := range m.visible[position+1:] {
				if visibleIDs[m.Entries[old].ID] {
					focus = m.Entries[old].ID
					break
				}
			}
			for i := position - 1; focus == "" && i >= 0; i-- {
				if visibleIDs[m.Entries[m.visible[i]].ID] {
					focus = m.Entries[m.visible[i]].ID
				}
			}
		}
		if focus == "" && len(visible) != 0 {
			focus = entries[visible[0]].ID
		}
	}
	selected := make(map[string]bool)
	for id := range m.Selected {
		if _, exists := index[id]; exists {
			selected[id] = true
		}
	}
	m.Entries = append([]Entry(nil), entries...)
	m.Selected = selected
	m.visible = visible
	m.Focus = focus
	m.Generation++
	return nil
}

func (m *Model) Page(offset int) ([]Entry, error) {
	if offset < 0 || offset%PageSize != 0 || (offset >= len(m.visible) && offset != 0) {
		return nil, errors.New("stale collection page")
	}
	end := offset + PageSize
	if end > len(m.visible) {
		end = len(m.visible)
	}
	page := make([]Entry, 0, end-offset)
	for _, i := range m.visible[offset:end] {
		page = append(page, m.Entries[i])
	}
	return page, nil
}

// SelectIDs admits an adapter's complete domain selection independently of
// paging. Native page callbacks continue using the narrower SelectPage policy.
func (m *Model) SelectIDs(generation uint64, ids []string) error {
	if generation != m.Generation || len(ids) > MaxSelected {
		return errors.New("stale or over-limit domain selection")
	}
	known := make(map[string]bool, len(m.Entries))
	for _, entry := range m.Entries {
		known[entry.ID] = true
	}
	selected := make(map[string]bool, len(ids))
	for _, id := range ids {
		if !known[id] || selected[id] {
			return errors.New("unknown or duplicate domain selection")
		}
		selected[id] = true
	}
	m.Selected = selected
	return nil
}

func (m *Model) ClearSelection(generation uint64) error {
	if generation != m.Generation {
		return errors.New("stale collection generation")
	}
	m.Selected = make(map[string]bool)
	return nil
}

func (m *Model) SelectWholePage(generation uint64, offset int) error {
	page, err := m.Page(offset)
	if err != nil {
		return err
	}
	ids := make([]string, 0, len(page))
	for _, entry := range page {
		ids = append(ids, entry.ID)
	}
	return m.SelectPage(generation, offset, ids)
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

// VisibleEntries copies the current ordered projection. Native row indices
// belong only to this immutable generation; they are never external item IDs.
func (m *Model) VisibleEntries() []Entry {
	entries := make([]Entry, 0, len(m.visible))
	for _, i := range m.visible {
		entries = append(entries, m.Entries[i])
	}
	return entries
}

// SelectNative admits a complete viewport packet atomically, preserving items
// hidden by the filter. Its maximum base64 representation is 686 bytes.
func (m *Model) SelectNative(generation uint64, packet []byte) error {
	count := len(m.visible)
	if generation != m.Generation || len(packet) != 2+(count+7)/8 {
		return errors.New("stale or malformed native selection")
	}
	focus := int(packet[0])<<8 | int(packet[1])
	if (count == 0 && focus != 0) || (count > 0 && focus >= count) {
		return errors.New("invalid native focus")
	}
	if count%8 != 0 && packet[len(packet)-1]>>uint(count%8) != 0 {
		return errors.New("invalid native selection padding")
	}
	selected := make(map[string]bool, len(m.Selected))
	for id := range m.Selected {
		selected[id] = true
	}
	for row, index := range m.visible {
		id := m.Entries[index].ID
		delete(selected, id)
		if packet[2+row/8]&(1<<uint(row%8)) != 0 {
			selected[id] = true
		}
	}
	if len(selected) > MaxSelected {
		return errors.New("native selection exceeds limit")
	}
	m.Selected = selected
	if count != 0 {
		m.Focus = m.Entries[m.visible[focus]].ID
	}
	return nil
}

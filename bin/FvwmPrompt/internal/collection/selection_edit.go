// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package collection

import "errors"

// EditSelection validates a complete edit before publishing. Hidden selections
// remain owned, and removing an unknown ID is refused just like adding one.
func (m *Model) EditSelection(generation uint64, mode string, ids []string) error {
	if generation != m.Generation || len(ids) > MaxSelected {
		return errors.New("stale or over-limit selection edit")
	}
	if mode == "clear" {
		if len(ids) != 0 {
			return errors.New("clear does not accept IDs")
		}
		return m.ClearSelection(generation)
	}
	if mode != "replace" && mode != "add" && mode != "remove" {
		return errors.New("unknown selection edit")
	}
	known := make(map[string]bool, len(m.Entries))
	for _, entry := range m.Entries {
		known[entry.ID] = true
	}
	seen := make(map[string]bool, len(ids))
	for _, id := range ids {
		if !known[id] || seen[id] {
			return errors.New("unknown or duplicate selection ID")
		}
		seen[id] = true
	}
	selected := make(map[string]bool, len(m.Selected)+len(ids))
	if mode != "replace" {
		for id := range m.Selected {
			selected[id] = true
		}
	}
	for _, id := range ids {
		if mode == "remove" {
			delete(selected, id)
		} else {
			selected[id] = true
		}
	}
	if len(selected) > MaxSelected {
		return errors.New("selection limit reached")
	}
	result := make([]string, 0, len(selected))
	for id := range selected {
		result = append(result, id)
	}
	return m.SelectIDs(generation, result)
}

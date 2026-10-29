// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"strconv"
)

type providerItem struct {
	ID       string `json:"id"`
	Name     []byte `json:"name_bytes"`
	Label    string `json:"label"`
	Kind     string `json:"kind"`
	Mode     string `json:"mode"`
	Size     string `json:"size_bytes"`
	Modified string `json:"mtime_ns"`
	Selected bool   `json:"selected"`
	Focused  bool   `json:"focused"`
}

// Pure admitted-snapshot queries: no path lookup, content read or side effect.
func (b *browser) validFileQuery(query fileQuery) bool {
	if query.Version != 1 || query.Provider != b.instance ||
		!hexToken(query.Serial, 16) || query.Serial == "0000000000000000" {
		return false
	}
	if query.Scope == "action" {
		return hexToken(query.Generation, 32) && query.Offset == 0 && query.Limit == 0 &&
			len(query.Values) > 0 && len(query.Values) <= providerValuesMax &&
			providerKnownAction(query.Operation)
	}
	return query.Operation == "" && len(query.Values) == 0 &&
		query.Offset >= 0 && query.Offset <= collection.MaxItems && query.Limit >= 1 && query.Limit <= providerPageMax &&
		(query.Generation == "" || hexToken(query.Generation, 32)) &&
		(query.Scope == "state" || query.Scope == "items" || query.Scope == "selection" || query.Scope == "properties" || query.Scope == "directory_roles" || query.Scope == "file_roles")
}

func (b *browser) providerResult(query fileQuery) (map[string]any, string) {
	if !b.validFileQuery(query) {
		return nil, "invalid_request"
	}
	if b.snapshot == nil || b.snapshot.Schema != 1 {
		return nil, "not_ready"
	}
	generation := b.providerGeneration()
	if query.Generation != "" && query.Generation != generation {
		return nil, "stale_generation"
	}
	if query.Offset != 0 && query.Generation == "" {
		return nil, "generation_required"
	}
	result := map[string]any{"version": 1, "provider": b.instance,
		"generation": generation, "scope": query.Scope, "observed": true,
		"file_content": false}
	selected := b.model.SelectedIDs()
	switch query.Scope {
	case "state":
		if query.Offset != 0 {
			return nil, "invalid_request"
		}
		visible := b.model.VisibleEntries()
		result["path_bytes"] = b.snapshot.Path
		result["loaded"], result["visible"] = len(b.model.Entries), len(visible)
		result["selected"], result["focus"] = len(selected), b.model.Focus
		result["filter"], result["sort"] = b.model.Filter(), b.ordering()
		result["reverse"], result["view"] = b.reverse, b.viewing()
		result["busy"], result["watch"] = b.active, b.watch.state()
		result["page_max"], result["selection_limit"] = providerPageMax, collection.MaxSelected
		result["progress"] = b.progress()
		result["availability"] = b.availability()
		result["availability_generation"] = generation
		result["bindings"] = b.bindingStats()
		result["actions"] = true
		result["file_actions"] = true
		result["work"] = b.providerWork
		result["jobs"] = b.jobSummary()
		result["directory_roles"] = b.directoryRoleSummary()
		result["file_roles"] = b.fileRoleSummary()
		result["operation_retention"] = "latest_accepted"
		result["path_request_max"] = providerPathMax
		result["target_request_max"] = providerTargetMax
		result["hidden"] = b.snapshot.Hidden
		result["watch_enabled"] = b.watch != nil && b.watch.enabled
		result["opener_available"], result["terminal_available"] = b.opener != "", b.terminal != ""
		actions := make([]map[string]any, 0, len(b.actions))
		for index, action := range b.actions {
			actions = append(actions, map[string]any{"index": index, "label": displayText(action.Label, 96)})
		}
		result["configured_actions"] = actions
		result["selection_edit_max"] = providerEditMax
		result["action_operations"] = providerActionNames()
	case "file_roles":
		if code := b.fileRolePage(query, result); code != "" {
			return nil, code
		}
	case "directory_roles":
		if code := b.directoryRolePage(query, result); code != "" {
			return nil, code
		}
	case "properties":
		if query.Offset != 0 {
			return nil, "invalid_request"
		}
		observation, err := b.snapshot.SummarizeSelection(selected)
		if err != nil {
			return nil, "selection_unavailable"
		}
		result["properties"] = map[string]any{"count": observation.Count,
			"files": observation.Files, "directories": observation.Directories,
			"links": observation.Links, "special": observation.Special,
			"regular_file_bytes": strconv.FormatInt(observation.FileBytes, 10),
			"recursive":          false, "links_followed": false}
	case "items", "selection":
		entries := b.model.VisibleEntries()
		if query.Scope == "selection" {
			entries = make([]collection.Entry, 0, len(selected))
			for _, entry := range b.model.Entries {
				if b.model.Selected[entry.ID] {
					entries = append(entries, entry)
				}
			}
		}
		if query.Offset > len(entries) {
			return nil, "offset_out_of_range"
		}
		end := query.Offset + query.Limit
		if end > len(entries) {
			end = len(entries)
		}
		known := make(map[string]filemodel.Item, len(b.snapshot.Items))
		for _, item := range b.snapshot.Items {
			known[item.ID] = item
		}
		items := make([]providerItem, 0, end-query.Offset)
		for _, entry := range entries[query.Offset:end] {
			item, found := known[entry.ID]
			if !found {
				return nil, "snapshot_unavailable"
			}
			items = append(items, providerItem{item.ID, item.Name, displayText(entry.Label, 90),
				item.Kind, item.Identity.Mode.String(),
				strconv.FormatInt(item.Identity.Size, 10), strconv.FormatInt(item.Identity.MTime, 10),
				b.model.Selected[item.ID], b.model.Focus == item.ID})
		}
		result["offset"], result["total"], result["items"] = query.Offset, len(entries), items
		result["next_offset"] = nil
		if end < len(entries) {
			result["next_offset"] = end
		}
	default:
		return nil, "unsupported_scope"
	}
	return result, ""
}

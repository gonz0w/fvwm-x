// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"strconv"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/transport"
)

const providerPageMax = 8
const providerResultMax = 8192
const providerChunk = 256

type fileQuery struct {
	Version    int             `json:"version"`
	Serial     string          `json:"serial"`
	Provider   string          `json:"provider"`
	Scope      string          `json:"scope"`
	Generation string          `json:"generation"`
	Offset     int             `json:"offset"`
	Limit      int             `json:"limit"`
	Operation  string          `json:"operation,omitempty"`
	Values     json.RawMessage `json:"values,omitempty"`
}

type providerRegistration struct {
	Version int    `json:"version"`
	ID      string `json:"id"`
	Status  string `json:"status"`
	Result  struct {
		Provider   string `json:"provider"`
		Registered bool   `json:"registered"`
	} `json:"result"`
}

func (b *browser) registerProvider(ctx context.Context,
	registered <-chan providerRegistration, finished <-chan error) error {
	data, _ := json.Marshal(map[string]any{"version": 1, "id": "file-register",
		"method": "register", "resource": "files", "provider": b.instance})
	if err := b.session.Send("request " + string(data)); err != nil {
		return err
	}
	timer := time.NewTimer(transport.WriteTimeout)
	defer timer.Stop()
	select {
	case reply := <-registered:
		if reply.Version == 1 && reply.Status == "ok" && reply.Result.Registered && reply.Result.Provider == b.instance {
			return nil
		}
		return errors.New("file provider registration refused; use matching FvwmMFL/browser builds")
	case <-finished:
		return errors.New("file provider connection closed during registration")
	case <-ctx.Done():
		return ctx.Err()
	case <-timer.C:
		return errors.New("file provider registration timed out")
	}
}

func hexToken(value string, size int) bool {
	if len(value) != size {
		return false
	}
	for _, c := range value {
		if !(c >= '0' && c <= '9' || c >= 'a' && c <= 'f') {
			return false
		}
	}
	return true
}

func (b *browser) providerGeneration() string {
	// A snapshot token binds metadata/projection plus current native selection
	// and focus. Draft inputs and file content never enter the public snapshot.
	data, _ := json.Marshal(struct {
		Collection   uint64
		Selected     []string
		Focus        string
		View         string
		Inspected    []string
		Work         *providerWork
		Busy         bool
		Hidden       bool
		WatchEnabled bool
	}{b.model.Generation, b.model.SelectedIDs(), b.model.Focus, b.viewing(), b.inspected, b.providerWork, b.active, b.snapshot != nil && b.snapshot.Hidden, b.watch != nil && b.watch.enabled})
	sum := sha256.Sum256(data)
	return hex.EncodeToString(sum[:16])
}

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
			(query.Operation == "selection" || query.Operation == "filter" || query.Operation == "sort" ||
				query.Operation == "view" || query.Operation == "properties" ||
				providerWorkerAction(query.Operation) || query.Operation == "cancel" || query.Operation == "watch")
	}
	return query.Operation == "" && len(query.Values) == 0 &&
		query.Offset >= 0 && query.Offset <= collection.MaxItems && query.Limit >= 1 && query.Limit <= providerPageMax &&
		(query.Generation == "" || hexToken(query.Generation, 32)) &&
		(query.Scope == "state" || query.Scope == "items" || query.Scope == "selection" || query.Scope == "properties")
}

func (b *browser) providerResult(query fileQuery) (any, string) {
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
		result["actions"] = true
		result["file_actions"] = true
		result["work"] = b.providerWork
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
		result["action_operations"] = []string{"selection", "filter", "sort", "view", "properties", "navigate", "refresh", "open", "terminal", "custom", "cancel", "reveal", "inspect", "hidden", "watch"}
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

func (b *browser) answerProvider(query fileQuery) (bool, error) {
	if !b.validFileQuery(query) {
		return false, errors.New("invalid file provider query")
	}
	var result any
	code := ""
	applied := false
	var action providerAction
	if query.Scope == "action" {
		var err error
		action, err = decodeProviderAction(query)
		if err != nil {
			code = "invalid_request"
		}
	}
	if code == "" && b.snapshot != nil && b.alias != "" {
		b.renderDeadline = time.Now().Add(workerLimit)
		packet, err := b.snapshotNative("q"+query.Serial, b.nativeGeneration, 0)
		if err != nil || b.model.SelectNative(b.model.Generation, packet) != nil {
			code = "native_state_unavailable"
		}
	}
	if code == "" && query.Scope == "action" {
		code = b.applyProviderAction(query, action)
		if code == "" {
			if err := b.show(); err != nil {
				// Retire the view on failed publication: never acknowledge a
				// split model/native state as an applied operation.
				return false, err
			}
			applied = true
			stateQuery := fileQuery{Version: 1, Provider: b.instance,
				Serial: query.Serial, Scope: "state", Limit: providerPageMax}
			result, code = b.providerResult(stateQuery)
			if code == "" {
				state := result.(map[string]any)
				state["scope"], state["operation"], state["applied"] = "action", query.Operation, true
			}
		}
	} else if code == "" {
		result, code = b.providerResult(query)
	}
	envelope := map[string]any{"result": result}
	if code != "" {
		envelope = map[string]any{"error": map[string]string{"code": code}}
	}
	data, err := json.Marshal(envelope)
	if err != nil {
		return false, err
	}
	if len(data) > providerResultMax {
		data = []byte(`{"error":{"code":"result_too_large"}}`)
	}
	parts := 0
	for start := 0; start < len(data); start += providerChunk {
		end := start + providerChunk
		if end > len(data) {
			end = len(data)
		}
		if err := b.session.Send(fmt.Sprintf("files_reply %s part %d %s", query.Serial, parts, hex.EncodeToString(data[start:end]))); err != nil {
			return false, err
		}
		parts++
	}
	return applied, b.session.Send(fmt.Sprintf("files_reply %s end %d", query.Serial, parts))
}

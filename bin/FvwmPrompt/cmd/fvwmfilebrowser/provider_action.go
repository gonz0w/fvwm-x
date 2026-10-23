// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"io"
	"strings"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
)

const providerEditMax = 8
const providerValuesMax = 384

type providerAction struct {
	mode, text, key string
	ids             []string
	reverse         bool
}

// Values are bounded literal data, never commands. Decode before any native
// handshake so malformed client text cannot change state or retire a provider.
func decodeProviderAction(query fileQuery) (providerAction, error) {
	var action providerAction
	data := query.Values
	if len(data) > providerValuesMax || !utf8.Valid(data) {
		return action, errors.New("invalid action values")
	}
	decoder := json.NewDecoder(bytes.NewReader(data))
	if token, err := decoder.Token(); err != nil || token != json.Delim('{') {
		return action, errors.New("action values must be an object")
	}
	fields := make(map[string]json.RawMessage)
	for decoder.More() {
		key, err := decoder.Token()
		name, ok := key.(string)
		if err != nil || !ok || fields[name] != nil || len(fields) == 2 {
			return action, errors.New("invalid or duplicate action field")
		}
		var value json.RawMessage
		if err := decoder.Decode(&value); err != nil {
			return action, err
		}
		fields[name] = value
	}
	if token, err := decoder.Token(); err != nil || token != json.Delim('}') {
		return action, errors.New("invalid action object")
	}
	var extra any
	if decoder.Decode(&extra) != io.EOF {
		return action, errors.New("trailing action values")
	}
	stringField := func(name string, target *string) bool {
		value := fields[name]
		return len(value) > 0 && value[0] == '"' && json.Unmarshal(value, target) == nil
	}
	valid := false
	switch query.Operation {
	case "selection":
		if !stringField("mode", &action.mode) {
			break
		}
		if action.mode == "clear" {
			valid = len(fields) == 1
			break
		}
		value := fields["ids"]
		if len(fields) != 2 || len(value) == 0 || value[0] != '[' ||
			json.Unmarshal(value, &action.ids) != nil || len(action.ids) == 0 || len(action.ids) > providerEditMax {
			break
		}
		valid = action.mode == "replace" || action.mode == "add" || action.mode == "remove"
		seen := make(map[string]bool, len(action.ids))
		for _, id := range action.ids {
			if !hexToken(id, 32) || seen[id] {
				valid = false
			}
			seen[id] = true
		}
	case "filter":
		valid = len(fields) == 1 && stringField("text", &action.text) && len(action.text) <= collection.MaxFilter && !strings.ContainsRune(action.text, 0)
		// SetFilter also validates UTF-8/NUL before publishing a candidate.
	case "sort":
		value := fields["reverse"]
		valid = len(fields) == 2 && stringField("key", &action.key) &&
			(bytes.Equal(value, []byte("true")) || bytes.Equal(value, []byte("false"))) &&
			json.Unmarshal(value, &action.reverse) == nil &&
			(action.key == "name" || action.key == "kind" || action.key == "size" || action.key == "modified")
	case "view":
		valid = len(fields) == 1 && stringField("mode", &action.mode) && (action.mode == "list" || action.mode == "grid")
	case "properties":
		valid = len(fields) == 0
	}
	if !valid {
		return action, errors.New("invalid action schema")
	}
	return action, nil
}

func (b *browser) applyProviderAction(query fileQuery, action providerAction) string {
	if b.snapshot == nil || b.snapshot.Schema != 1 {
		return "not_ready"
	}
	if query.Generation != b.providerGeneration() {
		return "stale_generation"
	}
	// Collection policy publishes new slices/maps only after full admission.
	// A refused candidate cannot change live selection, order or projection.
	candidate := b.model
	switch query.Operation {
	case "selection":
		if err := candidate.EditSelection(candidate.Generation, action.mode, action.ids); err != nil {
			return "invalid_selection"
		}
		b.model, b.replaceSelection, b.inspected = candidate, true, nil
	case "filter":
		if err := candidate.SetFilter(action.text); err != nil {
			return "operation_refused"
		}
		b.model = candidate
	case "sort":
		if action.key != b.ordering() || action.reverse != b.reverse {
			entries, err := b.snapshot.OrderedEntries(action.key, action.reverse)
			if err != nil || candidate.Replace(entries) != nil {
				return "operation_refused"
			}
			b.model, b.order, b.reverse = candidate, action.key, action.reverse
		}
	case "view":
		b.view = action.mode
	case "properties":
		ids := candidate.SelectedIDs()
		if _, err := b.snapshot.ObservedSelection(ids); err != nil {
			return "selection_unavailable"
		}
		b.inspected = append([]string(nil), ids...)
	default:
		return "invalid_request"
	}
	return ""
}

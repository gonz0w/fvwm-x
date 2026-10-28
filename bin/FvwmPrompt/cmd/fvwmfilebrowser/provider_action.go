// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"errors"
	"path/filepath"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
)

const providerEditMax = 8
const providerValuesMax = 384
const providerPathMax = 192
const providerTargetMax = 8

type providerAction struct {
	mode, text, key string
	roleGeneration  string
	id              string
	path            []byte
	targets         [][]byte
	enabled         bool
	index           int
	ids             []string
	reverse         bool
}

// Values are bounded literal data, never commands. Decode before any native
// handshake so malformed client text cannot change state or retire a provider.
func decodeProviderAction(query fileQuery) (providerAction, error) {
	var action providerAction
	fields, err := decodeProviderValues(query.Values)
	if err != nil {
		return action, err
	}
	valid := false
	switch query.Operation {
	case "selection":
		if !fields.string("mode", &action.mode) {
			break
		}
		if action.mode == "clear" {
			valid = len(fields) == 1
			break
		}
		if len(fields) != 2 || !fields.strings("ids", &action.ids, providerEditMax) {
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
		valid = len(fields) == 1 && fields.string("text", &action.text) && len(action.text) <= collection.MaxFilter && !strings.ContainsRune(action.text, 0)
		// SetFilter also validates UTF-8/NUL before publishing a candidate.
	case "sort":
		valid = len(fields) == 2 && fields.string("key", &action.key) &&
			fields.boolean("reverse", &action.reverse) &&
			(action.key == "name" || action.key == "kind" || action.key == "size" || action.key == "modified")
	case "view":
		valid = len(fields) == 1 && fields.string("mode", &action.mode) && (action.mode == "list" || action.mode == "grid")
	case "navigate":
		if !fields.string("mode", &action.mode) {
			break
		}
		switch action.mode {
		case "home", "up", "back":
			valid = len(fields) == 1
		case "item":
			valid = len(fields) == 2 && fields.string("id", &action.id) && hexToken(action.id, 32)
		case "path":
			var encoded string
			if len(fields) == 2 && fields.string("path_bytes", &encoded) {
				var err error
				action.path, err = decodeProviderPath(encoded)
				valid = err == nil
			}
		}
	case "reveal", "inspect":
		var paths []string
		if len(fields) != 1 || !fields.strings("paths_bytes", &paths, providerTargetMax) {
			break
		}
		seen := make(map[string]bool, len(paths))
		valid = true
		for _, path := range paths {
			decoded, err := decodeProviderPath(path)
			if err != nil || seen[string(decoded)] {
				valid = false
				break
			}
			seen[string(decoded)] = true
			action.targets = append(action.targets, decoded)
		}
	case "hidden", "watch":
		valid = len(fields) == 1 && fields.boolean("enabled", &action.enabled)
	case "open":
		valid = len(fields) == 1 && fields.string("id", &action.id) && hexToken(action.id, 32)
	case "directory-role-launch":
		valid = len(fields) == 2 && fields.string("generation", &action.roleGeneration) && hexToken(action.roleGeneration, 32) && fields.string("id", &action.id) && hexToken(action.id, 32)
	case "custom":
		value := fields["index"]
		valid = len(fields) == 1 && json.Unmarshal(value, &action.index) == nil && len(value) == 1 && action.index >= 0 && action.index < 4
	case "cancel":
		valid = len(fields) == 1 && fields.string("id", &action.id) && hexToken(action.id, 16) && action.id != "0000000000000000"
	case "properties", "refresh", "terminal", "present", "directory-roles", "directory-roles-close":
		valid = len(fields) == 0
	}
	if !valid {
		return action, errors.New("invalid action schema")
	}
	return action, nil
}

func decodeProviderPath(encoded string) ([]byte, error) {
	if len(encoded) > 256 {
		return nil, errors.New("encoded path exceeds limit")
	}
	path, err := base64.StdEncoding.Strict().DecodeString(encoded)
	if err != nil || len(path) == 0 || len(path) > providerPathMax || !filepath.IsAbs(string(path)) || bytes.ContainsRune(path, 0) {
		return nil, errors.New("expected a bounded canonical base64 absolute path")
	}
	return path, nil
}

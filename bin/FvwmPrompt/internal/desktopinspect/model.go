// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package desktopinspect collects bounded read-only inventory. Its rows are
// presentation data, never commands or proof of final rendered state.
package desktopinspect

import (
	"bytes"
	"encoding/json"
	"errors"
	"io"
	"strings"
	"unicode"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopcheck"
)

const ResultLimit = 512 * 1024
const RequestLimit = 64 * 1024
const RowLimit = 128

type Request struct {
	Scope              string               `json:"scope"`
	Filter             string               `json:"filter"`
	Provider           string               `json:"provider"`
	SettingsGeneration string               `json:"settings_generation"`
	Settings           json.RawMessage      `json:"settings"`
	Appearance         string               `json:"appearance"`
	Colors             map[string]string    `json:"colors"`
	Source             desktopcheck.Request `json:"source"`
}
type Result struct {
	SchemaVersion      int       `json:"schema_version"`
	Scope              string    `json:"scope"`
	Filter             string    `json:"filter"`
	Provider           string    `json:"provider"`
	SettingsGeneration string    `json:"settings_generation"`
	Sections           []Section `json:"sections"`
}
type Section struct {
	ID         string `json:"id"`
	Title      string `json:"title"`
	State      string `json:"state"`
	Generation string `json:"generation"`
	Reason     string `json:"reason"`
	Rows       []Row  `json:"rows"`
	Truncated  bool   `json:"truncated"`
}
type Row struct {
	Label string `json:"label"`
	Value string `json:"value"`
}

var scopes = []string{"settings", "theme", "bindings", "layout", "sources", "commands"}

func member(s string, values []string) bool {
	for _, value := range values {
		if s == value {
			return true
		}
	}
	return false
}
func safe(s string, max int) bool {
	if !utf8.ValidString(s) || utf8.RuneCountInString(s) > max {
		return false
	}
	for _, c := range s {
		if unicode.IsControl(c) {
			return false
		}
	}
	return true
}
func identity(s string) bool {
	if len(s) > 128 {
		return false
	}
	for _, c := range s {
		if !(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '-' || c == '_' || c == '.') {
			return false
		}
	}
	return true
}
func (r Request) validate() error {
	if r.Scope != "all" && !member(r.Scope, scopes) || len(r.Filter) > 128 || !safe(r.Filter, 128) || !identity(r.Provider) || !identity(r.SettingsGeneration) {
		return errors.New("invalid inspector request fields")
	}
	if r.Appearance != "" && r.Appearance != "light" && r.Appearance != "dark" {
		return errors.New("invalid appearance projection")
	}
	if len(r.Colors) > 8 {
		return errors.New("invalid color projection")
	}
	for k, v := range r.Colors {
		if !member(k, []string{"background", "surface", "raised", "text", "muted", "border", "accent", "on_accent"}) || len(v) != 7 || v[0] != '#' || strings.Trim(v[1:], "0123456789abcdefABCDEF") != "" {
			return errors.New("invalid color projection")
		}
	}
	if r.Source != (desktopcheck.Request{}) {
		if err := r.Source.Validate(); err != nil {
			return err
		}
	}
	if len(r.Settings) > 0 && !bytes.Equal(r.Settings, []byte("null")) {
		var value map[string]json.RawMessage
		if err := strict(r.Settings, &value, RequestLimit); err != nil || value == nil {
			return errors.New("invalid settings snapshot")
		}
	}
	return nil
}
func DecodeRequest(data []byte) (Request, error) {
	var r Request
	if err := strict(data, &r, RequestLimit); err != nil {
		return r, err
	}
	var fields map[string]json.RawMessage
	_ = json.Unmarshal(data, &fields)
	if !knownFields(fields, []string{"scope", "filter", "provider", "settings_generation", "settings", "appearance", "colors", "source"}) {
		return Request{}, errors.New("unknown inspector request field")
	}
	if source, exists := fields["source"]; exists {
		var fields map[string]json.RawMessage
		_ = json.Unmarshal(source, &fields)
		if len(fields) != 4 || !knownFields(fields, []string{"path", "userdir", "datadir", "cwd"}) {
			return Request{}, errors.New("invalid inspector source fields")
		}
	}
	return r, r.validate()
}
func DecodeResult(data []byte) (Result, error) {
	var r Result
	if err := strict(data, &r, ResultLimit); err != nil {
		return Result{}, err
	}
	// Require every schema field, including false/empty fields: missing fields
	// must not masquerade as valid zero values in worker/UI ownership transfer.
	var fields map[string]json.RawMessage
	_ = json.Unmarshal(data, &fields)
	if len(fields) != 6 || !knownFields(fields, []string{"schema_version", "scope", "filter", "provider", "settings_generation", "sections"}) {
		return Result{}, errors.New("incomplete inspector result")
	}
	var sections []map[string]json.RawMessage
	_ = json.Unmarshal(fields["sections"], &sections)
	for _, section := range sections {
		if len(section) != 7 || !knownFields(section, []string{"id", "title", "state", "generation", "reason", "rows", "truncated"}) {
			return Result{}, errors.New("incomplete inspector section")
		}
		var rows []map[string]json.RawMessage
		_ = json.Unmarshal(section["rows"], &rows)
		for _, row := range rows {
			if len(row) != 2 || !knownFields(row, []string{"label", "value"}) {
				return Result{}, errors.New("incomplete inspector row")
			}
		}
	}
	if err := r.validate(); err != nil {
		return Result{}, err
	}
	return r, nil
}
func knownFields(fields map[string]json.RawMessage, allowed []string) bool {
	for key := range fields {
		if !member(key, allowed) {
			return false
		}
	}
	return true
}
func (r Result) validate() error {
	if r.SchemaVersion != 1 || r.Scope != "all" && !member(r.Scope, scopes) || len(r.Filter) > 128 || !safe(r.Filter, 128) || !identity(r.Provider) || !identity(r.SettingsGeneration) || r.Sections == nil || len(r.Sections) > 6 {
		return errors.New("invalid inspector result")
	}
	if r.Scope == "all" && len(r.Sections) != 6 || r.Scope != "all" && len(r.Sections) != 1 {
		return errors.New("invalid inspector sections")
	}
	seen := map[string]bool{}
	for _, s := range r.Sections {
		if !member(s.ID, scopes) || seen[s.ID] || r.Scope != "all" && s.ID != r.Scope || !safe(s.Title, 80) || !safe(s.Reason, 200) || !identity(s.Generation) || !member(s.State, []string{"observed", "provider-policy", "literal-observations", "partial", "unavailable", "stale"}) || s.Rows == nil || len(s.Rows) > RowLimit {
			return errors.New("invalid inspector section")
		}
		seen[s.ID] = true
		for _, row := range s.Rows {
			if !safe(row.Label, 80) || !safe(row.Value, 200) {
				return errors.New("invalid inspector row")
			}
		}
	}
	return nil
}

// Validate duplicate keys and nesting before the typed decoder. json.Unmarshal
// alone permits ambiguous duplicate fields and null scalar substitutions.
func strict(data []byte, dest any, limit int) error {
	if len(data) > limit || !utf8.Valid(data) {
		return errors.New("inspector JSON size or encoding")
	}
	d := json.NewDecoder(bytes.NewReader(data))
	var walk func(int) error
	walk = func(depth int) error {
		if depth > 16 {
			return errors.New("inspector JSON nesting")
		}
		t, err := d.Token()
		if err != nil {
			return err
		}
		if delim, ok := t.(json.Delim); ok {
			switch delim {
			case '{':
				seen := map[string]bool{}
				for d.More() {
					key, err := d.Token()
					if err != nil {
						return err
					}
					name, ok := key.(string)
					if !ok || seen[name] {
						return errors.New("duplicate inspector field")
					}
					seen[name] = true
					if err := walk(depth + 1); err != nil {
						return err
					}
				}
			case '[':
				for d.More() {
					if err := walk(depth + 1); err != nil {
						return err
					}
				}
			default:
				return errors.New("invalid inspector delimiter")
			}
			_, err = d.Token()
			return err
		}
		return nil
	}
	if err := walk(0); err != nil {
		return err
	}
	if _, err := d.Token(); err != io.EOF {
		return errors.New("trailing inspector data")
	}
	d = json.NewDecoder(bytes.NewReader(data))
	d.DisallowUnknownFields()
	if err := d.Decode(dest); err != nil {
		return err
	}
	// Reject null except explicitly optional request policy/source data. Typed
	// decoding cannot distinguish null from an absent string/bool otherwise.
	return rejectNull(data, dest)
}
func rejectNull(data []byte, dest any) error {
	var value any
	_ = json.Unmarshal(data, &value)
	var visit func(any) bool
	visit = func(v any) bool {
		switch x := v.(type) {
		case nil:
			return true
		case []any:
			for _, e := range x {
				if visit(e) {
					return true
				}
			}
		case map[string]any:
			for _, e := range x {
				if visit(e) {
					return true
				}
			}
		}
		return false
	}
	if _, ok := dest.(*Request); ok {
		if fields, ok := value.(map[string]any); ok {
			delete(fields, "settings")
			delete(fields, "colors")
			if !visit(fields) {
				return nil
			}
		}
	} else if _, ok := dest.(*Result); !ok {
		return nil
	}
	if visit(value) {
		return errors.New("null inspector field")
	}
	return nil
}

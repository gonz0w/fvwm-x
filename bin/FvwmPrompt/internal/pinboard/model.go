// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package pinboard owns data-only references. Blocking acquisition/publication
// belongs in one disposable worker, never in the native X owner.
package pinboard

import (
	"bytes"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"io"
	"path/filepath"
	"strconv"
	"strings"
	"unicode"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

const MaxPins = 256
const MaxStateBytes = 2 * 1024 * 1024
const MaxLabel = 256
const MaxGeneration = 2147483647

type Layout struct {
	CellWidth  int `json:"cell_width"`
	CellHeight int `json:"cell_height"`
	IconSize   int `json:"icon_size"`
}

type Pin struct {
	ID      string `json:"id"`
	Target  []byte `json:"target_bytes"`
	Label   string `json:"label"`
	Icon    string `json:"icon,omitempty"`
	Monitor []byte `json:"monitor_bytes,omitempty"`
	Column  int    `json:"column"`
	Row     int    `json:"row"`
}

type State struct {
	Schema   int    `json:"schema_version"`
	Revision uint64 `json:"revision"`
	Pins     []Pin  `json:"pins"`
	Layout   Layout `json:"layout"`
}

func Empty() State { return State{Schema: 1, Pins: []Pin{}, Layout: Layout{96, 96, 32}} }

func NewID() (string, error) {
	var data [16]byte
	if _, err := rand.Read(data[:]); err != nil {
		return "", err
	}
	return hex.EncodeToString(data[:]), nil
}

func Hex(value string, size int) bool {
	if len(value) != size {
		return false
	}
	for _, c := range value {
		if c < '0' || c > '9' && c < 'a' || c > 'f' {
			return false
		}
	}
	return true
}

func validLabel(value string) bool {
	if value == "" || len(value) > MaxLabel || !utf8.ValidString(value) {
		return false
	}
	for _, c := range value {
		if !unicode.IsPrint(c) {
			return false
		}
	}
	return true
}

func ValidIcon(value string) bool {
	if len(value) > MaxLabel || !utf8.ValidString(value) || strings.ContainsAny(value, "/\\") {
		return false
	}
	for _, c := range value {
		if !unicode.IsPrint(c) {
			return false
		}
	}
	return value != "." && value != ".."
}

func DefaultLabel(target []byte) string {
	label := filepath.Base(string(target))
	if !utf8.ValidString(label) || !validLabel(label) {
		label = strconv.QuoteToGraphic(label)
	}
	if len(label) > MaxLabel {
		for len(label) > MaxLabel-3 {
			_, size := utf8.DecodeLastRuneInString(label)
			label = label[:len(label)-size]
		}
		label += "..."
	}
	return label
}

func (s State) Validate() error {
	if s.Schema != 1 || s.Pins == nil || len(s.Pins) > MaxPins || s.Layout.CellWidth < 64 || s.Layout.CellWidth > 512 ||
		s.Layout.CellHeight < 64 || s.Layout.CellHeight > 512 || s.Layout.IconSize < 16 || s.Layout.IconSize > 128 {
		return errors.New("invalid pinboard schema, count or layout")
	}
	seen := make(map[string]bool, len(s.Pins))
	for _, p := range s.Pins {
		// ParseTarget accepts URIs as input convenience; stored values must be
		// literal absolute bytes, including root and invalid UTF-8 names.
		if !Hex(p.ID, 32) || seen[p.ID] || !filepath.IsAbs(string(p.Target)) ||
			len(p.Target) > filemodel.MaxPath || bytes.IndexByte(p.Target, 0) >= 0 ||
			!validLabel(p.Label) || !ValidIcon(p.Icon) || len(p.Monitor) > 256 ||
			bytes.IndexByte(p.Monitor, 0) >= 0 || p.Column < 0 || p.Row < 0 ||
			p.Column > MaxGeneration || p.Row > MaxGeneration {
			return errors.New("invalid or duplicate pin reference")
		}
		for _, component := range strings.Split(string(p.Target), string(filepath.Separator)) {
			if component == "." || component == ".." {
				return errors.New("pin targets cannot contain dot path components")
			}
		}
		seen[p.ID] = true
	}
	return nil
}

func Decode(data []byte) (State, error) {
	s := State{}
	if len(data) > MaxStateBytes {
		return s, errors.New("pinboard exceeds 2 MiB")
	}
	if err := DecodeStrict(data, &s); err != nil {
		return State{}, err
	}
	// encoding/json accepts null for scalar zero values. The persisted schema
	// requires actual typed fields, so distinguish absence/null from zero.
	fields := func(raw []byte, required ...string) (map[string]json.RawMessage, error) {
		var values map[string]json.RawMessage
		if err := json.Unmarshal(raw, &values); err != nil {
			return nil, err
		}
		for _, name := range required {
			if _, ok := values[name]; !ok {
				return nil, errors.New("missing pinboard field: " + name)
			}
		}
		for _, value := range values {
			if bytes.Equal(bytes.TrimSpace(value), []byte("null")) {
				return nil, errors.New("null pinboard field")
			}
		}
		return values, nil
	}
	root, err := fields(data, "schema_version", "revision", "pins", "layout")
	if err != nil {
		return State{}, err
	}
	if _, err := fields(root["layout"], "cell_width", "cell_height", "icon_size"); err != nil {
		return State{}, err
	}
	var pins []json.RawMessage
	if err := json.Unmarshal(root["pins"], &pins); err != nil {
		return State{}, err
	}
	for _, pin := range pins {
		if _, err := fields(pin, "id", "target_bytes", "label", "column", "row"); err != nil {
			return State{}, err
		}
	}
	if err := s.Validate(); err != nil {
		return State{}, err
	}
	return s, nil
}

// DecodeStrict rejects duplicate JSON members as well as unknown/trailing data.
// It is shared by the small private worker records.
func DecodeStrict(data []byte, target any) error {
	if len(data) > 4*MaxStateBytes {
		return errors.New("record exceeds limit")
	}
	if !utf8.Valid(data) {
		return errors.New("record contains invalid UTF-8 JSON")
	}
	d := json.NewDecoder(bytes.NewReader(data))
	var walk func(int) error
	walk = func(depth int) error {
		if depth > 32 {
			return errors.New("record nesting exceeds limit")
		}
		token, err := d.Token()
		if err != nil {
			return err
		}
		if delimiter, ok := token.(json.Delim); ok {
			if delimiter != '{' && delimiter != '[' {
				return errors.New("unexpected JSON delimiter")
			}
			seen := map[string]bool{}
			count := 0
			for d.More() {
				count++
				if count > MaxPins {
					return errors.New("record container exceeds item limit")
				}
				if delimiter == '{' {
					key, err := d.Token()
					if err != nil {
						return err
					}
					name, ok := key.(string)
					if !ok || seen[name] {
						return errors.New("duplicate JSON member")
					}
					seen[name] = true
				}
				if err := walk(depth + 1); err != nil {
					return err
				}
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
		return errors.New("trailing JSON data")
	}
	d = json.NewDecoder(bytes.NewReader(data))
	d.DisallowUnknownFields()
	return d.Decode(target)
}

func (s State) Encode() ([]byte, error) {
	if err := s.Validate(); err != nil {
		return nil, err
	}
	data, err := json.Marshal(s)
	if len(data) > MaxStateBytes {
		return nil, errors.New("pinboard exceeds 2 MiB")
	}
	return data, err
}

func (s State) Clone() State {
	copy := s
	copy.Pins = append([]Pin{}, s.Pins...)
	for i := range copy.Pins {
		copy.Pins[i].Target = bytes.Clone(copy.Pins[i].Target)
		copy.Pins[i].Monitor = bytes.Clone(copy.Pins[i].Monitor)
	}
	return copy
}

func (s State) Index(id string) int {
	for i := range s.Pins {
		if s.Pins[i].ID == id {
			return i
		}
	}
	return -1
}

func (s State) Entries() []collection.Entry {
	entries := make([]collection.Entry, 0, len(s.Pins))
	for _, p := range s.Pins {
		entries = append(entries, collection.Entry{ID: p.ID, Label: p.Label, Kind: "pin"})
	}
	return entries
}

// Change builds a candidate; only a successful save may replace committed data.
func (s State) Change(verb string, p Pin) (State, error) {
	if err := s.Validate(); err != nil {
		return State{}, err
	}
	next := s.Clone()
	i := next.Index(p.ID)
	switch verb {
	case "create":
		if i >= 0 || len(next.Pins) == MaxPins {
			return State{}, errors.New("pin already exists or board is full")
		}
		next.Pins = append(next.Pins, p)
	case "edit":
		if i < 0 {
			return State{}, errors.New("stale pin")
		}
		next.Pins[i].Label, next.Pins[i].Icon = p.Label, p.Icon
	case "move":
		if i < 0 {
			return State{}, errors.New("stale pin")
		}
		next.Pins[i].Monitor, next.Pins[i].Column, next.Pins[i].Row = bytes.Clone(p.Monitor), p.Column, p.Row
	case "unpin":
		if i < 0 {
			return State{}, errors.New("stale pin")
		}
		next.Pins = append(next.Pins[:i], next.Pins[i+1:]...)
	default:
		return State{}, errors.New("unsupported pin change")
	}
	if err := next.Validate(); err != nil {
		return State{}, err
	}
	return next, nil
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package actionpalette owns bounded metadata and typed intents, never effects.
package actionpalette

import (
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"strings"
	"unicode"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
)

const MaxCore = 640
const MaxApps = 500
const MaxActions = 16
const MaxRows = MaxCore + MaxApps + MaxActions

type Kind string

const (
	Window      Kind = "window"
	Desktop     Kind = "desktop"
	Application Kind = "application"
	Registered  Kind = "action"
)

type Intent string

const (
	Settings    Intent = "settings"
	Appearance  Intent = "appearance"
	Inspector   Intent = "inspector"
	CheckConfig Intent = "checkconfig"
	RefreshApps Intent = "refreshapps"
	Terminal    Intent = "terminal"
	Files       Intent = "files"
	Run         Intent = "run"
)

type Action struct {
	Intent            Intent
	Label, Capability string
}

func Registry() []Action {
	return []Action{{Settings, "Settings", "desktop"}, {Appearance, "Appearance", "desktop"}, {Inspector, "Configuration inspector", "worker"}, {CheckConfig, "Check configuration", "worker"}, {RefreshApps, "Refresh applications", "worker"}, {Terminal, "Terminal", "terminal"}, {Files, "Files", "worker"}, {Run, "Run command", "desktop"}}
}
func Allowed(intent Intent, capabilities map[string]bool) bool {
	for _, action := range Registry() {
		if action.Intent == intent {
			return capabilities[action.Capability]
		}
	}
	return false
}

type Row struct {
	ID, Label, Detail, Icon, Version string
	Kind                             Kind
	Index                            int
	Enabled                          bool
	Intent                           Intent
}
type Provider struct {
	Version, State, Reason, Coverage string
	Truncated                        bool
	Rows                             []Row
}
type Model struct {
	Collection collection.Model
	Providers  [4]Provider
	rows       map[string]Row
}

func ID(kind Kind, identity string) string {
	h := sha256.Sum256([]byte(string(kind) + "\x00" + identity))
	return hex.EncodeToString(h[:])
}
func Text(value string, limit int) bool {
	if len(value) > limit || !utf8.ValidString(value) {
		return false
	}
	for _, r := range value {
		if unicode.IsControl(r) {
			return false
		}
	}
	return true
}
func ASCII(value string, limit int) bool {
	if value == "" || len(value) > limit {
		return false
	}
	for _, c := range []byte(value) {
		if c < '!' || c > '~' {
			return false
		}
	}
	return true
}
func (m *Model) Replace(providers [4]Provider) error {
	entries := []collection.Entry{}
	rows := map[string]Row{}
	limits := [4]int{512, 128, MaxApps, MaxActions}
	kinds := [4]Kind{Window, Desktop, Application, Registered}
	for i, p := range providers {
		if !ASCII(p.Version, 128) || !Text(p.Reason, 256) || !Text(p.Coverage, 256) || len(p.Rows) > limits[i] {
			return errors.New("invalid palette provider")
		}
		if p.State != "observed" && p.State != "unavailable" && p.State != "loading" {
			return errors.New("invalid palette state")
		}
		for _, r := range p.Rows {
			if r.Kind != kinds[i] || !ASCII(r.ID, 128) || !Text(r.Label, 160) || r.Label == "" || !Text(r.Detail, 160) || !ASCII(r.Version, 128) || (r.Icon != "file" && r.Icon != "directory" && r.Icon != "other") {
				return errors.New("invalid palette row")
			}
			id := ID(r.Kind, r.ID)
			if _, exists := rows[id]; exists {
				return errors.New("duplicate palette identity")
			}
			r.Enabled = r.Enabled && p.State == "observed"
			rows[id] = r
			label := fmt.Sprintf("%s: %s", []string{"Window", "Desktop", "Application", "Action"}[i], r.Label)
			if r.Detail != "" {
				label += " — " + r.Detail
			}
			if !r.Enabled {
				label += " [unavailable]"
			}
			entries = append(entries, collection.Entry{ID: id, Label: label, Kind: r.Icon, Revision: ID(r.Kind, r.Version)})
		}
	}
	if len(entries) > MaxRows {
		return errors.New("palette row limit")
	}
	if _, err := m.Collection.Reconcile(m.Collection.Generation, entries); err != nil {
		return err
	}
	m.Providers = providers
	m.rows = rows
	return nil
}
func (m *Model) Resolve(generation uint64, id string) (Row, error) {
	if generation != m.Collection.Generation {
		return Row{}, errors.New("stale palette selection")
	}
	r, ok := m.rows[id]
	if !ok || !r.Enabled {
		return Row{}, errors.New("palette action unavailable")
	}
	visible := false
	for _, e := range m.Collection.VisibleEntries() {
		if e.ID == id {
			visible = true
			break
		}
	}
	if !visible {
		return Row{}, errors.New("palette selection is filtered out")
	}
	return r, nil
}
func (m *Model) Filter(query string) error {
	if !Text(query, 128) {
		return errors.New("query must contain at most 128 UTF-8 bytes")
	}
	return m.Collection.SetFilter(query)
}
func (m *Model) Summary() string {
	parts := []string{}
	for i, p := range m.Providers {
		text := fmt.Sprintf("%s %s (%d)", []string{"Windows", "Desktops", "Applications", "Actions"}[i], p.State, len(p.Rows))
		if p.Truncated {
			text += "; bounded inventory, more omitted"
		}
		if p.Reason != "" {
			text += "; " + p.Reason
		}
		parts = append(parts, text)
	}
	return strings.Join(parts, " | ")
}

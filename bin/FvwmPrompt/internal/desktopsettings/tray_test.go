// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import (
	"encoding/json"
	"reflect"
	"strings"
	"testing"
)

func TestTrayTypesBoundsAndHistoricalMigration(t *testing.T) {
	keys, err := Scope("group:tray")
	want := []string{"tray_enabled", "tray_gap", "tray_padding", "tray_orientation"}
	if err != nil || !reflect.DeepEqual(keys, want) {
		t.Fatal(keys, err)
	}
	for key, bad := range map[string][]any{
		"tray_enabled":     {"on", 1, nil},
		"tray_gap":         {-1, 65, true, "4", json.Number("4.0"), json.Number("4e0")},
		"tray_padding":     {-1, 65, false, nil},
		"tray_orientation": {"Horizontal", "vertical;Exec bad", false},
	} {
		for _, value := range bad {
			if _, err := Value(key, value, false); err == nil {
				t.Fatal("accepted invalid typed tray value", key, value)
			}
		}
	}
	for _, key := range []string{"tray_gap", "tray_padding"} {
		for _, value := range []int{0, 4, 64} {
			if got, err := Value(key, value, false); err != nil || got != value {
				t.Fatal(key, got, err)
			}
		}
		for _, value := range []string{"65", "+4", "4.0", " 4", "４"} {
			if _, err := Value(key, value, true); err == nil {
				t.Fatal("accepted invalid text tray value", key, value)
			}
		}
	}
	for version := 1; version <= 7; version++ {
		fixture := versionedFixture(version)
		data, _ := json.Marshal(fixture)
		got, err := DecodeState(data)
		if err != nil || got["palette"] != "dune" || got["appearance"] != "light" || got["keep_wallpaper"] != true {
			t.Fatal("historical settings changed", version, got, err)
		}
		for _, key := range want {
			if got[key] != Defaults()[key] {
				t.Fatal("missing tray default", version, key, got)
			}
		}
	}
	for version := 1; version <= 6; version++ {
		fixture := versionedFixture(version)
		delete(fixture, "motion")
		fixture["tray_enabled"] = true
		data, _ := json.Marshal(fixture)
		if _, err := DecodeState(data); err == nil {
			t.Fatal("old schema accepted a future tray field", version)
		}
	}
}

func TestTrayCompactEditorAndUnobservedReadiness(t *testing.T) {
	s := snapshotFixture()
	token := strings.Repeat("c", 32)
	appearance, err := Editor(s, "all", false, nil, token)
	if err != nil || len(appearance.Keys) != 11 {
		t.Fatal(appearance, err)
	}
	for _, key := range appearance.Keys {
		if TrayKey(key) || SessionKey(key) {
			t.Fatal("nonappearance field expanded the appearance editor", key)
		}
	}
	plan, err := Editor(s, "group:tray", false, Values{
		"tray_enabled": true, "tray_gap": 0, "tray_padding": 64,
		"tray_orientation": "vertical",
	}, token)
	if err != nil || len(plan.Keys) != 4 || len(plan.Lines) > 65 {
		t.Fatal(plan, err)
	}
	text := strings.Join(plan.Lines, "\n")
	for _, part := range []string{`Title "Tray settings"`, "another owner", "tray_enabled_true true on", `Input tray_gap 3 "0"`, "tray_orientation_vertical vertical on", "apply-settings " + token + " $(tray_enabled) $(tray_gap) $(tray_padding) $(tray_orientation)"} {
		if !strings.Contains(text, part) {
			t.Fatal(part, text)
		}
	}
	if strings.Contains(text, "live handshake") || strings.Contains(text, "preview-") {
		t.Fatal("tray editor borrowed session or appearance actions")
	}
	s.Submitted = true
	query, err := Query(s, "group:tray", true)
	if err != nil {
		t.Fatal(err)
	}
	for key, raw := range query {
		item := raw.(map[string]any)
		if SessionKey(key) || item["readiness"] != "unobserved" || item["effective"] != Defaults()[key] || item["application"] != "requested-state-submitted" {
			t.Fatal("tray preference claimed a live service observation", key, item)
		}
	}
}

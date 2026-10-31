// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import (
	"encoding/hex"
	"encoding/json"
	"reflect"
	"strings"
	"testing"
)

func TestApplicationPreferenceBoundariesAndHistoricalDefaults(t *testing.T) {
	keys, err := Scope("group:application")
	if err != nil || !reflect.DeepEqual(keys, []string{"native_settings_enabled", "application_font", "icon_theme", "cursor_theme", "cursor_size"}) {
		t.Fatal(keys, err)
	}
	for _, v := range []string{"", "Noto Sans 12", "字体 12", strings.Repeat("é", 64), `Font $() "quoted" \\`} {
		if got, err := Value("application_font", v, false); err != nil || got != v {
			t.Fatal(v, got, err)
		}
	}
	for key, cases := range map[string][]any{
		"application_font":        {true, nil, strings.Repeat("é", 65), "line\nline", "\x00", "\x7f", "\u0085", "\u2028", "\u2029", "\xff"},
		"icon_theme":              {".", "..", "/tmp/theme", "theme name", "é", strings.Repeat("x", 129), true},
		"cursor_theme":            {"a/b", "$theme", "a\n", 2},
		"cursor_size":             {-1, 129, "24", true, json.Number("24.0")},
		"native_settings_enabled": {"on", 1, nil},
	} {
		for _, value := range cases {
			if _, err := Value(key, value, false); err == nil {
				t.Fatal("accepted invalid", key, value)
			}
		}
	}
	for _, key := range []string{"icon_theme", "cursor_theme"} {
		for _, value := range []string{"", "Adwaita", "Theme_1.2-a", strings.Repeat("a", 128)} {
			if _, err := Value(key, value, false); err != nil {
				t.Fatal(err)
			}
		}
	}
	for version := 1; version < 9; version++ {
		fixture := versionedFixture(version)
		data, _ := json.Marshal(fixture)
		got, err := DecodeState(data)
		if err != nil {
			t.Fatal(version, err)
		}
		for _, key := range keys {
			if got[key] != Defaults()[key] {
				t.Fatal(version, key, got)
			}
		}
		// A future key cannot displace a historical required key.
		delete(fixture, "motion")
		fixture["application_font"] = "Sans 12"
		data, _ = json.Marshal(fixture)
		if _, err := DecodeState(data); err == nil {
			t.Fatal("future historical key", version)
		}
	}
}

func TestApplicationEditorKeepsLiteralStringsAndSeparateObservation(t *testing.T) {
	s := snapshotFixture()
	s.Saved["application_font"] = `Noto Sans "A" $x 13`
	s.Native = NativeObservation{Readiness: "conflict", ResourceState: "unavailable", Reason: "owner-conflict", Run: strings.Repeat("d", 32), Sequence: 1}
	plan, err := Editor(s, "group:application", false, nil, strings.Repeat("c", 32))
	if err != nil || len(plan.Keys) != 5 || len(plan.Lines) > 75 {
		t.Fatal(plan, err)
	}
	lines := strings.Join(plan.Lines, "\n")
	for _, part := range []string{`Title "Application preferences"`, "InputHex application_font 24 x" + hex.EncodeToString([]byte(s.Saved["application_font"].(string))), "x$(application_font:Base64) x$(icon_theme:Base64) x$(cursor_theme:Base64) $(cursor_size)", "retry-native-settings", "owner-conflict"} {
		if !strings.Contains(lines, part) {
			t.Fatal(part, lines)
		}
	}
	appearance, err := Editor(s, "all", false, nil, strings.Repeat("c", 32))
	if err != nil || len(appearance.Keys) != 11 {
		t.Fatal(appearance, err)
	}
	q, err := Query(s, "group:application", true)
	if err != nil {
		t.Fatal(err)
	}
	for _, raw := range q {
		row := raw.(map[string]any)
		if row["readiness"] != "conflict" || row["application"] != "native-conflict" || row["resource_state"] != "unavailable" {
			t.Fatal(row)
		}
	}
	q, err = Query(s, "group:application", false)
	if err != nil {
		t.Fatal(err)
	}
	for _, raw := range q {
		row := raw.(map[string]any)
		if _, ok := row["readiness"]; ok {
			t.Fatal("proposed snapshot claimed observation", row)
		}
	}
}

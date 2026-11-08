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

func TestDesktopMetricsBoundariesAndIndependentChoices(t *testing.T) {
	for _, font := range []string{"", "xft:Sans:size=14", "fixed", "字体 12"} {
		if got, err := Value("desktop_font", font, false); err != nil || got != font {
			t.Fatal(font, got, err)
		}
	}
	for _, font := range []string{" xft:Sans", "fixed ", "a\nb", "a\tb", "a\x00b", "\xff", "\u2028", "\u0085", `a"b`, `a\b`, "$name", "%n", "a&b", strings.Repeat("x", 129)} {
		if _, err := Value("desktop_font", font, false); err == nil {
			t.Fatal("unrepresentable font accepted", font)
		}
	}
	for _, row := range []struct {
		density           string
		item, title, form int
	}{{"inherit", 8, 10, 12}, {"compact", 4, 6, 6}, {"comfortable", 8, 10, 12}, {"spacious", 12, 14, 16}} {
		s := Defaults()
		s["desktop_font"], s["desktop_density"], s["desktop_border"] = "fixed", row.density, "strong"
		s["application_font"], s["palette"], s["icon_theme"] = "Other font 11", "dune", "Other"
		got, err := ResolveDesktopMetrics(s)
		if err != nil || got.Version != 2 || got.Font != "fixed" || got.MenuItemPad != row.item || got.MenuTitlePad != row.title || got.FormPad != row.form || got.MenuBorder != 3 {
			t.Fatal(row, got, err)
		}
	}
	for key, value := range map[string]any{"desktop_font": true, "desktop_density": "wide", "desktop_border": "rounded"} {
		s := Defaults()
		s[key] = value
		if _, err := ResolveDesktopMetrics(s); err == nil {
			t.Fatal(key, value)
		}
	}
}

func TestDesktopMetricsOldSchemasAndSeparateEditor(t *testing.T) {
	for version := 1; version <= 11; version++ {
		state := versionedFixture(version)
		data, _ := json.Marshal(state)
		got, err := DecodeState(data)
		if err != nil || got["desktop_font"] != "" || got["desktop_density"] != "inherit" || got["desktop_border"] != "inherit" {
			t.Fatal(version, got, err)
		}
		if version < 10 {
			delete(state, "palette")
			state["desktop_font"] = "fixed"
			data, _ = json.Marshal(state)
			if _, err := DecodeState(data); err == nil {
				t.Fatal("future key displaced old field", version)
			}
		}
	}
	s := snapshotFixture()
	plan, err := Editor(s, "group:desktop", false, nil, strings.Repeat("c", 32))
	if err != nil || !reflect.DeepEqual(plan.Keys, []string{"desktop_font", "desktop_density", "desktop_border", "desktop_icon_theme"}) {
		t.Fatal(plan, err)
	}
	text := strings.Join(plan.Lines, "\n")
	if !strings.Contains(text, "InputHex desktop_font 24 x") || !strings.Contains(text, "x$(desktop_font:Base64)") || strings.Contains(text, "Preview one setting") {
		t.Fatal(text)
	}
	appearance, err := Editor(s, "all", false, nil, strings.Repeat("c", 32))
	if err != nil || len(appearance.Keys) != 11 || !strings.Contains(strings.Join(appearance.Lines, "\n"), "edit-settings group:desktop") {
		t.Fatal(appearance, err)
	}
	for _, key := range plan.Keys {
		if PreviewKey(key) {
			t.Fatal("unqualified reversible domain", key)
		}
	}
}

func TestDesktopMetricsOverrideAndApplicationFacts(t *testing.T) {
	s := snapshotFixture()
	s.Overrides = Values{"desktop_density": "spacious"}
	prepared, err := Prepare(s, Values{"desktop_density": "compact", "desktop_font": "fixed"}, 720)
	if err != nil {
		t.Fatal(err)
	}
	s.Saved, s.Effective = prepared["candidate"].(map[string]any), prepared["effective"].(map[string]any)
	if s.Saved["desktop_density"] != "compact" || s.Effective["desktop_density"] != "spacious" {
		t.Fatal(prepared)
	}
	s.Submitted = true // Old color publication does not attest new metrics.
	q, err := Query(s, "group:desktop", true)
	if err != nil {
		t.Fatal(err)
	}
	item := q["desktop_density"].(map[string]any)
	if item["application"] != "next-owned-form-start; launcher-not-submitted" || item["source"] != "module-configuration" {
		t.Fatal(item)
	}
	s.DesktopSubmitted = true
	q, err = Query(s, "desktop_font", true)
	if err != nil || q["desktop_font"].(map[string]any)["application"] != "next-owned-form-start; launcher-submitted" {
		t.Fatal(q, err)
	}
}

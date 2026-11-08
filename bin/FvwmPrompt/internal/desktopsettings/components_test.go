// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import (
	"bytes"
	"encoding/json"
	"reflect"
	"strings"
	"testing"
)

func TestComponentSchemasPreserveExactOlderFields(t *testing.T) {
	newDefaults := Values{"desktop_style": "inherit", "launcher_font": "",
		"launcher_title_font": "", "form_font": "", "form_input_font": "",
		"form_button_font": "", "launcher_density": "inherit", "form_density": "inherit"}
	for version := 1; version < StateVersion; version++ {
		old := versionedFixture(version)
		data, _ := json.Marshal(old)
		before := append([]byte(nil), data...)
		got, err := DecodeState(data)
		if err != nil || !bytes.Equal(data, before) {
			t.Fatal(version, got, err)
		}
		for key, want := range newDefaults {
			if got[key] != want {
				t.Fatal(version, key, got[key])
			}
			// A new field may not masquerade as a missing historical field,
			// even when its value equals the new schema's default.
			bad := versionedFixture(version)
			delete(bad, "motion")
			bad[key] = want
			data, _ = json.Marshal(bad)
			if _, err := DecodeState(data); err == nil {
				t.Fatal("future field accepted in historical schema", version, key)
			}
		}
	}
	for key := range newDefaults {
		current := versionedFixture(StateVersion)
		delete(current, key)
		data, _ := json.Marshal(current)
		if _, err := DecodeState(data); err == nil {
			t.Fatal("new schema supplied a missing default", key)
		}
	}
}

func TestComponentCurrentStateRoundTrip(t *testing.T) {
	state := versionedFixture(StateVersion)
	changes := Values{"desktop_style": "relief", "launcher_font": "Launcher 18",
		"launcher_title_font": "Title 20", "form_font": "Body 18",
		"form_input_font": "Input 18", "form_button_font": "Button 18",
		"launcher_density": "spacious", "form_density": "compact"}
	for key, value := range changes {
		state[key] = value
	}
	data, _ := json.Marshal(state)
	got, err := DecodeState(data)
	if err != nil {
		t.Fatal(err)
	}
	for key, want := range changes {
		if got[key] != want {
			t.Fatal(key, got[key], want)
		}
	}
	delete(state, "schema_version")
	if !reflect.DeepEqual(got, state) {
		t.Fatal("current state fields changed during decode", got, state)
	}
}

func TestComponentIndependentFontsAndDensityFallback(t *testing.T) {
	s := Defaults()
	baseline, err := ResolveDesktopMetrics(s)
	if err != nil || baseline.Version != 2 || baseline.LauncherFont != "" ||
		baseline.LauncherTitleFont != "" || baseline.FormFont != "" ||
		baseline.FormInputFont != "" || baseline.FormButtonFont != "" ||
		baseline.LauncherDensity != "inherit" || baseline.FormDensity != "inherit" {
		t.Fatal(baseline, err)
	}
	s["desktop_font"], s["desktop_density"] = "fixed", "compact"
	s["launcher_title_font"], s["form_input_font"] = "xft:Sans:size=18", "xft:Mono:size=16"
	s["launcher_density"] = "spacious"
	got, err := ResolveDesktopMetrics(s)
	if err != nil || got.Font != "fixed" || got.Density != "compact" ||
		got.LauncherFont != "fixed" || got.LauncherTitleFont != "xft:Sans:size=18" ||
		got.FormFont != "fixed" || got.FormInputFont != "xft:Mono:size=16" ||
		got.FormButtonFont != "fixed" || got.LauncherDensity != "spacious" ||
		got.FormDensity != "compact" || got.MenuItemPad != 12 || got.MenuTitlePad != 14 || got.FormPad != 6 {
		t.Fatal(got, err)
	}
	// Every font is independently addressable; setting one cannot replace
	// siblings, the global fallback or the unrelated application font.
	for _, key := range []string{"launcher_font", "launcher_title_font", "form_font", "form_input_font", "form_button_font"} {
		s := Defaults()
		s["desktop_font"], s["application_font"], s[key] = "fixed", "Other 11", "xft:Sans:size=18"
		got, err := ResolveDesktopMetrics(s)
		if err != nil {
			t.Fatal(err)
		}
		fonts := map[string]string{"launcher_font": got.LauncherFont, "launcher_title_font": got.LauncherTitleFont,
			"form_font": got.FormFont, "form_input_font": got.FormInputFont, "form_button_font": got.FormButtonFont}
		for sibling, font := range fonts {
			want := "fixed"
			if sibling == key {
				want = "xft:Sans:size=18"
			}
			if font != want {
				t.Fatal(key, sibling, font)
			}
		}
		if s["desktop_font"] != "fixed" || s["application_font"] != "Other 11" {
			t.Fatal("resolution modified inputs", s)
		}
	}
	for _, row := range []struct {
		launcher, form   string
		item, title, pad int
	}{{"compact", "spacious", 4, 6, 16}, {"comfortable", "compact", 8, 10, 6}, {"inherit", "comfortable", 8, 10, 12}} {
		s := Defaults()
		s["launcher_density"], s["form_density"] = row.launcher, row.form
		got, err := ResolveDesktopMetrics(s)
		if err != nil || got.MenuItemPad != row.item || got.MenuTitlePad != row.title || got.FormPad != row.pad {
			t.Fatal(row, got, err)
		}
	}
}

func TestComponentNativeStyleHasFixedCorners(t *testing.T) {
	for _, row := range []struct {
		style           string
		relief          int
		long, triangles bool
	}{{"inherit", 0, false, true}, {"flat", 0, true, false}, {"relief", 2, false, true}} {
		s := Defaults()
		s["desktop_style"], s["desktop_border"] = row.style, "strong"
		got, err := ResolveDesktopMetrics(s)
		if err != nil || got.Style != row.style || got.MenuRelief != row.relief ||
			got.LongSeparators != row.long || got.ReliefTriangles != row.triangles ||
			got.CornerRadius != 0 || got.MenuBorder != 3 {
			t.Fatal(row, got, err)
		}
	}
	for key, bad := range (Values{"desktop_style": "rounded", "launcher_density": "wide", "form_density": true}) {
		s := Defaults()
		s[key] = bad
		if _, err := ResolveDesktopMetrics(s); err == nil {
			t.Fatal(key, bad)
		}
	}
}

func TestComponentFontsRejectUnrepresentableCommands(t *testing.T) {
	for _, key := range []string{"launcher_font", "launcher_title_font", "form_font", "form_input_font", "form_button_font"} {
		for _, font := range []string{"", "fixed", "xft:Sans:size=18", "字体 12", strings.Repeat("x", 128)} {
			if got, err := Value(key, font, false); err != nil || got != font {
				t.Fatal(key, got, err)
			}
		}
		for _, font := range []string{" fixed", "fixed ", "a\nb", "a\tb", "a\x00b", "\xff", "\u2028", "\u0085", `a"b`, `a\b`, "$font", "%n", "a&b", strings.Repeat("x", 129)} {
			if _, err := Value(key, font, false); err == nil {
				t.Fatal("unrepresentable component font accepted", key, font)
			}
		}
	}
}

func TestComponentMetadataFallbackAndOverrideOwnership(t *testing.T) {
	s := snapshotFixture()
	s.Saved["desktop_font"], s.Saved["launcher_font"] = "saved-global", "saved-launcher"
	s.Overrides = Values{"desktop_font": "module-global", "launcher_title_font": "module-title", "form_density": "compact"}
	prepared, err := Prepare(s, Values{"form_input_font": "saved-input"}, 720)
	if err != nil {
		t.Fatal(err)
	}
	s.Saved, s.Effective = prepared["candidate"].(map[string]any), prepared["effective"].(map[string]any)
	s.Submitted, s.DesktopSubmitted = true, true
	query, err := Query(s, "group:desktop", true)
	if err != nil || len(query) != 12 {
		t.Fatal(query, err)
	}
	for _, row := range []struct{ key, resolved, resolution, source string }{
		{"launcher_font", "saved-launcher", "explicit", "saved"},
		{"launcher_title_font", "module-title", "explicit", "module-configuration"},
		{"form_font", "module-global", "desktop-default", "saved"},
		{"form_input_font", "saved-input", "explicit", "saved"},
		{"form_button_font", "module-global", "desktop-default", "saved"},
		{"launcher_density", "configured-profile", "profile-baseline", "saved"},
		{"form_density", "compact", "explicit", "module-configuration"},
		{"desktop_style", "configured-profile", "profile-baseline", "saved"},
	} {
		item := query[row.key].(map[string]any)
		if item["resolved"] != row.resolved || item["resolution"] != row.resolution || item["source"] != row.source ||
			item["component_version"] != 2 || item["application"] != "next-owned-form-start; launcher-submitted" ||
			item["runtime_constraint"] != "save-apply-only; existing-forms-unchanged" {
			t.Fatal(row, item)
		}
	}
	common := query["desktop_font"].(map[string]any)["component_roles"].(map[string]any)
	for key, raw := range query {
		if key == "desktop_icon_theme" {
			continue
		}
		if !reflect.DeepEqual(raw.(map[string]any)["component_roles"], common) {
			t.Fatal("roles no longer share a factorizable tuple", key, raw)
		}
	}
	if common["launcher_font"] != "saved-launcher" || common["launcher_title_font"] != "module-title" ||
		common["form_font"] != "module-global" || common["form_input_font"] != "saved-input" ||
		common["form_button_font"] != "module-global" || common["form_density"] != "compact" ||
		common["style"] != "inherit" || common["corner_radius"] != 0 {
		t.Fatal(common)
	}
	common["form_font"] = "caller mutation"
	if query["form_font"].(map[string]any)["component_roles"].(map[string]any)["form_font"] != "module-global" ||
		s.Effective["desktop_font"] != "module-global" {
		t.Fatal("borrowed metric roles escaped", query)
	}
	// No previously submitted colors or metrics claim existing Form fonts or
	// observed launcher paint. Unobserved queries retain submission uncertainty.
	unobserved, err := Query(s, "form_font", false)
	if err != nil || unobserved["form_font"].(map[string]any)["application"] != "next-owned-form-start; launcher-not-submitted" {
		t.Fatal(unobserved, err)
	}
}

func TestComponentCompactEditorScopesAndDefaults(t *testing.T) {
	pages := map[string][]string{
		"component:defaults": {"desktop_font", "desktop_density", "desktop_border", "desktop_icon_theme"},
		"component:launcher": {"desktop_style", "launcher_font", "launcher_title_font", "launcher_density"},
		"component:forms":    {"form_font", "form_input_font", "form_button_font", "form_density"},
	}
	s := snapshotFixture()
	s.Saved["launcher_font"], s.Saved["form_input_font"] = "xft:Sans:size=18", "xft:Mono:size=18"
	s.Saved["launcher_density"], s.Saved["form_density"] = "spacious", "compact"
	for scope, want := range pages {
		keys, err := Scope(scope)
		if err != nil || !reflect.DeepEqual(keys, want) {
			t.Fatal(scope, keys, err)
		}
		keys[0] = "caller mutation"
		plan, err := Editor(s, scope, true, nil, strings.Repeat("c", 32))
		if err != nil || !reflect.DeepEqual(plan.Keys, want) {
			t.Fatal(scope, plan, err)
		}
		rows := 0
		for _, line := range plan.Lines {
			if strings.HasPrefix(line, "Line ") {
				rows++
			}
		}
		// Bound vertical growth before native enlarged-text inspection: four
		// controls and at most thirteen layout rows, never all twelve axes.
		if rows > 13 {
			t.Fatal("component editor grew beyond compact enlarged-text plan", scope, rows)
		}
		joined := strings.Join(plan.Lines, "\n")
		for _, key := range want {
			item := Registry()[key]
			if item.Type == "desktop-font" || item.Type == "theme" {
				if !strings.Contains(joined, "InputHex "+key+" 24 x\n") {
					t.Fatal("defaults retained an edited font/theme", scope, key, joined)
				}
			} else if !strings.Contains(joined, "Choice "+key+"_inherit inherit on") {
				t.Fatal("defaults did not select inheritance", scope, key, joined)
			}
		}
		for other := range pages {
			if strings.Contains(joined, "Command SendToModule FvwmDesktop edit-settings "+other) != (other != scope) {
				t.Fatal("component navigation missing or self-repeating", scope, other, joined)
			}
		}
		changes := Values{}
		for _, key := range want {
			changes[key] = Registry()[key].Default
		}
		prepared, err := Prepare(s, changes, 720)
		if err != nil {
			t.Fatal(err)
		}
		candidate := prepared["candidate"].(map[string]any)
		for key := range Registry() {
			saved := s.Saved[key]
			if !has(want, key) && !reflect.DeepEqual(candidate[key], saved) {
				t.Fatal("scoped defaults changed hidden axes", scope, key, candidate[key], saved)
			}
		}
	}
	group, err := Scope("group:desktop")
	if err != nil || len(group) != 12 {
		t.Fatal(group, err)
	}
	plan, err := Editor(s, "group:desktop", false, nil, strings.Repeat("c", 32))
	if err != nil || !reflect.DeepEqual(plan.Keys, pages["component:defaults"]) {
		t.Fatal(plan, err)
	}
}

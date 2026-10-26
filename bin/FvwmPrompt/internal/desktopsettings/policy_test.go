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

func TestRegistryDomainsAndIndependentDefaults(t *testing.T) {
	keys, _ := Scope("all")
	if !reflect.DeepEqual(keys, []string{"palette", "appearance", "day", "night", "transparency", "motion", "keep_wallpaper", "launcher_opacity", "wallpaper_palette", "wallpaper_appearance", "contrast"}) {
		t.Fatal(keys)
	}
	for key, item := range Registry() {
		if _, err := Value(key, item.Default, false); err != nil {
			t.Fatal(key, err)
		}
	}
	r := Registry()
	r["palette"].Choices[0] = "changed"
	d := Defaults()
	d["wallpaper"].(map[string]any)["palette"] = "changed"
	if Registry()["palette"].Choices[0] != "slate" || Defaults()["wallpaper"].(map[string]any)["palette"] != "slate" {
		t.Fatal("caller mutated policy")
	}
	for _, scope := range []string{"", "group:unknown", "all;Exec bad"} {
		if _, err := Scope(scope); err == nil {
			t.Fatal(scope)
		}
	}
}
func TestStrictValuesAndTextOnlyConversion(t *testing.T) {
	bad := map[string][]any{"contrast": {"HIGH", "high;Exec bad", true, nil}, "wallpaper_palette": {"auto", "SLATE", "slate;Exec bad", true, nil}, "wallpaper_appearance": {"auto", "LIGHT", "light;Exec bad", true, nil}, "palette": {"SLATE", "slate; Exec bad", true, nil}, "motion": {"true", 1, json.Number("1"), nil}, "launcher_opacity": {true, -1, 101, "94", json.Number("94.0"), json.Number("9e1")}, "day": {"7:00", "24:00", "10:60", "07:00\n"}}
	for key, values := range bad {
		for _, v := range values {
			if _, err := Value(key, v, false); err == nil {
				t.Errorf("%s accepted %#v", key, v)
			}
		}
	}
	for _, pair := range []struct {
		key             string
		input, expected any
	}{{"motion", "ON", true}, {"motion", "no", false}, {"launcher_opacity", "000", 0}, {"launcher_opacity", "100", 100}, {"day", "23:59", "23:59"}} {
		got, err := Value(pair.key, pair.input, true)
		if err != nil || got != pair.expected {
			t.Fatalf("%+v %v %v", pair, got, err)
		}
	}
	for _, v := range []any{"+1", "1.0", " 94", "1000", "９４"} {
		if _, err := Value("launcher_opacity", v, true); err == nil {
			t.Fatal(v)
		}
	}
}
func TestSchedulesAndMigration(t *testing.T) {
	for _, pair := range []struct {
		minute   int
		expected string
	}{{419, "dark"}, {420, "light"}, {1139, "light"}, {1140, "dark"}, {0, "dark"}} {
		mode, err := Appearance(Defaults(), pair.minute)
		if err != nil || mode != pair.expected {
			t.Fatal(pair, mode, err)
		}
	}
	d := Defaults()
	d["day"], d["night"] = "19:00", "07:00"
	for _, pair := range []struct {
		minute   int
		expected string
	}{{1380, "light"}, {480, "dark"}} {
		got, _ := Appearance(d, pair.minute)
		if got != pair.expected {
			t.Fatal(pair, got)
		}
	}
	migrated := Migrate(Values{"palette": "invalid", "motion": "false", "transparency": false, "day": "08:00", "night": "08:00", "extra": true})
	want := Defaults()
	want["transparency"] = false
	if !reflect.DeepEqual(migrated, want) {
		t.Fatal(migrated)
	}
	d["night"] = d["day"]
	if _, err := Checked(d); err == nil {
		t.Fatal("equal schedule")
	}
	if _, err := Appearance(Defaults(), 1440); err == nil {
		t.Fatal("minute overflow")
	}
}
func TestStoredSchemaStrictnessAndDuplicateRejection(t *testing.T) {
	d := Defaults()
	delete(d, "launcher_opacity")
	delete(d, "wallpaper_palette")
	delete(d, "wallpaper_appearance")
	delete(d, "contrast")
	d["schema_version"] = 1
	data, _ := json.Marshal(d)
	got, err := DecodeState(data)
	if err != nil || got["launcher_opacity"] != 94 {
		t.Fatal(got, err)
	}
	d["schema_version"] = 2
	data, _ = json.Marshal(d)
	if _, err := DecodeState(data); err == nil {
		t.Fatal("missing current field")
	}
	for _, raw := range []string{`[]`, `null`, `{"schema_version":true}`, `{"schema_version":1.0}`, `{"palette":"slate","palette":"ink"}`, `{"wallpaper":{"palette":"slate","palette":"ink"}}`, `{} {}`, strings.Repeat(" ", StateLimit+1), "{\"value\":\"\xff\"}"} {
		if _, err := DecodeState([]byte(raw)); err == nil {
			t.Fatalf("accepted %q", raw)
		}
	}
	if _, err := DecodeJSON([]byte(strings.Repeat("[", 18) + "0" + strings.Repeat("]", 18))); err == nil {
		t.Fatal("deep nesting")
	}
}
func snapshotFixture() Snapshot {
	return Snapshot{Saved: Defaults(), Effective: Defaults(), Overrides: Values{}, Generation: strings.Repeat("a", 32), Instance: strings.Repeat("b", 32)}
}
func TestCandidateAndQueryAreNonPublishing(t *testing.T) {
	s := snapshotFixture()
	s.Overrides["palette"] = "dune"
	s.Effective["palette"] = "dune"
	s.Effective["wallpaper"] = Values{"palette": "ink", "appearance": "light"}
	s.Saved["keep_wallpaper"] = true
	result, err := Prepare(s, Values{"motion": true}, 480)
	if err != nil {
		t.Fatal(err)
	}
	candidate := result["candidate"].(map[string]any)
	effective := result["effective"].(map[string]any)
	if candidate["palette"] != "slate" || effective["palette"] != "dune" || candidate["wallpaper"].(map[string]any)["palette"] != "ink" || s.Saved["motion"] != false {
		t.Fatal(result)
	}
	q, err := Query(s, "palette", false)
	if err != nil {
		t.Fatal(err)
	}
	item := q["palette"].(map[string]any)
	if item["saved"] != "slate" || item["effective"] != "dune" || item["source"] != "module-configuration" || item["application"] != "not-submitted" {
		t.Fatal(item)
	}
	candidate["wallpaper"].(map[string]any)["palette"] = "changed"
	if effective["wallpaper"].(map[string]any)["palette"] != "ink" || s.Effective["wallpaper"].(map[string]any)["palette"] != "ink" {
		t.Fatal("borrowed wallpaper escaped")
	}
	item["choices"].([]string)[0] = "changed"
	if Registry()["palette"].Choices[0] != "slate" {
		t.Fatal("borrowed choices escaped")
	}
	s.Submitted = true
	q, _ = Query(s, "palette", true)
	if q["palette"].(map[string]any)["application"] != "submitted-to-fvwm" {
		t.Fatal(q)
	}
	if _, err := Prepare(s, Values{"wallpaper": Values{}}, 480); err == nil {
		t.Fatal("internal field mutation")
	}
}
func TestEditorAndMenuPlansKeepScopedCallbacks(t *testing.T) {
	s := snapshotFixture()
	s.Overrides["motion"] = true
	s.Effective["motion"] = true
	token := strings.Repeat("c", 32)
	plan, err := Editor(s, "group:effects", false, nil, token)
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(plan.Keys, []string{"transparency", "motion", "launcher_opacity"}) {
		t.Fatal(plan.Keys)
	}
	lines := strings.Join(plan.Lines, "\n")
	for _, required := range []string{"Input launcher_opacity 3 \"94\"", "Command SendToModule FvwmDesktop apply-settings " + token + " $(transparency) $(motion) $(launcher_opacity)", "Timeout 3000", "Preview opacity (F4)", "Effective request: true (module configuration override)"} {
		if !strings.Contains(lines, required) {
			t.Fatalf("missing %q", required)
		}
	}
	if _, err := Editor(s, "all", false, nil, "bad;Exec"); err == nil {
		t.Fatal("callback injection")
	}
	if _, err := Editor(s, "motion", false, Values{"palette": "ink"}, token); err == nil {
		t.Fatal("wrong draft scope")
	}
	menus, err := Menus(s, "launcher_opacity")
	if err != nil || len(menus) != 3 {
		t.Fatal(menus, err)
	}
	if menus[0].Name != "SlateSetting_launcher_opacity" || menus[2].Name != "SlateSettings" {
		t.Fatal(menus)
	}
	for _, menu := range menus {
		for _, row := range menu.Rows {
			if strings.ContainsAny(row[1], "\r\n\x00") {
				t.Fatal(row)
			}
		}
	}
}
func TestScheduleAppearanceAndPreviewPlans(t *testing.T) {
	s := Defaults()
	menu := AppearanceMenu(s)
	if menu.Name != "SlateAppearance" || menu.Rows[0][0] != "* Slate" {
		t.Fatal(menu)
	}
	lines := strings.Join(Schedule(s), "\n")
	if !strings.Contains(lines, "Command SendToModule FvwmDesktop times $(day) $(night)") {
		t.Fatal(lines)
	}
	for _, saved := range []bool{false, true} {
		lines, err := Preview(strings.Repeat("a", 32), strings.Repeat("b", 16), 73, saved)
		if err != nil {
			t.Fatal(err)
		}
		joined := strings.Join(lines, "\n")
		if strings.Contains(joined, " save") != saved || !strings.HasSuffix(joined, `Timeout 160 Nop ""`) {
			t.Fatal(joined)
		}
	}
	if _, err := Preview(strings.Repeat("a", 32), "bad;Exec", 73, true); err == nil {
		t.Fatal("invalid view")
	}
	if _, err := Preview(strings.Repeat("a", 32), strings.Repeat("b", 16), 101, false); err == nil {
		t.Fatal("invalid opacity")
	}
}

func TestParallelPolicyResultsDoNotShareMutableState(t *testing.T) {
	for i := 0; i < 8; i++ {
		t.Run(string(rune('a'+i)), func(t *testing.T) {
			t.Parallel()
			for j := 0; j < 20; j++ {
				s := snapshotFixture()
				q, err := Query(s, "all", true)
				if err != nil {
					t.Fatal(err)
				}
				q["palette"].(map[string]any)["choices"].([]string)[0] = "changed"
				q["wallpaper_palette"].(map[string]any)["choices"].([]string)[0] = "changed"
				r := Registry()
				if r["palette"].Choices[0] != "slate" || r["wallpaper_palette"].Choices[0] != "follow" {
					t.Fatal("shared registry")
				}
			}
		})
	}
}

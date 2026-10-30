// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import (
	"bytes"
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func shippedPalettes(t *testing.T) map[string]map[string]map[string]string {
	t.Helper()
	data, err := os.ReadFile(filepath.Join("..", "..", "..", "..", "default-config", "palettes.json"))
	if err != nil {
		t.Fatal(err)
	}
	var palettes map[string]map[string]map[string]string
	if err := json.Unmarshal(data, &palettes); err != nil {
		t.Fatal(err)
	}
	return palettes
}

func TestThemeProjectionPreservesEveryShippedColorset(t *testing.T) {
	palettes := shippedPalettes(t)
	for _, name := range []string{"slate", "dune", "grove", "ink"} {
		for _, appearance := range []string{"light", "dark"} {
			t.Run(name+"/"+appearance, func(t *testing.T) {
				colors := palettes[name][appearance]
				projection, err := ProjectTheme(colors, appearance, "normal")
				if err != nil {
					t.Fatal(err)
				}
				// Pin observable legacy values, including the skipped colorset 9
				// and identical background/highlight/shadow.
				want := []struct {
					id           int
					name, fg, bg string
				}{
					{0, "surface", "text", "surface"},
					{1, "muted", "muted", "surface"},
					{2, "raised", "text", "raised"},
					{3, "border", "border", "border"},
					{4, "accent", "accent", "accent"},
					{5, "menu.normal", "text", "surface"},
					{6, "menu.active", "on_accent", "accent"},
					{7, "menu.disabled", "muted", "surface"},
					{8, "menu.accent", "accent", "surface"},
					{10, "panel.normal", "text", "surface"},
					{11, "panel.active", "on_accent", "accent"},
					{12, "panel.raised", "text", "raised"},
					{13, "panel.selected", "on_accent", "accent"},
					{14, "panel.muted", "muted", "surface"},
				}
				if projection.Version != 2 || len(projection.Roles) != len(want) {
					t.Fatal(projection)
				}
				for i, expected := range want {
					role := projection.Roles[i]
					if role.Name != expected.name || role.Colorset != expected.id ||
						role.Foreground != colors[expected.fg] || role.Background != colors[expected.bg] ||
						role.Highlight != colors[expected.bg] || role.Shadow != colors[expected.bg] {
						t.Fatalf("role %d: %#v", i, role)
					}
				}
				original := projection.Roles[0].Foreground
				colors["text"] = "#abcdef"
				if projection.Roles[0].Foreground != original {
					t.Fatal("projection borrowed palette")
				}
				second, _ := ProjectTheme(colors, appearance, "normal")
				projection.Roles[0].Name = "changed"
				if second.Roles[0].Name != "surface" {
					t.Fatal("shared projection roles")
				}
			})
		}
	}
}

func TestThemeProjectionRejectsMalformedColorsBeforeReturningRoles(t *testing.T) {
	colors := shippedPalettes(t)["slate"]["dark"]
	for _, key := range []string{"background", "surface", "raised", "text", "muted", "border", "accent", "on_accent"} {
		for _, invalid := range []string{"", "red", "#123", "#1234567", "#abcdeg", "#12345\n", "#１２３４５６", "#123456;Exec bad"} {
			copy := map[string]string{}
			for k, v := range colors {
				copy[k] = v
			}
			copy[key] = invalid
			projection, err := ProjectTheme(copy, "dark", "normal")
			if err == nil || projection.Version != 0 || projection.Roles != nil {
				t.Fatalf("accepted %s=%q: %#v, %v", key, invalid, projection, err)
			}
		}
	}
	if projection, err := ProjectTheme(nil, "dark", "normal"); err == nil || projection.Roles != nil {
		t.Fatal(projection, err)
	}
	for key := range colors {
		colors[key] = "#aBcD09"
	}
	if _, err := ProjectTheme(colors, "dark", "normal"); err != nil {
		t.Fatal(err)
	}
}

func TestWallpaperResolutionPrecedenceAndSchedule(t *testing.T) {
	for _, keep := range []bool{false, true} {
		for _, palette := range []string{"follow", "slate", "dune", "grove", "ink"} {
			for _, appearance := range []string{"follow", "light", "dark"} {
				for _, uiMode := range []string{"auto", "light", "dark"} {
					for _, schedule := range [][2]string{{"07:00", "19:00"}, {"19:00", "07:00"}} {
						for _, minute := range []int{0, 419, 420, 1139, 1140, 1439} {
							settings := Defaults()
							settings["palette"], settings["appearance"] = "grove", uiMode
							settings["day"], settings["night"] = schedule[0], schedule[1]
							settings["wallpaper_palette"], settings["wallpaper_appearance"] = palette, appearance
							settings["keep_wallpaper"] = keep
							settings["wallpaper"] = Values{"palette": "ink", "appearance": "light"}
							before, _ := json.Marshal(settings)
							got, err := ResolveWallpaper(settings, minute)
							if err != nil {
								t.Fatal(err)
							}
							wantPalette, wantMode := palette, appearance
							if palette == "follow" {
								wantPalette = "grove"
							}
							if appearance == "follow" {
								wantMode = uiMode
								if uiMode == "auto" {
									light := minute >= 420 && minute < 1140
									if schedule[0] == "19:00" {
										light = !light
									}
									wantMode = "dark"
									if light {
										wantMode = "light"
									}
								}
							}
							if keep {
								wantPalette, wantMode = "ink", "light"
							}
							if !reflect.DeepEqual(got, Values{"palette": wantPalette, "appearance": wantMode}) {
								t.Fatalf("%#v, %d: %#v want %s/%s", settings, minute, got, wantPalette, wantMode)
							}
							got["palette"] = "changed"
							after, _ := json.Marshal(settings)
							if !bytes.Equal(before, after) {
								t.Fatal("wallpaper resolution mutated borrowed settings")
							}
						}
					}
				}
			}
		}
	}
}

func TestWallpaperResolutionRejectsMalformedSettings(t *testing.T) {
	for _, change := range []Values{
		{"palette": "unknown"}, {"appearance": "unknown"}, {"keep_wallpaper": "true"},
		{"wallpaper_palette": "unknown"}, {"wallpaper_appearance": "auto"},
		{"day": "24:00"}, {"night": "07:00"}, {"wallpaper": nil},
		{"wallpaper": Values{"palette": "ink"}},
		{"wallpaper": Values{"palette": "ink", "appearance": "auto"}},
		{"wallpaper": Values{"palette": "ink", "appearance": "light", "extra": true}},
		{"extra": true},
	} {
		for _, keep := range []bool{false, true} {
			settings := Defaults()
			settings["keep_wallpaper"] = keep
			for key, value := range change {
				settings[key] = value
			}
			if got, err := ResolveWallpaper(settings, 480); err == nil || got != nil {
				t.Fatalf("accepted %#v: %#v, %v", settings, got, err)
			}
		}
	}
	for _, minute := range []int{-1, 1440} {
		if got, err := ResolveWallpaper(Defaults(), minute); err == nil || got != nil {
			t.Fatal(minute, got, err)
		}
	}
}

func versionedFixture(version int) Values {
	settings := Defaults()
	settings["schema_version"] = version
	settings["palette"], settings["appearance"] = "dune", "light"
	settings["keep_wallpaper"] = true
	settings["wallpaper"] = Values{"palette": "ink", "appearance": "dark"}
	if version < 5 {
		delete(settings, "session_bus")
		delete(settings, "session_notifications")
		delete(settings, "notification_dnd")
	}
	if version < 4 {
		delete(settings, "contrast")
	}
	if version < 3 {
		delete(settings, "wallpaper_palette")
		delete(settings, "wallpaper_appearance")
	}
	if version == 1 {
		delete(settings, "launcher_opacity")
	} else {
		settings["launcher_opacity"] = 73
	}
	return settings
}

func TestExactStoredSchemasAndMigration(t *testing.T) {
	if StateVersion != 5 {
		t.Fatal(StateVersion)
	}
	for _, version := range []int{1, 2, 3, 4, 5} {
		settings := versionedFixture(version)
		if len(settings) != map[int]int{1: 9, 2: 10, 3: 12, 4: 13, 5: 16}[version] {
			t.Fatal(settings)
		}
		data, _ := json.Marshal(settings)
		before := append([]byte(nil), data...)
		got, err := DecodeState(data)
		if err != nil || got["palette"] != "dune" || got["appearance"] != "light" || got["keep_wallpaper"] != true ||
			got["wallpaper_palette"] != "follow" || got["wallpaper_appearance"] != "follow" || got["contrast"] != "normal" ||
			!reflect.DeepEqual(got["wallpaper"], Values{"palette": "ink", "appearance": "dark"}) {
			t.Fatal(version, got, err)
		}
		wantOpacity := 73
		if version == 1 {
			wantOpacity = 94
		}
		if got["launcher_opacity"] != wantOpacity || !bytes.Equal(before, data) {
			t.Fatal(got)
		}
		for _, key := range []string{"palette", "wallpaper", "motion"} {
			bad := versionedFixture(version)
			delete(bad, key)
			bad["unknown"] = true
			data, _ := json.Marshal(bad)
			if got, err := DecodeState(data); err == nil || got != nil {
				t.Fatal(version, key, got, err)
			}
		}
		for _, change := range []Values{{"unknown": true}, {"palette": "unknown"}, {"day": "07:00", "night": "07:00"}, {"wallpaper": Values{"palette": "ink", "appearance": "auto"}}} {
			bad := versionedFixture(version)
			for key, value := range change {
				bad[key] = value
			}
			data, _ := json.Marshal(bad)
			if got, err := DecodeState(data); err == nil || got != nil {
				t.Fatal(version, change, got, err)
			}
		}
	}
	current := versionedFixture(StateVersion)
	current["wallpaper_palette"], current["wallpaper_appearance"] = "grove", "light"
	data, _ := json.Marshal(current)
	got, err := DecodeState(data)
	if err != nil || got["wallpaper_palette"] != "grove" || got["wallpaper_appearance"] != "light" {
		t.Fatal(got, err)
	}
	for _, version := range []any{0, 6, -1, true, "4", 4.5, nil} {
		bad := versionedFixture(3)
		bad["schema_version"] = version
		data, _ := json.Marshal(bad)
		if got, err := DecodeState(data); err == nil || got != nil {
			t.Fatal(version, got, err)
		}
	}
	for _, version := range []int{1, 2, 3} {
		for _, key := range []string{"contrast", "wallpaper_palette", "wallpaper_appearance", "launcher_opacity"} {
			if version >= 2 && key == "launcher_opacity" || version >= 3 && strings.HasPrefix(key, "wallpaper_") {
				continue
			}
			bad := versionedFixture(version)
			delete(bad, "motion")
			bad[key] = "follow"
			data, _ := json.Marshal(bad)
			if got, err := DecodeState(data); err == nil || got != nil {
				t.Fatal(version, key, got, err)
			}
		}
	}
}

func TestWallpaperPrepareCopiesCurrentRetentionAndAppliesOverrides(t *testing.T) {
	s := snapshotFixture()
	s.Effective["wallpaper"] = Values{"palette": "ink", "appearance": "light"}
	s.Overrides["wallpaper_palette"] = "grove"
	s.Overrides["wallpaper_appearance"] = "dark"
	result, err := Prepare(s, Values{"wallpaper_palette": "dune", "keep_wallpaper": true}, 480)
	if err != nil {
		t.Fatal(err)
	}
	candidate, effective := result["candidate"].(map[string]any), result["effective"].(map[string]any)
	if candidate["wallpaper_palette"] != "dune" || effective["wallpaper_palette"] != "grove" ||
		!reflect.DeepEqual(candidate["wallpaper"], s.Effective["wallpaper"]) {
		t.Fatal(result)
	}
	candidate["wallpaper"].(map[string]any)["palette"] = "changed"
	if effective["wallpaper"].(map[string]any)["palette"] != "ink" || s.Effective["wallpaper"].(map[string]any)["palette"] != "ink" {
		t.Fatal("borrowed retained pair")
	}
	result, err = Prepare(s, Values{"wallpaper_palette": "dune", "keep_wallpaper": false}, 480)
	if err != nil {
		t.Fatal(err)
	}
	for _, field := range []string{"candidate", "effective"} {
		if !reflect.DeepEqual(result[field].(map[string]any)["wallpaper"], Values{"palette": "grove", "appearance": "dark"}) {
			t.Fatal(result)
		}
	}
	for _, keep := range []bool{false, true} {
		s.Overrides["keep_wallpaper"] = keep
		result, err := Prepare(s, Values{"keep_wallpaper": !keep}, 480)
		if err != nil {
			t.Fatal(err)
		}
		want := Values{"palette": "grove", "appearance": "dark"}
		if keep {
			want = Values{"palette": "ink", "appearance": "light"}
		}
		if !reflect.DeepEqual(result["effective"].(map[string]any)["wallpaper"], want) {
			t.Fatal("retention override precedence", result)
		}
	}
	delete(s.Overrides, "keep_wallpaper")
	for _, bad := range []any{nil, "ink", Values{"palette": "ink"}} {
		s.Effective["wallpaper"] = bad
		if got, err := Prepare(s, Values{"keep_wallpaper": true}, 480); err == nil || got != nil {
			t.Fatal(got, err)
		}
	}
}

func TestWallpaperRegistryEditorAndStableMetadata(t *testing.T) {
	keys, err := Scope("group:wallpaper")
	if err != nil || !reflect.DeepEqual(keys, []string{"wallpaper_palette", "wallpaper_appearance"}) {
		t.Fatal(keys, err)
	}
	appearance, _ := Scope("group:appearance")
	if !reflect.DeepEqual(appearance, []string{"palette", "appearance", "keep_wallpaper", "contrast"}) {
		t.Fatal(appearance)
	}
	s := snapshotFixture()
	plan, err := Editor(s, "group:wallpaper", false, nil, strings.Repeat("c", 32))
	if err != nil || !reflect.DeepEqual(plan.Keys, keys) {
		t.Fatal(plan, err)
	}
	lines := strings.Join(plan.Lines, "\n")
	for _, want := range []string{"Selection wallpaper_palette single", "Choice wallpaper_palette_follow follow on", "Choice wallpaper_appearance_follow follow on", "$(wallpaper_palette) $(wallpaper_appearance)"} {
		if !strings.Contains(lines, want) {
			t.Fatal(want, lines)
		}
	}
	menus, err := Menus(s, "group:wallpaper")
	if err != nil || len(menus) != 4 || menus[0].Name != "SlateSetting_wallpaper_palette" ||
		menus[1].Name != "SlateSetting_wallpaper_appearance" {
		t.Fatal(menus, err)
	}
	for _, pair := range [][2]string{{"wallpaper_palette", "grove"}, {"wallpaper_appearance", "dark"}} {
		found := false
		for _, menu := range menus {
			for _, row := range menu.Rows {
				found = found || row[1] == "SendToModule FvwmDesktop set "+pair[0]+" "+pair[1]
			}
		}
		if !found {
			t.Fatal("missing wallpaper menu action", pair)
		}
	}
	for _, mode := range []string{"auto", "light", "dark"} {
		s.Effective["appearance"] = mode
		q, err := Query(s, "group:wallpaper", true)
		if err != nil {
			t.Fatal(err)
		}
		for _, key := range keys {
			item := q[key].(map[string]any)
			want := any("slate")
			if key == "wallpaper_appearance" {
				want = mode
			}
			if item["saved"] != "follow" || item["effective"] != "follow" || item["resolution"] != "follow" ||
				item["resolved"] != want || item["composition_version"] != 2 || item["generation"] != s.Generation || item["runtime_constraint"] != nil {
				t.Fatal(item)
			}
		}
		s.Effective["wallpaper"] = Values{"palette": "slate", "appearance": "light"}
		second, _ := Query(s, "group:wallpaper", true)
		if !reflect.DeepEqual(q, second) {
			t.Fatal("runtime phase leaked into preference metadata", q, second)
		}
	}
	s.Effective["wallpaper_palette"], s.Effective["wallpaper_appearance"] = "dune", "dark"
	s.Overrides["wallpaper_palette"] = "dune"
	q, _ := Query(s, "group:wallpaper", false)
	palette := q["wallpaper_palette"].(map[string]any)
	if palette["saved"] != "follow" || palette["effective"] != "dune" || palette["resolved"] != "dune" || palette["resolution"] != "explicit" || palette["source"] != "module-configuration" {
		t.Fatal(palette)
	}
	s.Effective["keep_wallpaper"] = true
	s.Effective["wallpaper"] = Values{"palette": "ink", "appearance": "light"}
	q, err = Query(s, "group:wallpaper", false)
	if err != nil {
		t.Fatal(err)
	}
	for _, key := range keys {
		item := q[key].(map[string]any)
		want := any("ink")
		if key == "wallpaper_appearance" {
			want = "light"
		}
		if item["resolution"] != "retained" || item["resolved"] != want || item["runtime_constraint"] != "wallpaper-retained" {
			t.Fatal(item)
		}
	}
	q["wallpaper_palette"].(map[string]any)["choices"].([]string)[0] = "changed"
	if Registry()["wallpaper_palette"].Choices[0] != "follow" {
		t.Fatal("borrowed registry choices")
	}
	s.Effective["wallpaper"] = nil
	if got, err := Query(s, "group:wallpaper", false); err == nil || got != nil {
		t.Fatal(got, err)
	}
}

func TestWallpaperUIShowsSelectionAndRetentionConstraints(t *testing.T) {
	for _, test := range []struct {
		name, palette, appearance string
		keep                      bool
		selections                [2]string
	}{
		{"follow", "follow", "follow", false, [2]string{"Selection: slate (follow)", "Selection: auto (follow)"}},
		{"explicit", "grove", "dark", false, [2]string{"Selection: grove (explicit)", "Selection: dark (explicit)"}},
		{"retained", "grove", "dark", true, [2]string{"Selection: ink (retained)", "Selection: light (retained)"}},
	} {
		t.Run(test.name, func(t *testing.T) {
			s := snapshotFixture()
			s.Effective["keep_wallpaper"] = test.keep
			s.Effective["wallpaper_palette"], s.Effective["wallpaper_appearance"] = test.palette, test.appearance
			s.Effective["wallpaper"] = Values{"palette": "ink", "appearance": "light"}
			before, _ := json.Marshal(s)
			for _, scope := range []string{"group:wallpaper", "all", "group:appearance"} {
				plan, err := Editor(s, scope, false, nil, strings.Repeat("c", 32))
				if err != nil {
					t.Fatal(err)
				}
				want := 0
				if test.keep && scope != "group:appearance" {
					want = 1
				}
				if count := strings.Count(strings.Join(plan.Lines, "\n"), wallpaperRetentionHint); count != want {
					t.Fatalf("%s: retention hints=%d want %d", scope, count, want)
				}
			}
			menus, err := Menus(s, "group:wallpaper")
			if err != nil {
				t.Fatal(err)
			}
			for i, selection := range test.selections {
				selected, constrained := false, false
				for _, row := range menus[i].Rows {
					if row[0] == selection {
						selected = row[1] == "Greyed"
					}
					if row[0] == wallpaperRetentionHint {
						constrained = row[1] == "Greyed"
					}
				}
				if !selected || constrained != test.keep {
					t.Fatal(selection, menus[i])
				}
			}
			after, _ := json.Marshal(s)
			if !bytes.Equal(before, after) {
				t.Fatal("UI planning mutated borrowed snapshot")
			}
		})
	}
}

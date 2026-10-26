// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import (
	"bytes"
	"encoding/json"
	"fmt"
	"math"
	"reflect"
	"strings"
	"testing"
)

// Independently measure the published role values, rather than source palette
// ratios, so a projection mapping mistake cannot satisfy the numerical gate.
func publishedContrast(t *testing.T, first, second string) float64 {
	t.Helper()
	luminance := func(value string) float64 {
		var r, g, b uint32
		if _, err := fmt.Sscanf(value, "#%02x%02x%02x", &r, &g, &b); err != nil {
			t.Fatal(value, err)
		}
		linear := func(channel uint32) float64 {
			c := float64(channel) / 255
			if c <= 0.04045 {
				return c / 12.92
			}
			return math.Pow((c+0.055)/1.055, 2.4)
		}
		return 0.2126*linear(r) + 0.7152*linear(g) + 0.0722*linear(b)
	}
	a, b := luminance(first), luminance(second)
	return (math.Max(a, b) + 0.05) / (math.Min(a, b) + 0.05)
}

func TestHighContrastPublishedStatePairsForEveryPalette(t *testing.T) {
	for name, variants := range shippedPalettes(t) {
		for appearance, colors := range variants {
			t.Run(name+"/"+appearance, func(t *testing.T) {
				before, _ := json.Marshal(colors)
				projection, err := ProjectTheme(colors, appearance, "high")
				if err != nil || projection.Version != 2 || len(projection.Roles) != 14 {
					t.Fatal(projection, err)
				}
				surface, text := "#000000", "#ffffff"
				if appearance == "light" {
					surface, text = text, surface
				}
				wantAccent := colors["accent"]
				if publishedContrast(t, wantAccent, surface) < 4.5 {
					wantAccent = text
				}
				wantOnAccent := "#000000"
				if publishedContrast(t, "#ffffff", wantAccent) > publishedContrast(t, "#000000", wantAccent) {
					wantOnAccent = "#ffffff"
				}
				for _, role := range projection.Roles {
					if role.Highlight != role.Background || role.Shadow != role.Background {
						t.Fatal(role)
					}
					switch role.Name {
					case "border":
						if role.Foreground != text || role.Background != text {
							t.Fatal(role)
						}
						continue
					case "accent":
						if role.Foreground != wantAccent || role.Background != wantAccent {
							t.Fatal(role)
						}
						continue
					case "menu.active", "panel.active", "panel.selected":
						if role.Foreground != wantOnAccent || role.Background != wantAccent {
							t.Fatal(role)
						}
					case "menu.accent":
						if role.Foreground != wantAccent || role.Background != surface {
							t.Fatal(role)
						}
					default:
						if role.Foreground != text || role.Background != surface {
							t.Fatal(role)
						}
					}
					if ratio := publishedContrast(t, role.Foreground, role.Background); ratio < 4.5 {
						t.Fatalf("%s contrast %.6f: %#v", role.Name, ratio, role)
					}
				}
				after, _ := json.Marshal(colors)
				if !bytes.Equal(before, after) {
					t.Fatal("projection mutated borrowed palette")
				}
				projection.Roles[0].Name = "changed"
				second, _ := ProjectTheme(colors, appearance, "high")
				if second.Roles[0].Name != "surface" {
					t.Fatal("shared role storage")
				}
			})
		}
	}
}

func TestHighContrastAccentThresholdAndInputErrors(t *testing.T) {
	colors := shippedPalettes(t)["slate"]["dark"]
	for _, test := range []struct{ appearance, accent, want string }{
		{"light", "#767676", "#767676"},
		{"light", "#777777", "#000000"},
		{"dark", "#747474", "#ffffff"},
		{"dark", "#757575", "#757575"},
	} {
		colors["accent"] = test.accent
		projection, err := ProjectTheme(colors, test.appearance, "high")
		if err != nil || projection.Roles[4].Foreground != test.want {
			t.Fatal(test, projection, err)
		}
	}
	for _, pair := range [][2]string{{"auto", "high"}, {"", "normal"}, {"dark", "HIGH"}, {"light", ""}} {
		if got, err := ProjectTheme(colors, pair[0], pair[1]); err == nil || !reflect.DeepEqual(got, ThemeProjection{}) {
			t.Fatal(pair, got, err)
		}
	}
	colors["unknown"] = "#000000"
	if got, err := ProjectTheme(colors, "dark", "high"); err == nil || got.Roles != nil {
		t.Fatal(got, err)
	}
	delete(colors, "unknown")
	colors["on_accent"] = "invalid"
	if got, err := ProjectTheme(colors, "dark", "high"); err == nil || got.Roles != nil {
		t.Fatal("high contrast must validate hidden palette entries", got, err)
	}
}

func TestPresentationAndQueryPreserveRequestsAndStrongestConstraint(t *testing.T) {
	for _, contrast := range []string{"normal", "high"} {
		for _, transparency := range []bool{false, true} {
			for _, opacity := range []int{0, 73, 100} {
				s := snapshotFixture()
				s.Effective["contrast"] = contrast
				s.Effective["transparency"], s.Effective["launcher_opacity"] = transparency, opacity
				s.Overrides = Values{"contrast": contrast, "transparency": transparency, "launcher_opacity": opacity}
				s.Submitted = true
				before, _ := json.Marshal(s)
				want := Presentation{Contrast: contrast, Transparency: transparency, LauncherOpacity: opacity}
				if contrast == "high" {
					want.Transparency, want.LauncherOpacity, want.Constraint = false, 100, "high-contrast"
				} else if !transparency {
					want.LauncherOpacity, want.Constraint = 100, "transparency-disabled"
				}
				got, err := ResolvePresentation(s.Effective)
				if err != nil || got != want {
					t.Fatal(s.Effective, got, want, err)
				}
				query, err := Query(s, "group:effects", true)
				if err != nil {
					t.Fatal(err)
				}
				for _, key := range []string{"transparency", "launcher_opacity"} {
					item := query[key].(map[string]any)
					resolved := any(want.Transparency)
					if key == "launcher_opacity" {
						resolved = want.LauncherOpacity
					}
					constraint, resolution := any(nil), "requested"
					if contrast == "high" || key == "launcher_opacity" && !transparency {
						constraint, resolution = want.Constraint, want.Constraint
					}
					if item["saved"] != s.Saved[key] || item["effective"] != s.Effective[key] || item["resolved"] != resolved ||
						item["source"] != "module-configuration" || item["runtime_constraint"] != constraint || item["resolution"] != resolution ||
						item["composition_version"] != 2 || item["generation"] != s.Generation || item["application"] != "submitted-to-fvwm" {
						t.Fatal(key, item, want)
					}
				}
				second, _ := Query(s, "group:effects", true)
				if !reflect.DeepEqual(query, second) {
					t.Fatal("unstable preference metadata")
				}
				after, _ := json.Marshal(s)
				if !bytes.Equal(before, after) {
					t.Fatal("presentation mutated snapshot")
				}
			}
		}
	}
}

func TestPresentationValidatesCompleteHiddenSettings(t *testing.T) {
	for _, change := range []Values{{"contrast": "bad"}, {"transparency": "true"}, {"launcher_opacity": 101}, {"palette": "bad"}, {"wallpaper": nil}, {"unknown": true}} {
		settings := Defaults()
		settings["contrast"] = "high"
		for key, value := range change {
			settings[key] = value
		}
		if got, err := ResolvePresentation(settings); err == nil || got != (Presentation{}) {
			t.Fatal(change, got, err)
		}
		s := snapshotFixture()
		s.Effective = settings
		if got, err := Query(s, "transparency", true); err == nil || got != nil {
			t.Fatal(change, got, err)
		}
	}
	settings := Defaults()
	delete(settings, "motion")
	if _, err := ResolvePresentation(settings); err == nil {
		t.Fatal("incomplete settings")
	}
}

func TestContrastPreparedOverridesAndUIExplanations(t *testing.T) {
	s := snapshotFixture()
	s.Overrides["contrast"] = "normal"
	prepared, err := Prepare(s, Values{"contrast": "high", "transparency": true, "launcher_opacity": 73}, 480)
	if err != nil {
		t.Fatal(err)
	}
	candidate := prepared["candidate"].(map[string]any)
	effective := prepared["effective"].(map[string]any)
	if candidate["contrast"] != "high" || effective["contrast"] != "normal" || effective["launcher_opacity"] != 73 {
		t.Fatal(prepared)
	}
	candidate["contrast"] = "changed"
	if s.Saved["contrast"] != "normal" || effective["contrast"] != "normal" {
		t.Fatal("borrowed candidate")
	}
	s.Effective["contrast"] = "high"
	plan, err := Editor(s, "all", false, nil, strings.Repeat("c", 32))
	if err != nil {
		t.Fatal(err)
	}
	lines := strings.Join(plan.Lines, "\n")
	for _, want := range []string{highContrastHint, "Selection contrast single", "Choice contrast_normal normal on", "Choice contrast_high high off", "Opacity preview is unavailable while high contrast is selected."} {
		if !strings.Contains(lines, want) {
			t.Fatal(want, lines)
		}
	}
	if !reflect.DeepEqual(plan.Keys[:4], []string{"palette", "appearance", "keep_wallpaper", "contrast"}) {
		t.Fatal(plan.Keys)
	}
	menus, err := Menus(s, "all")
	if err != nil {
		t.Fatal(err)
	}
	for _, menu := range menus {
		if menu.Name == "SlateOpacityPreview" {
			for _, row := range menu.Rows {
				if row[1] != "Greyed" {
					t.Fatal("high contrast offers runtime preview", row)
				}
			}
		}
		if menu.Name == "SlateSettings" {
			for _, row := range menu.Rows {
				if strings.HasPrefix(row[0], "transparency:") && row[0] != "transparency: false (high contrast; requested true)" {
					t.Fatal(row)
				}
			}
		}
	}
	appearance := AppearanceMenu(s.Effective)
	found := false
	for _, row := range appearance.Rows {
		found = found || row[0] == "Transparency  Off (high contrast; requested true)"
	}
	if !found {
		t.Fatal(appearance)
	}
}

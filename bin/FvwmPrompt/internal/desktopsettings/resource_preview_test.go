// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import (
	"strings"
	"testing"
)

func TestResourcePreviewCompactEditorCallbacksAndScopes(t *testing.T) {
	for _, scope := range []string{"all", "palette", "appearance", "contrast", "group:wallpaper", "group:session"} {
		s := snapshotFixture()
		plan, err := Editor(s, scope, false, nil, strings.Repeat("a", 32))
		if err != nil {
			t.Fatal(err)
		}
		lines := strings.Join(plan.Lines, "\n")
		arguments := strings.Repeat("a", 32)
		previewable := false
		for _, key := range plan.Keys {
			arguments += " $(" + key + ")"
			previewable = previewable || PreviewKey(key)
		}
		command := "Command SendToModule FvwmDesktop preview-choose-editor " + arguments
		if strings.Contains(lines, command) != previewable {
			t.Fatal("full draft not captured", scope, lines)
		}
		if strings.Count(lines, `Button continue "Preview one setting (F5)" F5`) != boolInt(previewable) {
			t.Fatal("preview control is not compact", lines)
		}
		if strings.Contains(lines, "Palette (F6)") || strings.Contains(lines, "Appearance (F7)") {
			t.Fatal("old wide button row retained")
		}
	}
}
func boolInt(value bool) int {
	if value {
		return 1
	}
	return 0
}
func TestResourceDialogsAndMenuHaveOnlyNamedTypedChoices(t *testing.T) {
	for _, key := range []string{"palette", "appearance"} {
		value := "ink"
		if key == "appearance" {
			value = "light"
		}
		lines, err := ResourcePreview(strings.Repeat("a", 32), strings.Repeat("b", 16), key, value)
		if err != nil {
			t.Fatal(err)
		}
		body := strings.Join(lines, "\n")
		for _, required := range []string{`Title "Preview ` + key + `"`, `Button continue "Cancel" ^[`, `Button continue "Save" ^M`, " saves only " + key + ".", `Timeout 160 Nop ""`} {
			if !strings.Contains(body, required) {
				t.Fatal("dialog omitted named control", required)
			}
		}
		if strings.Contains(body, " confirm") {
			t.Fatal("resource dialog exposed runtime-only confirmation")
		}
	}
	for _, bad := range [][2]string{{"appearance", "unknown"}, {"palette", "unknown"}, {"motion", "true"}, {"contrast", "other"}} {
		if _, err := ResourcePreview(strings.Repeat("a", 32), strings.Repeat("b", 16), bad[0], bad[1]); err == nil {
			t.Fatal("unsupported dialog accepted", bad)
		}
	}
	s := snapshotFixture()
	menus, err := Menus(s, "all")
	if err != nil {
		t.Fatal(err)
	}
	commands := []string{}
	for _, menu := range menus {
		for _, row := range menu.Rows {
			commands = append(commands, row[1])
		}
	}
	body := strings.Join(commands, "\n")
	for _, command := range []string{"preview-resource " + s.Instance + " palette ink", "preview-resource " + s.Instance + " appearance light", "preview-resource " + s.Instance + " appearance dark", "preview-resource " + s.Instance + " appearance auto"} {
		if !strings.Contains(body, command) {
			t.Fatal("native preview entry missing", command)
		}
	}

}
func TestResourceHeldMetadataOnlyAttributesItsThreeKeys(t *testing.T) {
	s := snapshotFixture()
	s.RuntimeHeld = PreviewValues{"palette": previewTestValue(t, "palette", "ink"), "appearance": previewTestValue(t, "appearance", "light"), "contrast": previewTestValue(t, "contrast", "high")}
	for key, value := range s.RuntimeHeld {
		s.Effective[key] = value.Value()
	}
	query, err := Query(s, "all", true)
	if err != nil {
		t.Fatal(err)
	}
	for key, raw := range query {
		item := raw.(map[string]any)
		_, held := s.RuntimeHeld[key]
		if (item["source"] == "runtime-kept-preference-unsaved") != held {
			t.Fatal("wrong held-key attribution", key, item)
		}
		if held && (item["persistence_error"] == nil || item["effective"] != s.RuntimeHeld[key].Value() || item["saved"] != s.Saved[key]) {
			t.Fatal("held state pretended saved", key, item)
		}
	}
}

func TestPreviewTypedAllowlistAndSaveOnlyReasons(t *testing.T) {
	allowed := map[string]any{"palette": "ink", "appearance": "auto", "contrast": "normal", "transparency": false,
		"keep_wallpaper": true, "wallpaper_palette": "follow", "wallpaper_appearance": "dark", "launcher_opacity": 63}
	for key, value := range allowed {
		v, err := PreviewValueFor(key, value)
		if err != nil || v.Value() != value || !PreviewKey(key) {
			t.Fatal("typed preview lost value", key, err)
		}
	}
	for _, key := range []string{"day", "night", "motion", "tray_enabled", "application_font", "session_bus"} {
		if PreviewKey(key) || PreviewReason(key) == "" {
			t.Fatal("save-only capability concealed", key)
		}
		if _, err := PreviewValueFor(key, Defaults()[key]); err == nil {
			t.Fatal("unowned overlay admitted", key)
		}
	}
	if len(Registry()) != 26 || StateVersion != 9 {
		t.Fatal("preview changed persistence registry")
	}
}

func previewTestValue(t *testing.T, key string, value any) PreviewValue {
	t.Helper()
	v, err := PreviewValueFor(key, value)
	if err != nil {
		t.Fatal(err)
	}
	return v
}

func TestPreviewValueCannotBeRekeyedAsUnownedHold(t *testing.T) {
	s := snapshotFixture()
	s.RuntimeHeld = PreviewValues{"motion": previewTestValue(t, "palette", "ink")}
	query, err := Query(s, "motion", true)
	if err != nil || query["motion"].(map[string]any)["source"] == "runtime-kept-preference-unsaved" {
		t.Fatal("unowned hold was attributed", err)
	}
}

func TestTypedHoldsResolveRetentionBeforeUnrelatedSave(t *testing.T) {
	s := snapshotFixture()
	s.Effective["wallpaper"] = Values{"palette": "ink", "appearance": "light"}
	s.Effective["keep_wallpaper"] = true
	s.RuntimeHeld = PreviewValues{"keep_wallpaper": previewTestValue(t, "keep_wallpaper", true),
		"transparency": previewTestValue(t, "transparency", false), "launcher_opacity": previewTestValue(t, "launcher_opacity", 61)}
	result, err := Prepare(s, Values{"motion": true}, 720)
	if err != nil {
		t.Fatal(err)
	}
	effective := result["effective"].(map[string]any)
	if effective["wallpaper"].(map[string]any)["palette"] != "ink" || effective["keep_wallpaper"] != true || effective["transparency"] != false || effective["launcher_opacity"] != 61 {
		t.Fatal("unrelated save lost typed holds or retained wallpaper", effective)
	}
	result, err = Prepare(s, Values{"keep_wallpaper": false}, 720)
	if err != nil || result["effective"].(map[string]any)["keep_wallpaper"] != false {
		t.Fatal("explicit replacement did not clear only that hold", err)
	}
}

func TestAllEditorSeparatesActionNavigationAndPreviewRows(t *testing.T) {
	token := strings.Repeat("c", 32)
	plan, err := Editor(snapshotFixture(), "all", false, nil, token)
	if err != nil {
		t.Fatal(err)
	}
	type actionPlacement struct {
		row      int
		shortcut string
	}
	placements := map[string]actionPlacement{}
	buttonsPerRow := map[int]int{}
	row := 0
	for index, line := range plan.Lines {
		if strings.HasPrefix(line, "Line ") {
			row++
		}
		if !strings.HasPrefix(line, "Button ") {
			continue
		}
		if index+1 == len(plan.Lines) || !strings.HasPrefix(plan.Lines[index+1], "Command ") {
			t.Fatal("editor action has no callback", line)
		}
		words := strings.Fields(line)
		command := strings.TrimPrefix(plan.Lines[index+1], "Command ")
		if _, duplicate := placements[command]; duplicate {
			t.Fatal("editor repeats callback", command)
		}
		placements[command] = actionPlacement{row: row, shortcut: words[len(words)-1]}
		buttonsPerRow[row]++
	}
	arguments := token
	for _, key := range plan.Keys {
		arguments += " $(" + key + ")"
	}
	prefix := "SendToModule FvwmDesktop "
	groups := [][]struct{ command, shortcut string }{
		{{"cancel-settings " + token, "^["}, {"defaults-settings " + token, "F2"},
			{"apply-settings " + arguments, "F3"}},
		{{"edit-settings group:session", "F8"}, {"edit-settings group:tray", "F9"},
			{"edit-settings group:application", "F10"}},
		{{"preview-choose-editor " + arguments, "F5"}},
	}
	previousRow := -1
	for _, group := range groups {
		groupRow := -1
		for _, expected := range group {
			placement, found := placements[prefix+expected.command]
			if !found || placement.shortcut != expected.shortcut {
				t.Fatal("editor action lost callback arguments or keyboard access", expected, placement)
			}
			if groupRow == -1 {
				groupRow = placement.row
			}
			if placement.row != groupRow {
				t.Fatal("related editor actions split across rows", group)
			}
		}
		if groupRow <= previousRow || buttonsPerRow[groupRow] != len(group) {
			t.Fatal("primary, navigation and preview controls share a row", placements)
		}
		previousRow = groupRow
	}
}

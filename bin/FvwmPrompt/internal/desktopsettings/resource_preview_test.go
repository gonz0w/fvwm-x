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
	for _, scope := range []string{"all", "palette", "appearance", "contrast", "group:wallpaper"} {
		s := snapshotFixture()
		plan, err := Editor(s, scope, false, nil, strings.Repeat("a", 32))
		if err != nil {
			t.Fatal(err)
		}
		lines := strings.Join(plan.Lines, "\n")
		arguments := strings.Repeat("a", 32)
		for _, key := range plan.Keys {
			arguments += " $(" + key + ")"
		}
		for _, key := range []string{"contrast", "palette", "appearance"} {
			command := "Command SendToModule FvwmDesktop preview-" + key + "-editor " + arguments
			if strings.Contains(lines, command) != has(plan.Keys, key) {
				t.Fatal("scope or full-draft callback mismatch", scope, key, lines)
			}
		}
		if scope == "all" {
			start := strings.Index(lines, `Text "Preview:"`)
			if start < 0 {
				t.Fatal("compact preview row missing")
			}
			for _, caption := range []string{"High contrast (F5)", "Palette (F6)", "Appearance (F7)"} {
				if !strings.Contains(lines[start:], caption) {
					t.Fatal("preview keyboard action missing", caption)
				}
			}
			if strings.Contains(lines[start:], "\nLine ") {
				t.Fatal("preview controls expanded into new rows")
			}
		}
	}
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
	for _, bad := range [][2]string{{"appearance", "auto"}, {"palette", "unknown"}, {"motion", "true"}, {"contrast", "normal"}} {
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
	for _, command := range []string{"preview-palette " + s.Instance + " ink", "preview-appearance " + s.Instance + " light", "preview-appearance " + s.Instance + " dark"} {
		if !strings.Contains(body, command) {
			t.Fatal("native preview entry missing", command)
		}
	}
	if strings.Contains(body, "preview-appearance "+s.Instance+" auto") {
		t.Fatal("menu advertised schedule preview")
	}
}
func TestResourceHeldMetadataOnlyAttributesItsThreeKeys(t *testing.T) {
	s := snapshotFixture()
	s.RuntimeHeld = map[string]string{"palette": "ink", "appearance": "light", "contrast": "high"}
	for key, value := range s.RuntimeHeld {
		s.Effective[key] = value
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
		if held && (item["persistence_error"] == nil || item["effective"] != s.RuntimeHeld[key] || item["saved"] != s.Saved[key]) {
			t.Fatal("held state pretended saved", key, item)
		}
	}
}

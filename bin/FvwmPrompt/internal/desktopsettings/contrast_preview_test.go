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

func TestContrastPreviewEditorCapturesEveryDraftField(t *testing.T) {
	s := snapshotFixture()
	token := strings.Repeat("c", 32)
	plan, err := Editor(s, "all", false, nil, token)
	if err != nil {
		t.Fatal(err)
	}
	arguments := token
	for _, key := range plan.Keys {
		arguments += " $(" + key + ")"
	}
	lines := strings.Join(plan.Lines, "\n")
	if !strings.Contains(lines, `Button continue "High contrast (F5)" F5`) ||
		!strings.Contains(lines, "Command SendToModule FvwmDesktop preview-contrast-editor "+arguments) {
		t.Fatal("contrast preview failed to capture complete editor draft", lines)
	}
	if len(Registry()) != 14 {
		t.Fatal("resource preview expanded preference registry")
	}
}

func TestContrastPreviewOffersOnlySaveCancel(t *testing.T) {
	lines, err := ContrastPreview(strings.Repeat("a", 32), strings.Repeat("b", 16))
	if err != nil {
		t.Fatal(err)
	}
	text := strings.Join(lines, "\n")
	for _, required := range []string{`Button continue "Cancel"`, `Button continue "Save"`, " revert", " save", highContrastHint, `Timeout 160 Nop ""`} {
		if !strings.Contains(text, required) {
			t.Fatal("missing contrast control", required)
		}
	}
	if strings.Contains(text, " confirm") || strings.Contains(text, "Keep current") {
		t.Fatal("contrast preview exposed opacity confirmation semantics")
	}
	if _, err := ContrastPreview("bad", strings.Repeat("b", 16)); err == nil {
		t.Fatal("invalid actor accepted")
	}
	if _, err := ContrastPreview(strings.Repeat("a", 32), "invalid;command!"); err == nil {
		t.Fatal("invalid view accepted")
	}
}

func TestContrastPreviewMenuAndRuntimeOnlySource(t *testing.T) {
	s := snapshotFixture()
	s.Saved["launcher_opacity"], s.Effective["launcher_opacity"] = 73, 73
	query, err := Query(s, "all", true)
	if err != nil {
		t.Fatal(err)
	}
	for key, raw := range query {
		metadata := raw.(map[string]any)
		if metadata["preview"] != false ||
			metadata["native_preview"] != (key == "contrast" || key == "launcher_opacity" || key == "palette" || key == "appearance") ||
			metadata["source"] != "saved" && !SessionKey(key) {
			t.Fatal("native consumer flag claimed generic preview or changed source", key, metadata)
		}
	}
	menus, err := Menus(s, "all")
	if err != nil {
		t.Fatal(err)
	}
	found := false
	for _, menu := range menus {
		for _, row := range menu.Rows {
			found = found || row[1] == "SendToModule FvwmDesktop preview-contrast "+s.Instance
		}
	}
	if !found {
		t.Fatal("normal presentation has no contrast preview entry")
	}
	s.RuntimeHeld, s.Effective["contrast"] = map[string]string{"contrast": "high"}, "high"
	query, err = Query(s, "all", true)
	if err != nil {
		t.Fatal(err)
	}
	contrast := query["contrast"].(map[string]any)
	if contrast["source"] != "runtime-kept-preference-unsaved" || contrast["saved"] != "normal" || contrast["effective"] != "high" || contrast["native_preview"] != true || contrast["preview"] != false {
		t.Fatal("runtime-only kept preference was misreported", contrast)
	}
	transparency := query["transparency"].(map[string]any)
	opacity := query["launcher_opacity"].(map[string]any)
	if transparency["saved"] != true || transparency["effective"] != true || transparency["resolved"] != false ||
		opacity["saved"] != 73 || opacity["effective"] != 73 || opacity["resolved"] != 100 {
		t.Fatal("native contrast metadata replaced requested effects", transparency, opacity)
	}
}

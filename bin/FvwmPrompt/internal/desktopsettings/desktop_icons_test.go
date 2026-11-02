// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import (
	"encoding/json"
	"strings"
	"testing"
)

func TestDesktopIconThemePolicyAndHistoricalFiles(t *testing.T) {
	for _, name := range []string{"", "hicolor", "Adwaita", "User_1.2-theme", strings.Repeat("x", 128)} {
		s := Defaults()
		s["desktop_icon_theme"] = name
		got, err := DesktopIconTheme(s)
		want := name
		if want == "" {
			want = "hicolor"
		}
		if err != nil || got != want {
			t.Fatal(name, got, err)
		}
	}
	for _, name := range []any{".", "..", "/tmp/icons", "two words", "é", "$theme", "a\nb", strings.Repeat("x", 129), true, nil} {
		if _, err := Value("desktop_icon_theme", name, false); err == nil {
			t.Fatal(name)
		}
	}
	for version := 1; version <= 11; version++ {
		state := versionedFixture(version)
		data, _ := json.Marshal(state)
		got, err := DecodeState(data)
		if err != nil || got["desktop_icon_theme"] != "" {
			t.Fatal(version, got, err)
		}
		if version < 11 {
			delete(state, "palette")
			state["desktop_icon_theme"] = "Other"
			data, _ = json.Marshal(state)
			if _, err := DecodeState(data); err == nil {
				t.Fatal("future key displaced required historical field", version)
			}
		}
	}
}

func TestDesktopIconThemeIndependenceAndSubmissionMetadata(t *testing.T) {
	s := snapshotFixture()
	s.Saved["icon_theme"] = "Application"
	s.Effective["icon_theme"] = "Application"
	s.Overrides = Values{"desktop_icon_theme": "NativeOverride"}
	p, err := Prepare(s, Values{"desktop_icon_theme": "SavedNative"}, 720)
	if err != nil {
		t.Fatal(err)
	}
	s.Saved, s.Effective = p["candidate"].(map[string]any), p["effective"].(map[string]any)
	if s.Saved["desktop_icon_theme"] != "SavedNative" || s.Effective["desktop_icon_theme"] != "NativeOverride" || s.Effective["icon_theme"] != "Application" {
		t.Fatal(p)
	}
	s.Submitted, s.DesktopSubmitted = true, true
	q, err := Query(s, "desktop_icon_theme", true)
	if err != nil {
		t.Fatal(err)
	}
	v := q["desktop_icon_theme"].(map[string]any)
	if v["source"] != "module-configuration" || v["resolved"] != "NativeOverride" || v["application"] != "not-submitted" || PreviewKey("desktop_icon_theme") || NativeKey("desktop_icon_theme") {
		t.Fatal(v)
	}
	s.DesktopIconSubmitted = true
	q, err = Query(s, "desktop_icon_theme", true)
	if err != nil || q["desktop_icon_theme"].(map[string]any)["application"] != "tray-configuration-submitted" {
		t.Fatal(q, err)
	}
}

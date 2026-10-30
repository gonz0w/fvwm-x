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

func TestSessionRegistryAndHistoricalReads(t *testing.T) {
	if StateVersion != 7 || len(Registry()) != 19 {
		t.Fatal("registry scope")
	}
	keys, _ := Scope("group:session")
	if !reflect.DeepEqual(keys, []string{"session_bus", "session_notifications", "notification_dnd", "session_power"}) {
		t.Fatal(keys)
	}
	for _, version := range []int{1, 2, 3, 4, 5, 6} {
		v := versionedFixture(version)
		data, _ := json.Marshal(v)
		got, err := DecodeState(data)
		if err != nil || got["session_bus"] != "off" || got["session_notifications"] != "off" || got["notification_dnd"] != false || got["session_power"] != "portable" {
			t.Fatal(version, got, err)
		}
	}
	for key, bad := range map[string][]any{"session_bus": {"system", "auto", true}, "session_notifications": {"start", true, "external;Exec bad"}, "notification_dnd": {"yes", 0, nil}, "session_power": {"auto", "systemctl", true, "login1;Exec bad"}} {
		for _, v := range bad {
			if _, err := Value(key, v, false); err == nil {
				t.Fatal(key, v)
			}
		}
	}
}
func TestSessionEditorIsCompactAndAppearanceDraftRemainsElevenFields(t *testing.T) {
	s := snapshotFixture()
	token := strings.Repeat("c", 32)
	appearance, err := Editor(s, "all", false, nil, token)
	if err != nil {
		t.Fatal(err)
	}
	if len(appearance.Keys) != 11 || strings.Contains(strings.Join(appearance.Lines, "\n"), "Choice session_bus") {
		t.Fatal("appearance grew offscreen")
	}
	if !strings.Contains(strings.Join(appearance.Lines, "\n"), "edit-settings group:session") {
		t.Fatal("missing scoped navigation")
	}
	plan, err := Editor(s, "group:session", false, Values{"session_bus": "start", "session_notifications": "external", "notification_dnd": true, "session_power": "login1"}, token)
	if err != nil || len(plan.Keys) != 4 {
		t.Fatal(plan, err)
	}
	text := strings.Join(plan.Lines, "\n")
	for _, part := range []string{`Title "Session settings"`, "Colorset 2", "ItemColorset 12", "next login", "live handshake", "session_bus_start start on", "notification_dnd_true true on", "apply-settings " + token + " $(session_bus) $(session_notifications) $(notification_dnd) $(session_power)"} {
		if !strings.Contains(text, part) {
			t.Fatal(part, text)
		}
	}
	if strings.Contains(text, "preview-") || len(plan.Lines) > 75 {
		t.Fatal("session editor grew appearance controls")
	}
}
func TestServiceMetadataDistinguishesSavedFromObservedLivePolicy(t *testing.T) {
	s := snapshotFixture()
	s.Saved["session_bus"] = "start"
	s.Saved["session_notifications"] = "external"
	q, err := Query(s, "group:session", true)
	if err != nil {
		t.Fatal(err)
	}
	if q["session_bus"].(map[string]any)["application"] != "next-login" || q["session_notifications"].(map[string]any)["effective"] != nil {
		t.Fatal(q)
	}
	s.SessionAvailable = true
	q, _ = Query(s, "group:session", true)
	if q["session_notifications"].(map[string]any)["saved"] != "external" || q["session_notifications"].(map[string]any)["effective"] != "off" || q["session_notifications"].(map[string]any)["application"] != "live-owner-observed" {
		t.Fatal(q)
	}
}

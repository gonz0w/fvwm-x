// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"strings"
	"testing"
)

func TestDesktopIconsScopedThemeResetAndNoReload(t *testing.T) {
	d, commands := fixture(t)
	if err := d.theme(true); err != nil {
		t.Fatal(err)
	}
	*commands = nil
	if err := d.changeSettings(values{"desktop_icon_theme": "UserTheme"}); err != nil {
		t.Fatal(err)
	}
	text := strings.Join(*commands, "\n")
	if !strings.Contains(text, "AddToFunc SlateTrayConfigure I *SlateTray IconTheme UserTheme") || !strings.Contains(text, "AddToFunc SlateTrayConfigure I SendToModule SlateTray Refresh") || !d.desktopIconsSubmitted() {
		t.Fatal(text)
	}
	forbidComponentCommands(t, *commands, "Colorset ", "MenuStyle ", "Style *",
		"SendToModule SlateBar", "KillModule FvwmTray", "Module FvwmTray ",
		"*PersonalTray", "SendToModule PersonalTray")
	*commands = nil
	if err := d.changeSettings(values{"desktop_icon_theme": "UserTheme"}); err != nil {
		t.Fatal(err)
	}
	if strings.Contains(strings.Join(*commands, "\n"), "SlateTray Refresh") {
		t.Fatal("unchanged effective choice reloads", *commands)
	}
	*commands = nil
	if err := d.changeSettings(values{"desktop_icon_theme": ""}); err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(strings.Join(*commands, "\n"), "*SlateTray IconTheme hicolor") {
		t.Fatal(*commands)
	}
	// Application preference publication is optional and independently owned.
	if d.effective["icon_theme"] != "" || d.effective["native_settings_enabled"] != false {
		t.Fatal(d.effective)
	}
}

func TestDesktopIconThemeWaitsForResourceRecovery(t *testing.T) {
	d, commands := colorFixture(t, "normal")
	activeColor(t, d, "palette", "dune")
	previous := d.lastDesktopIconTheme
	*commands = nil
	if err := d.changeSettings(values{"desktop_icon_theme": "NewNative"}); err != nil {
		t.Fatal(err)
	}
	if d.lastDesktopIconTheme != previous || d.desktopIconsSubmitted() || strings.Contains(strings.Join(*commands, "\n"), "*SlateTray IconTheme NewNative") {
		t.Fatal(*commands)
	}
	*commands = nil
	d.projectTray() // Reconfiguration must retain the submitted tuple too.
	if strings.Contains(strings.Join(*commands, "\n"), "*SlateTray IconTheme NewNative") {
		t.Fatal("configuration crossed recovery gate", *commands)
	}
	d.preview.blocked = true
	*commands = nil
	if err := d.theme(true); err != nil || len(*commands) != 0 {
		t.Fatal(err, *commands)
	}
	*commands = nil
	completeComponentRecovery(t, d)
	if !d.desktopIconsSubmitted() {
		t.Fatal("recovered icon profile not submitted", *commands)
	}
	text := strings.Join(*commands, "\n")
	if !strings.Contains(text, "*SlateTray IconTheme NewNative") {
		t.Fatal(text)
	}
	forbidComponentCommands(t, *commands, "Colorset ", "MenuStyle ", "Style *",
		"SendToModule SlateBar")
}

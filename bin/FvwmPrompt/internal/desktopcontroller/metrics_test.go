// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/base64"
	"reflect"
	"strings"
	"testing"
)

// Check executable command boundaries, including immediate function bodies.
// Menu labels and module configuration may legitimately name core directives.
func forbidComponentCommands(t *testing.T, commands []string, prefixes ...string) {
	t.Helper()
	for _, command := range commands {
		body := command
		if words := strings.SplitN(command, " ", 4); len(words) == 4 && words[0] == "AddToFunc" && words[2] == "I" {
			body = words[3]
		}
		for _, prefix := range prefixes {
			if strings.HasPrefix(body, prefix) {
				t.Fatal("expanded component command scope", command)
			}
		}
	}
}

func completeComponentRecovery(t *testing.T, d *Controller) {
	t.Helper()
	if d.preview.pending == nil || !d.preview.recovering || !d.preview.replay {
		t.Fatal("fixture has no pending recovery", d.preview)
	}
	// Release only the fixture's synthetic blocked marker. The ordinary reply
	// decoder must clear transaction state and perform the guarded replay.
	d.preview.blocked = false
	contrastReply(&d.preview, colorResult("terminal", "abandoned", "restored"))
	if d.preview.pending != nil || d.preview.control != nil || d.preview.recovering || d.preview.replay {
		t.Fatal("terminal reply did not complete recovery", d.preview)
	}
}

func TestDesktopMetricsScopedPublicationAndInherit(t *testing.T) {
	d, commands := fixture(t)
	if err := d.theme(true); err != nil {
		t.Fatal(err)
	}
	for _, command := range *commands {
		if strings.HasPrefix(command, "MenuStyle SlateLauncherStyle Font ") || strings.Contains(command, "VerticalItemSpacing") {
			t.Fatal("initial inherit overrides profile", command)
		}
	}
	*commands = nil
	if err := d.changeSettings(values{"desktop_font": "xft:Sans:size=14", "desktop_density": "spacious", "desktop_border": "strong"}); err != nil {
		t.Fatal(err)
	}
	text := strings.Join(*commands, "\n")
	want := `MenuStyle SlateLauncherStyle Font "xft:Sans:size=14", VerticalItemSpacing 12 12, VerticalTitleSpacing 14 14, BorderWidth 3`
	if !strings.Contains(text, want) || !d.desktopSubmitted() {
		t.Fatal(text)
	}
	forbidComponentCommands(t, *commands, "Colorset ", "MenuStyle *", "Style *",
		"SendToModule SlateBar", "*SlateTray", "SendToModule SlateTray", "SlateTrayConfigure",
		"DefaultFont ", "InfoStoreAdd slate_", "SlatePanel", "KillModule FvwmButtons", "EwmhBaseStruts")
	*commands = nil
	if err := d.changeSettings(values{"desktop_density": "spacious"}); err != nil {
		t.Fatal(err)
	}
	for _, command := range *commands {
		if strings.HasPrefix(command, "MenuStyle ") || strings.HasPrefix(command, "Colorset ") || strings.HasPrefix(command, "Style ") {
			t.Fatal("unchanged metrics republished appearance", command)
		}
	}
	if len(d.lastTheme) != 8 {
		t.Fatal("color preview contract changed", d.lastTheme)
	}
	*commands = nil
	d.form("SlateNoticeForm", []string{`Text "new form"`}, false)
	text = strings.Join(*commands, "\n")
	for _, want := range []string{`*SlateNoticeForm: Font "xft:Sans:size=14"`, `*SlateNoticeForm: InputFont "xft:Sans:size=14"`, `*SlateNoticeForm: ButtonFont "xft:Sans:size=14"`, "*SlateNoticeForm: PadVText 16"} {
		if !strings.Contains(text, want) {
			t.Fatal(want, text)
		}
	}
	*commands = nil
	if err := d.theme(false); err != nil || len(*commands) != 0 {
		t.Fatal("unchanged projection republishes", *commands, err)
	}
	if err := d.changeSettings(values{"desktop_font": "", "desktop_density": "inherit", "desktop_border": "inherit"}); err != nil {
		t.Fatal(err)
	}
	text = strings.Join(*commands, "\n")
	if !strings.Contains(text, `MenuStyle SlateLauncherStyle Font "$[infostore.slate_font]", VerticalItemSpacing 8 8, VerticalTitleSpacing 10 10, BorderWidth 1`) {
		t.Fatal(text)
	}
}

func TestDesktopMetricsWaitForRecoveryAndKeepOldFormTuple(t *testing.T) {
	d, commands := colorFixture(t, "normal")
	activeColor(t, d, "palette", "dune")
	before := *d.lastDesktop
	*commands = nil
	if err := d.changeSettings(values{"desktop_font": "fixed", "desktop_density": "compact"}); err != nil {
		t.Fatal(err)
	}
	noThemeReplay(t, *commands)
	if *d.lastDesktop != before || d.desktopSubmitted() || !d.preview.replay {
		t.Fatal("unconfirmed metrics published", d.lastDesktop, d.preview)
	}
	*commands = nil
	d.form("SlateNoticeForm", []string{`Text "recovery pending"`}, false)
	if strings.Contains(strings.Join(*commands, "\n"), `Font "fixed"`) {
		t.Fatal("new intent crossed preview gate", *commands)
	}
	// A conflict retains the same publication barrier; neither font nor geometry
	// is part of the color lease or claimed recoverable by that lease.
	d.preview.blocked = true
	*commands = nil
	if err := d.theme(true); err != nil || len(*commands) != 0 || *d.lastDesktop != before {
		t.Fatal(*commands, err)
	}
	// This unit drives the terminal decoder with a synthetic core reply; real
	// core recovery and pixels remain part of the combined native checkpoint.
	*commands = nil
	completeComponentRecovery(t, d)
	if !d.desktopSubmitted() {
		t.Fatal("recovered metrics not submitted", *commands)
	}
	for _, command := range *commands {
		for _, prefix := range []string{"Colorset ", "MenuStyle *", "Style *", "SendToModule SlateBar", "*SlateTray"} {
			if strings.HasPrefix(command, prefix) {
				t.Fatal("recovery widened metrics scope", command)
			}
		}
	}
	if !strings.Contains(strings.Join(*commands, "\n"), `MenuStyle SlateLauncherStyle Font "fixed", VerticalItemSpacing 4 4`) {
		t.Fatal(*commands)
	}
}

func TestDesktopMetricsEditorLiteralAndIndependentAliases(t *testing.T) {
	d, commands := fixture(t)
	if err := d.editSettings("group:desktop", false, nil); err != nil {
		t.Fatal(err)
	}
	font := "xft:Sans:size=13"
	_, changes, err := d.editorValues(d.editor.token + " x" + base64.RawURLEncoding.EncodeToString([]byte(font)) + " compact strong x")
	if err != nil || !reflect.DeepEqual(changes, values{"desktop_font": font, "desktop_density": "compact", "desktop_border": "strong", "desktop_icon_theme": ""}) {
		t.Fatal(changes, err)
	}
	*commands = nil
	d.form("PersonalForm", []string{`Font "personal"`}, false)
	if strings.Contains(strings.Join(*commands, "\n"), "PadVText") {
		t.Fatal(*commands)
	}
	before := d.saved["desktop_font"]
	if err := d.changeSettings(values{"desktop_font": `fixed";Exec bad`}); err == nil || d.saved["desktop_font"] != before {
		t.Fatal("unrepresentable value persisted")
	}
}

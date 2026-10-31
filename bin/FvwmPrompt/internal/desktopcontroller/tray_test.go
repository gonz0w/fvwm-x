// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"os"
	"reflect"
	"strings"
	"testing"
)

func trayLifecycleCommands(commands []string) []string {
	var result []string
	for _, command := range commands {
		if strings.HasPrefix(command, "KillModule FvwmTray") || strings.HasPrefix(command, "Module FvwmTray") {
			result = append(result, command)
		}
	}
	return result
}

func TestTrayOffStartupEnableRefreshAndAliasOnlyDisable(t *testing.T) {
	d, commands := fixture(t)
	if got := trayLifecycleCommands(*commands); len(got) != 0 {
		t.Fatal("off startup touched a tray", got)
	}
	*commands = nil
	if err := d.changeSettings(values{
		"tray_enabled": true, "tray_gap": 0, "tray_padding": 64,
		"tray_orientation": "vertical", "tray_modern": true, "tray_icon_size": 32,
	}); err != nil {
		t.Fatal(err)
	}
	want := []string{"Module FvwmTray SlateTray"}
	if got := trayLifecycleCommands(*commands); !reflect.DeepEqual(got, want) {
		t.Fatal("enable did not preserve an existing owner", got)
	}
	text := strings.Join(*commands, "\n")
	for _, expected := range []string{"*SlateTray Colorset 2", "*SlateTray StatusNotifier On", "*SlateTray IconSize 32", `*SlateTray Font "$[infostore.slate_font]"`, "*SlateTray Gap 0", "*SlateTray Padding 64", "*SlateTray Orientation Vertical", "*SlateTray Visible Off", "SendToModule SlateTray Reload", "SendToModule SlateBar ChangeButton tray Enabled 1"} {
		if !strings.Contains(text, expected) {
			t.Fatal("missing coherent tray projection", expected)
		}
	}
	if strings.Index(text, "*SlateTray Visible Off") > strings.Index(text, "Module FvwmTray SlateTray") || strings.Contains(text, "ModuleSynchronous") || strings.Contains(text, "Wait ") {
		t.Fatal("tray started before its hidden configuration or blocked startup", text)
	}
	persisted, _, err := loadSettings(d.statePath)
	if err != nil || persisted["tray_enabled"] != true || persisted["tray_gap"] != 0 || persisted["tray_padding"] != 64 || persisted["tray_orientation"] != "vertical" || persisted["tray_modern"] != true || persisted["tray_icon_size"] != 32 {
		t.Fatal("typed tray settings did not persist", persisted, err)
	}
	for _, refresh := range []func() error{
		func() error { return d.handle("refresh", "1280") },
		func() error { return d.changeSettings(values{"palette": "dune"}) },
		func() error { return d.changeSettings(values{"tray_gap": 12}) },
		d.reloadSettings,
	} {
		*commands = nil
		if err := refresh(); err != nil {
			t.Fatal(err)
		}
		if got := trayLifecycleCommands(*commands); len(got) != 0 {
			t.Fatal("refresh restarted the tray", got)
		}
		if !strings.Contains(strings.Join(*commands, "\n"), "SendToModule SlateBar ChangeButton tray Enabled 1") {
			t.Fatal("refresh failed to restore launcher enablement")
		}
	}
	*commands = nil
	if err := d.changeSettings(values{"tray_enabled": true}); err != nil {
		t.Fatal(err)
	}
	if got := trayLifecycleCommands(*commands); !reflect.DeepEqual(got, want) {
		t.Fatal("explicit enable could not retry a competing-owner refusal", got)
	}
	*commands = nil
	if err := d.changeSettings(values{"tray_enabled": false}); err != nil {
		t.Fatal(err)
	}
	if got := trayLifecycleCommands(*commands); !reflect.DeepEqual(got, []string{"KillModule FvwmTray SlateTray"}) {
		t.Fatal("disable touched a custom alias or started a tray", got)
	}
	if !strings.Contains(strings.Join(*commands, "\n"), "SendToModule SlateBar ChangeButton tray Enabled 0") {
		t.Fatal("disabled launcher stayed enabled")
	}
}

func TestTraySavedStartupAndInvalidTransaction(t *testing.T) {
	d, _ := fixture(t)
	if err := d.changeSettings(values{"tray_enabled": true}); err != nil {
		t.Fatal(err)
	}
	var commands []string
	restarted, err := New(d.root, d.statePath, d.system, d.worker, d.capture, d.png,
		func(command string) error { commands = append(commands, command); return nil })
	if err != nil {
		t.Fatal(err)
	}
	defer restarted.Close()
	if err := restarted.finishConfiguration(); err != nil {
		t.Fatal(err)
	}
	if got := trayLifecycleCommands(commands); !reflect.DeepEqual(got, []string{"Module FvwmTray SlateTray"}) {
		t.Fatal("saved enablement did not preserve an existing owner", got)
	}
	commands = nil
	before, _ := os.ReadFile(d.statePath)
	generation := restarted.generation
	if err := restarted.changeSettings(values{"tray_enabled": false, "tray_gap": 65}); err == nil {
		t.Fatal("invalid tray tuple committed")
	}
	after, _ := os.ReadFile(d.statePath)
	if string(before) != string(after) || restarted.generation != generation || len(commands) != 0 {
		t.Fatal("refused tray tuple changed persistence, generation or lifecycle")
	}
	query, err := restarted.query("group:tray", nil, nil)
	if err != nil || query["tray_enabled"].(map[string]any)["effective"] != true || query["tray_enabled"].(map[string]any)["readiness"] != "unobserved" {
		t.Fatal("requested tray state was confused with live session observation", query, err)
	}
}

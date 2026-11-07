// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/json"
	"errors"
	"reflect"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopworker"
)

func reloadCandidate(t *testing.T, d *Controller) desktopsettings.Palettes {
	t.Helper()
	data, err := json.Marshal(d.palettes)
	if err != nil {
		t.Fatal(err)
	}
	candidate, err := desktopsettings.DecodePalettes(data)
	if err != nil {
		t.Fatal(err)
	}
	for _, phases := range candidate {
		for _, colors := range phases {
			colors["surface"] = "#123456"
			colors["background"] = "#abcdef"
		}
	}
	return candidate
}
func finishReloadFixture(d *Controller, value any, err error) {
	origin := d.themeReload.job
	d.cancelQueued("theme-reload")
	d.finishThemeReload(job{key: "theme-reload", origin: origin, result: value, err: err})
}
func assertReloadCommands(t *testing.T, commands []string) {
	t.Helper()
	for _, command := range commands {
		if !strings.HasPrefix(command, "Colorset ") && !strings.HasPrefix(command, "Echo FvwmDesktop:") {
			t.Fatal("reload disturbed another component", command)
		}
		if strings.HasPrefix(command, "Colorset 20 ") || strings.HasPrefix(command, "Colorset 9 ") {
			t.Fatal("reload changed an unsupported colorset", command)
		}
	}
}
func TestThemeReloadRetainsUIAndSettingsWhilePublishingChangedContent(t *testing.T) {
	d, commands := fixture(t)
	if err := d.theme(true); err != nil {
		t.Fatal(err)
	}
	if err := d.editSettings("palette", false, nil); err != nil {
		t.Fatal(err)
	}
	generation, revision, editor := d.generation, d.revision, d.editor
	settings, _ := json.Marshal(d.snapshot(nil, nil))
	bindings := []*menuBinding{d.settingsBinding, d.appBinding, d.systemBinding}
	theme, background := append([]any(nil), d.lastTheme...), d.lastWallpaper
	candidate := reloadCandidate(t, d)
	*commands = nil
	if err := d.handle("reload-theme", ""); err != nil {
		t.Fatal(err)
	}
	if len(*commands) != 1 || (*commands)[0] != "Echo FvwmDesktop: theme colors reload pending." || len(d.queue) != 1 || len(d.jobs) != 1 || d.themeReload.palettes != nil {
		t.Fatal("acquisition published or lost bounded ownership")
	}
	if err := d.reloadTheme(); err == nil || len(d.queue) != 1 {
		t.Fatal("duplicate reload admitted")
	}
	pending := d.themeReload
	d.finishThemeReload(job{origin: &job{}, result: desktopworker.PaletteResult{Palettes: candidate}})
	if d.themeReload != pending || pending.palettes != nil {
		t.Fatal("another worker completion acquired this reload")
	}
	*commands = nil
	finishReloadFixture(d, desktopworker.PaletteResult{Palettes: candidate}, nil)
	if d.themeReload != nil || d.palettes["slate"]["dark"]["surface"] != "#123456" {
		t.Fatal("prepared colors did not publish")
	}
	if d.palettes["slate"]["dark"]["background"] != "#17232b" {
		t.Fatal("reload admitted background changes for later publication")
	}
	after, _ := json.Marshal(d.snapshot(nil, nil))
	if string(after) != string(settings) || generation != d.generation || !sameRevision(revision, d.revision) || d.editor != editor || !reflect.DeepEqual(theme, d.lastTheme) || background != d.lastWallpaper {
		t.Fatal("reload replaced saved state or a live identity")
	}
	if !reflect.DeepEqual(bindings, []*menuBinding{d.settingsBinding, d.appBinding, d.systemBinding}) {
		t.Fatal("reload replaced menu bindings")
	}
	assertReloadCommands(t, *commands)
	if !strings.Contains(strings.Join(*commands, "\n"), "#123456") || !strings.Contains(strings.Join(*commands, "\n"), "RootTransparent buffer, Tint #123456 88") {
		t.Fatal("unchanged preference tuple hid changed colors", *commands)
	}
	*commands = nil
	if err := d.theme(false); err != nil || len(*commands) != 0 {
		t.Fatal("ordinary tick needlessly replayed theme", *commands, err)
	}
	if err := d.reloadTheme(); err != nil {
		t.Fatal(err)
	}
	*commands = nil
	finishReloadFixture(d, desktopworker.PaletteResult{Palettes: d.palettes}, nil)
	if len(*commands) != 1 || !strings.HasPrefix((*commands)[0], "Echo FvwmDesktop:") {
		t.Fatal("identical reload rebuilt native resources", *commands)
	}
}
func TestThemeReloadFailureStalenessAndTeardownKeepWorkingColors(t *testing.T) {
	for _, reason := range []string{"worker", "source", "shape", "generation", "instance", "closed"} {
		t.Run(reason, func(t *testing.T) {
			d, commands := fixture(t)
			if err := d.theme(true); err != nil {
				t.Fatal(err)
			}
			working, _ := json.Marshal(d.palettes)
			candidate := reloadCandidate(t, d)
			*commands = nil
			if err := d.reloadTheme(); err != nil {
				t.Fatal(err)
			}
			var value any = desktopworker.PaletteResult{Palettes: candidate}
			var err error
			switch reason {
			case "worker":
				err = errors.New("worker unavailable")
			case "source":
				value = desktopworker.PaletteResult{Error: "/theme/palettes.json: slate.dark.surface: expected #RRGGBB"}
			case "shape":
				delete(candidate, "grove")
			case "generation":
				d.generation = newID(16)
			case "instance":
				d.instance = newID(16)
			case "closed":
				d.closed = true
			}
			*commands = nil
			finishReloadFixture(d, value, err)
			after, _ := json.Marshal(d.palettes)
			if d.themeReload != nil || string(after) != string(working) || len(*commands) != 0 {
				t.Fatal("failed/stale reload mutated working component", *commands)
			}
		})
	}
}
func TestThemeReloadWaitsForResourceRecoveryAndRechecksGeneration(t *testing.T) {
	for _, outcome := range []string{"restored", "conflict", "settings-changed"} {
		t.Run(outcome, func(t *testing.T) {
			d, commands := fixture(t)
			if err := d.theme(true); err != nil {
				t.Fatal(err)
			}
			candidate := reloadCandidate(t, d)
			if err := d.reloadTheme(); err != nil {
				t.Fatal(err)
			}
			// Model a resource lease already in recovery. Only the ordinary
			// native reply path may release it; acquisition must not do so.
			d.preview.control = &previewControl{resource: &resourceCandidate{}}
			d.preview.recovering = true
			control := d.preview.control
			*commands = nil
			finishReloadFixture(d, desktopworker.PaletteResult{Palettes: candidate}, nil)
			if d.themeReload == nil || d.preview.control != control || len(*commands) != 0 || d.palettes["slate"]["dark"]["surface"] != "#202a31" {
				t.Fatal("prepared colors escaped recovery gate")
			}
			d.preview.control, d.preview.recovering = nil, false
			if outcome == "conflict" {
				d.preview.blocked = true
			} else if outcome == "settings-changed" {
				d.generation = newID(16)
			}
			d.publishThemeReload()
			if d.themeReload != nil {
				t.Fatal("terminal decision retained candidate")
			}
			if outcome == "restored" {
				if d.palettes["slate"]["dark"]["surface"] != "#123456" {
					t.Fatal("restored candidate not published")
				}
				assertReloadCommands(t, *commands)
			} else if len(*commands) != 0 || d.palettes["slate"]["dark"]["surface"] != "#202a31" {
				t.Fatal("conflicting/stale recovery published colors")
			}
		})
	}
}
func TestThemeReloadAdmissionAndHighContrast(t *testing.T) {
	d, commands := fixture(t)
	for _, argument := range []string{"extra", " all"} {
		if err := d.handle("reload-theme", argument); err == nil {
			t.Fatal("reload accepted arguments")
		}
	}
	d.preview.blocked = true
	if err := d.reloadTheme(); err == nil {
		t.Fatal("reload admitted blocked recovery")
	}
	d.preview.blocked = false
	d.preview.control = &previewControl{}
	if err := d.reloadTheme(); err == nil {
		t.Fatal("reload interrupted legacy opacity preview")
	}
	d.preview.control = nil
	for _, key := range []string{"a", "b", "c", "d"} {
		d.jobs[key] = &job{key: key}
	}
	if err := d.reloadTheme(); err == nil || d.themeReload != nil {
		t.Fatal("reload exceeded job budget")
	}
	d.jobs = map[string]*job{}
	d.effective["contrast"], d.effective["appearance"] = "high", "dark"
	if err := d.theme(true); err != nil {
		t.Fatal(err)
	}
	*commands = nil
	if err := d.reloadTheme(); err != nil {
		t.Fatal(err)
	}
	finishReloadFixture(d, desktopworker.PaletteResult{Palettes: reloadCandidate(t, d)}, nil)
	assertReloadCommands(t, *commands)
	joined := strings.Join(*commands, "\n")
	if !strings.Contains(joined, "Colorset 0 Plain, fg #ffffff, bg #000000") || strings.Contains(joined, "RootTransparent") {
		t.Fatal("reload ignored high-contrast presentation", joined)
	}
}

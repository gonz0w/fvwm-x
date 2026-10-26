// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"fmt"
	"os"
	"reflect"
	"strings"
	"testing"
)

func TestHighContrastPublicationAndRequestedOpacityRestoration(t *testing.T) {
	for _, appearance := range []string{"dark", "light"} {
		t.Run(appearance, func(t *testing.T) {
			d, commands := fixture(t)
			if err := d.changeSettings(values{"appearance": appearance,
				"launcher_opacity": 61, "motion": true,
				"wallpaper_palette": "dune", "wallpaper_appearance": "dark"}); err != nil {
				t.Fatal(err)
			}
			wallpaper := d.lastWallpaper
			*commands = nil
			if err := d.changeSettings(values{"contrast": "high"}); err != nil {
				t.Fatal(err)
			}
			text := strings.Join(*commands, "\n")
			foreground, background := "#ffffff", "#000000"
			if appearance == "light" {
				foreground, background = background, foreground
			}
			for _, id := range []int{0, 1, 2, 5, 7, 10, 12, 14} {
				want := fmt.Sprintf("Colorset %d Plain, fg %s, bg %s, hi %s, sh %s", id, foreground, background, background, background)
				if !strings.Contains(text, want) {
					t.Fatal("missing projected native role", want)
				}
			}
			if !strings.Contains(text, "MenuStyle * !Translucent") || strings.Contains(text, "RootTransparent") || strings.Contains(text, " Translucent ") || !strings.Contains(text, "MenuStyle * Animation") {
				t.Fatal("high contrast did not constrain only translucency", text)
			}
			if strings.Contains(text, "Colorset 20 ") || d.lastWallpaper != wallpaper {
				t.Fatal("contrast changed independent wallpaper")
			}
			for _, settings := range []values{d.saved, d.effective} {
				if settings["transparency"] != true || settings["launcher_opacity"] != 61 {
					t.Fatal("presentation overwrote requested effects", settings)
				}
			}
			if !reflect.DeepEqual(d.lastTheme, []any{"slate", appearance, true, true, 61, "dune", "dark", "high"}) {
				t.Fatal("contrast cache changed previous positions", d.lastTheme)
			}
			*commands = nil
			if err := d.theme(false); err != nil || len(*commands) != 0 {
				t.Fatal("unchanged high contrast replayed", err)
			}
			if err := d.resetSettings("contrast"); err != nil {
				t.Fatal(err)
			}
			text = strings.Join(*commands, "\n")
			if !strings.Contains(text, "Colorset 10 RootTransparent buffer, Tint "+d.palettes["slate"][appearance]["surface"]+" 88") || !strings.Contains(text, "MenuStyle SlateLauncherStyle Translucent 61") || strings.Contains(text, "Colorset 20 ") {
				t.Fatal("return to normal lost requested effects or wallpaper", text)
			}
		})
	}
}

func TestHighContrastPreviewAdmissionAndEditorCandidate(t *testing.T) {
	d, commands := fixture(t)
	if err := d.changeSettings(values{"contrast": "high"}); err != nil {
		t.Fatal(err)
	}
	for _, saved := range []bool{false, true} {
		*commands = nil
		if err := d.preview.start(63, saved, nil); err == nil || d.preview.control != nil || len(*commands) != 0 {
			t.Fatal("current high contrast admitted preview", saved)
		}
	}
	if err := d.resetSettings("contrast"); err != nil {
		t.Fatal(err)
	}
	if err := d.editSettings("all", false, nil); err != nil {
		t.Fatal(err)
	}
	e := d.editor
	argument := e.token
	for _, key := range e.keys {
		value := d.saved[key]
		if key == "contrast" {
			value = "high"
		}
		argument += " " + fmt.Sprint(value)
	}
	*commands = nil
	if err := d.handle("preview-editor", argument); err == nil || d.editor != e || d.preview.control != nil || len(*commands) != 0 {
		t.Fatal("draft high contrast admitted preview or retired editor")
	}
	// Module authority applies before presentation. A saved high request may
	// still preview when the effective configuration explicitly selects normal.
	if err := d.configure("contrast", "normal"); err != nil {
		t.Fatal(err)
	}
	if err := d.changeSettings(values{"contrast": "high"}); err != nil {
		t.Fatal(err)
	}
	activePreview(t, d, true)
	if err := d.preview.action(d.preview.control.view, "save"); err != nil {
		t.Fatal(err)
	}
	replyTo(&d.preview, success(values{"scope": "menu-opacity", "state": "confirmed"}))
	if d.saved["contrast"] != "high" || d.effective["contrast"] != "normal" || d.saved["launcher_opacity"] != 63 {
		t.Fatal("preview lost module authority or saved request")
	}
}

func pendingPreview(t *testing.T, d *Controller, phase string) {
	t.Helper()
	if err := d.preview.start(63, true, nil); err != nil {
		t.Fatal(err)
	}
	if phase == "inspect" {
		return
	}
	replyTo(&d.preview, success(values{"generation": strings.Repeat("a", 48)}))
	if phase == "start" {
		return
	}
	replyTo(&d.preview, success(values{"scope": "menu-opacity", "state": "active", "generation": strings.Repeat("b", 48), "lease": strings.Repeat("c", 48)}))
	if err := d.preview.request(phase); err != nil {
		t.Fatal(err)
	}
}

func TestHighContrastDelayedPreviewReplies(t *testing.T) {
	for _, phase := range []string{"inspect", "start", "query", "confirm", "save"} {
		t.Run(phase, func(t *testing.T) {
			d, commands := fixture(t)
			pendingPreview(t, d, phase)
			if phase == "query" {
				if err := d.preview.action(d.preview.control.view, "confirm"); err != nil {
					t.Fatal(err)
				}
			}
			// Exercise each delayed-reply guard independently of normal commit
			// invalidation, which otherwise clears the old serial immediately.
			d.effective["contrast"] = "high"
			*commands = nil
			result := values{"scope": "menu-opacity", "state": "active", "generation": strings.Repeat("b", 48), "lease": strings.Repeat("c", 48)}
			if phase == "inspect" {
				result = values{"generation": strings.Repeat("a", 48)}
			} else if phase == "save" || phase == "confirm" {
				result["state"] = "confirmed"
			}
			replyTo(&d.preview, success(result))
			text := strings.Join(*commands, "\n")
			if d.preview.control != nil || d.saved["launcher_opacity"] != 94 || !strings.Contains(text, " abandon ") || strings.Contains(text, " start ") || strings.Contains(text, " confirm ") || strings.Contains(text, "\nModule FvwmForm SlatePreview-") {
				t.Fatal("delayed reply escaped opaque policy", text)
			}
		})
	}
}

func TestHighContrastQueuedPreviewActionsRecheck(t *testing.T) {
	for _, op := range []string{"confirm", "save"} {
		t.Run(op, func(t *testing.T) {
			d, commands := fixture(t)
			activePreview(t, d, true)
			control := d.preview.control
			if err := d.preview.request("query"); err != nil {
				t.Fatal(err)
			}
			if err := d.preview.action(control.view, op); err != nil || d.preview.queued != op {
				t.Fatal("could not queue preview action", err)
			}
			d.effective["contrast"] = "high"
			*commands = nil
			if err := d.preview.action(control.view, op); err == nil || len(*commands) != 0 {
				t.Fatal("new action escaped changed policy")
			}
			replyTo(&d.preview, success(values{"scope": "menu-opacity", "state": "active", "generation": strings.Repeat("b", 48), "lease": strings.Repeat("c", 48)}))
			if strings.Contains(strings.Join(*commands, "\n"), " confirm ") || d.preview.control != nil {
				t.Fatal("queued action escaped changed policy")
			}
		})
	}
}

func TestOpaqueCommitAbandonsPreviewBeforeTheme(t *testing.T) {
	for _, phase := range []string{"inspect", "start", "query", "confirm", "save"} {
		t.Run(phase, func(t *testing.T) {
			d, commands := fixture(t)
			pendingPreview(t, d, phase)
			serial := d.preview.pending.serial
			*commands = nil
			if err := d.changeSettings(values{"contrast": "high"}); err != nil {
				t.Fatal(err)
			}
			text := strings.Join(*commands, "\n")
			abandon, publish := strings.Index(text, " abandon "), strings.Index(text, "Colorset 0 Plain,")
			if abandon < 0 || publish <= abandon || d.preview.control != nil || d.preview.pending != nil || d.preview.queued != "" {
				t.Fatal("opaque publication preceded preview retirement", text)
			}
			count := len(*commands)
			d.preview.reply("MFL_CORE_1 " + d.instance + " " + serial + " end 1")
			if len(*commands) != count {
				t.Fatal("late retired reply published commands")
			}
		})
	}
}

func TestFailedOpaqueTransitionPreservesPreview(t *testing.T) {
	for _, failure := range []string{"projection", "persistence", "reload"} {
		t.Run(failure, func(t *testing.T) {
			d, commands := fixture(t)
			pendingPreview(t, d, "query")
			control, pending, generation := d.preview.control, d.preview.pending, d.generation
			if failure == "projection" {
				d.palettes["slate"]["dark"]["surface"] = "invalid"
				d.palettes["slate"]["light"]["surface"] = "invalid"
			} else if err := os.WriteFile(d.statePath, []byte("{}"), 0600); err != nil {
				t.Fatal(err)
			}
			if failure == "reload" {
				if err := os.WriteFile(d.statePath, []byte("invalid"), 0600); err != nil {
					t.Fatal(err)
				}
			}
			*commands = nil
			var err error
			if failure == "reload" {
				err = d.reloadSettings()
			} else {
				err = d.changeSettings(values{"contrast": "high"})
			}
			if err == nil || d.preview.control != control || d.preview.pending != pending || d.generation != generation || len(*commands) != 0 {
				t.Fatal("failed opaque transition disturbed current preview", err)
			}
		})
	}
}

func TestOpaqueReloadAndModuleOverrideRetirePreview(t *testing.T) {
	for _, transition := range []string{"reload", "module"} {
		t.Run(transition, func(t *testing.T) {
			d, commands := fixture(t)
			activePreview(t, d, true)
			*commands = nil
			var err error
			switch transition {
			case "reload":
				stored := merged(d.saved, values{"contrast": "high"})
				if _, err = saveSettings(d.statePath, stored, d.revision); err == nil {
					err = d.reloadSettings()
				}
			case "module":
				if err = d.configure("contrast", "high"); err == nil {
					err = d.changeSettings(values{"motion": true})
				}
			}
			if err != nil {
				t.Fatal(err)
			}
			text := strings.Join(*commands, "\n")
			if d.preview.control != nil || strings.Index(text, " abandon ") < 0 || strings.Index(text, " abandon ") > strings.Index(text, "Colorset 0 Plain,") {
				t.Fatal("opaque transition did not retire preview before publication", text)
			}
			if transition == "module" && (d.saved["contrast"] != "normal" || d.effective["contrast"] != "high") {
				t.Fatal("module constraint replaced saved request")
			}
		})
	}
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/json"
	"fmt"
	"os"
	"reflect"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

func TestThemeProjectionPreservesColorsetsAndEffects(t *testing.T) {
	for _, transparent := range []bool{false, true} {
		t.Run(fmt.Sprint(transparent), func(t *testing.T) {
			d, commands := fixture(t)
			d.effective["palette"], d.effective["appearance"] = "grove", "light"
			d.effective["transparency"], d.effective["motion"] = transparent, true
			d.effective["launcher_opacity"] = 63
			*commands = nil
			if err := d.theme(false); err != nil {
				t.Fatal(err)
			}
			palette := d.palettes["grove"]["light"]
			roles := []struct {
				id     int
				fg, bg string
			}{{0, "text", "surface"}, {1, "muted", "surface"}, {2, "text", "raised"}, {3, "border", "border"},
				{4, "accent", "accent"}, {5, "text", "surface"}, {6, "on_accent", "accent"}, {7, "muted", "surface"}, {8, "accent", "surface"},
				{10, "text", "surface"}, {11, "on_accent", "accent"}, {12, "text", "raised"}, {13, "on_accent", "accent"}, {14, "muted", "surface"}}
			for i, role := range roles {
				want := fmt.Sprintf("Colorset %d Plain, fg %s, bg %s, hi %s, sh %s", role.id, palette[role.fg], palette[role.bg], palette[role.bg], palette[role.bg])
				if (*commands)[i] != want {
					t.Fatalf("role %d: %q != %q", role.id, (*commands)[i], want)
				}
			}
			text := strings.Join(*commands, "\n")
			for _, command := range []string{"MenuStyle * Animation", "Style * WindowShadeSteps 6", "CopyMenuStyle * SlateLauncherStyle"} {
				if !strings.Contains(text, command) {
					t.Fatal("missing effect:", command)
				}
			}
			if transparent {
				if !strings.Contains(text, "Colorset 10 RootTransparent buffer, Tint "+palette["surface"]+" 88") || !strings.Contains(text, "MenuStyle SlateLauncherStyle Translucent 63") {
					t.Fatal("transparent panel or launcher opacity changed")
				}
			} else if !strings.Contains(text, "MenuStyle * !Translucent") || strings.Contains(text, "RootTransparent") {
				t.Fatal("opaque effects changed")
			}
		})
	}
}

func TestWallpaperOnlyInvalidationAndIndependentPalette(t *testing.T) {
	d, commands := fixture(t)
	d.effective["appearance"] = "light"
	if err := d.theme(false); err != nil {
		t.Fatal(err)
	}
	for _, settings := range []values{
		{"wallpaper_palette": "dune", "wallpaper_appearance": "dark"},
		{"wallpaper_appearance": "light"},
		{"palette": "ink"},
	} {
		d.effective = merged(d.effective, settings)
		*commands = nil
		if err := d.theme(false); err != nil {
			t.Fatal(err)
		}
		want := [2]string{"dune", stringAt(d.effective, "wallpaper_appearance")}
		if d.lastWallpaper != want || !reflect.DeepEqual(d.effective["wallpaper"], values{"palette": want[0], "appearance": want[1]}) {
			t.Fatalf("wallpaper resolution: %v %v", d.lastWallpaper, d.effective["wallpaper"])
		}
		text := strings.Join(*commands, "\n")
		if len(*commands) == 0 || !strings.HasPrefix((*commands)[0], "Colorset 0 Plain,") {
			t.Fatal("wallpaper-only transition bypassed theme invalidation")
		}
		if settings["palette"] == "ink" {
			if strings.Contains(text, "Colorset 20 ") || !strings.Contains((*commands)[0], d.palettes["ink"]["light"]["surface"]) {
				t.Fatal("UI palette changed explicit wallpaper")
			}
		} else if !strings.Contains(text, "dune-"+want[1]+".png") {
			t.Fatal("wallpaper command missing")
		}
	}
	*commands = nil
	if err := d.theme(false); err != nil || len(*commands) != 0 {
		t.Fatal("unchanged theme replayed", err)
	}
}

func TestWallpaperHoldReleaseAndSolidBackground(t *testing.T) {
	d, commands := fixture(t)
	d.png = false
	if err := d.changeSettings(values{"appearance": "light", "wallpaper_palette": "dune", "wallpaper_appearance": "dark"}); err != nil {
		t.Fatal(err)
	}
	*commands = nil
	if err := d.changeSettings(values{"keep_wallpaper": true, "palette": "ink", "wallpaper_palette": "grove", "wallpaper_appearance": "light"}); err != nil {
		t.Fatal(err)
	}
	if d.lastWallpaper != [2]string{"dune", "dark"} || strings.Contains(strings.Join(*commands, "\n"), "Colorset 20 ") {
		t.Fatal("hold failed to preserve actual wallpaper")
	}
	for _, settings := range []values{d.saved, d.effective} {
		if !reflect.DeepEqual(settings["wallpaper"], values{"palette": "dune", "appearance": "dark"}) {
			t.Fatal("retained wallpaper was not committed")
		}
	}
	*commands = nil
	if err := d.changeSettings(values{"keep_wallpaper": false}); err != nil {
		t.Fatal(err)
	}
	text := strings.Join(*commands, "\n")
	if d.lastWallpaper != [2]string{"grove", "light"} || !strings.Contains(text, "Colorset 20 Plain, bg "+d.palettes["grove"]["light"]["background"]) || strings.Contains(text, "AspectPixmap") {
		t.Fatal("release or minimal solid background failed")
	}
}

func TestThemeRefusalsPreserveWorkingProjection(t *testing.T) {
	for _, malformed := range []string{"ui", "wallpaper", "retained"} {
		t.Run(malformed, func(t *testing.T) {
			d, commands := fixture(t)
			d.effective["appearance"] = "light"
			d.effective["wallpaper_palette"], d.effective["wallpaper_appearance"] = "dune", "dark"
			if err := d.theme(false); err != nil {
				t.Fatal(err)
			}
			switch malformed {
			case "ui":
				d.palettes["slate"]["light"]["accent"] = "bad; Echo injected"
			case "wallpaper":
				d.palettes["dune"]["dark"]["background"] = ""
			case "retained":
				d.effective["keep_wallpaper"] = true
				d.effective["wallpaper"] = values{"palette": "unknown", "appearance": "dark"}
			}
			before, _ := json.Marshal(d.effective)
			key := append([]any(nil), d.lastTheme...)
			wallpaper := d.lastWallpaper
			*commands = nil
			if err := d.theme(false); err == nil {
				t.Fatal("invalid projection accepted")
			}
			after, _ := json.Marshal(d.effective)
			if string(after) != string(before) || !reflect.DeepEqual(key, d.lastTheme) || wallpaper != d.lastWallpaper || len(*commands) != 0 {
				t.Fatal("failed projection mutated or published state")
			}
		})
	}
}

func TestThemePreflightRefusesCommitAndReload(t *testing.T) {
	d, commands := fixture(t)
	if err := d.changeSettings(values{"appearance": "light"}); err != nil {
		t.Fatal(err)
	}
	before, _ := os.ReadFile(d.statePath)
	generation, revision := d.generation, d.revision
	effective, _ := json.Marshal(d.effective)
	d.palettes["dune"]["dark"]["background"] = "invalid"
	*commands = nil
	if err := d.changeSettings(values{"wallpaper_palette": "dune", "wallpaper_appearance": "dark"}); err == nil {
		t.Fatal("invalid wallpaper committed")
	}
	after, _ := os.ReadFile(d.statePath)
	if string(after) != string(before) {
		t.Fatal("projection failure replaced settings")
	}
	stored := desktopsettings.Defaults()
	stored["wallpaper_palette"], stored["wallpaper_appearance"] = "dune", "dark"
	if _, err := saveSettings(d.statePath, stored, revision); err != nil {
		t.Fatal(err)
	}
	if err := d.reloadSettings(); err == nil {
		t.Fatal("invalid wallpaper reloaded")
	}
	observed, _ := json.Marshal(d.effective)
	if string(observed) != string(effective) || d.generation != generation || !sameRevision(d.revision, revision) || len(*commands) != 0 {
		t.Fatal("preflight refusal changed runtime authority")
	}
}

func TestWallpaperEditorRegistryAndIPCMetadata(t *testing.T) {
	d, commands := fixture(t)
	for key, value := range map[string]string{"wallpaper_palette": "grove", "wallpaper_appearance": "light"} {
		if err := d.configure(key, value); err != nil {
			t.Fatal(err)
		}
	}
	if err := d.finishConfiguration(); err != nil {
		t.Fatal(err)
	}
	if err := d.editSettings("group:wallpaper", false, nil); err != nil {
		t.Fatal(err)
	}
	e := d.editor
	if !reflect.DeepEqual(e.keys, []string{"wallpaper_palette", "wallpaper_appearance"}) {
		t.Fatal("registry editor keys", e.keys)
	}
	argument := e.token
	for _, key := range e.keys {
		argument += " " + map[string]string{"wallpaper_palette": "dune", "wallpaper_appearance": "dark"}[key]
	}
	if err := d.handle("apply-settings", argument); err != nil {
		t.Fatal(err)
	}
	if d.saved["wallpaper_palette"] != "dune" || d.lastWallpaper != [2]string{"grove", "light"} {
		t.Fatal("module override authority lost")
	}
	response := d.settingsRequest(values{"operation": "query", "scope": "group:wallpaper"})
	result := response["result"].(map[string]any)
	settings := result["settings"].(map[string]any)
	for key, resolved := range map[string]string{"wallpaper_palette": "grove", "wallpaper_appearance": "light"} {
		item := reconstructedPublicSetting(result, settings[key].(map[string]any))
		if item["source"] != "module-configuration" || item["resolution"] != "explicit" || item["resolved"] != resolved || item["composition_version"] != 2 {
			t.Fatal("override metadata", item)
		}
	}
	*commands = nil
	generation := d.generation
	response = d.settingsRequest(values{"operation": "set", "provider": d.instance, "generation": strings.Repeat("f", 32), "values": values{"wallpaper_palette": "ink"}})
	if code(response) != "stale_generation" || len(*commands) != 0 {
		t.Fatal("stale generation admitted", response)
	}
	if err := d.editSettings("group:wallpaper", false, nil); err != nil {
		t.Fatal(err)
	}
	e = d.editor
	stored, revision, err := loadSettings(d.statePath)
	if err != nil {
		t.Fatal(err)
	}
	stored["wallpaper_palette"] = "ink"
	if _, err := saveSettings(d.statePath, stored, revision); err != nil {
		t.Fatal(err)
	}
	response = d.settingsRequest(values{"operation": "set", "provider": d.instance, "generation": generation, "values": values{"wallpaper_palette": "slate"}})
	if code(response) != "persistence_failed" || d.editor != e || d.generation != generation {
		t.Fatal("disk conflict lost editor or generation", response)
	}
	response = d.settingsRequest(values{"operation": "reload", "provider": d.instance, "generation": generation})
	if code(response) != "" || d.saved["wallpaper_palette"] != "ink" || d.editor != nil || d.lastWallpaper != [2]string{"grove", "light"} {
		t.Fatal("reload/override failed", response)
	}
	response = d.settingsRequest(values{"operation": "reset", "provider": d.instance, "generation": d.generation, "scope": "group:wallpaper"})
	if code(response) != "" || d.saved["wallpaper_palette"] != "follow" || d.saved["wallpaper_appearance"] != "follow" || d.lastWallpaper != [2]string{"grove", "light"} {
		t.Fatal("reset/override failed", response)
	}
	data, err := os.ReadFile(d.statePath)
	if err != nil {
		t.Fatal(err)
	}
	state, err := desktopsettings.DecodeJSON(data)
	if err != nil || state.(map[string]any)["schema_version"] != json.Number("9") {
		t.Fatal("saved schema was not version 7", string(data), err)
	}
}

func TestWallpaperQuerySelectionPolicyAndPreviewPosition(t *testing.T) {
	d, commands := fixture(t)
	d.effective["wallpaper_palette"], d.effective["wallpaper_appearance"] = "dune", "dark"
	if err := d.theme(false); err != nil {
		t.Fatal(err)
	}
	activePreview(t, d, true)
	control := d.preview.control
	if err := d.preview.action(control.view, "save"); err != nil {
		t.Fatal(err)
	}
	replyTo(&d.preview, success(values{"scope": "menu-opacity", "state": "confirmed"}))
	if len(d.lastTheme) != 8 || d.lastTheme[4] != 63 || d.lastTheme[5] != "dune" || d.lastTheme[6] != "dark" || d.lastTheme[7] != "normal" {
		t.Fatal("opacity preview changed wallpaper cache ownership", d.lastTheme)
	}
	*commands = nil
	if err := d.theme(false); err != nil || len(*commands) != 0 {
		t.Fatal("preview confirmation invalidated theme", err)
	}
	delete(d.overrides, "wallpaper_appearance")
	d.effective["wallpaper_palette"], d.effective["wallpaper_appearance"] = "follow", "follow"
	d.effective["appearance"] = "auto"
	before, err := d.query("group:wallpaper", nil, nil)
	if err != nil {
		t.Fatal(err)
	}
	appearance := before["wallpaper_appearance"].(map[string]any)
	if appearance["resolution"] != "follow" || appearance["resolved"] != "auto" {
		t.Fatal("query reported painted phase instead of selection policy", appearance)
	}
	d.effective["wallpaper"] = values{"palette": "slate", "appearance": "light"}
	after, err := d.query("group:wallpaper", nil, nil)
	if err != nil || !reflect.DeepEqual(before, after) {
		t.Fatal("clock-phase retained pair changed generation-stable metadata", after, err)
	}
	response := d.settingsRequest(values{"operation": "query", "scope": "group:wallpaper", "provider": d.instance, "generation": d.generation})
	if response["result"].(map[string]any)["state"] != "unchanged" {
		t.Fatal("conditional query generation contract changed")
	}
	d.effective["keep_wallpaper"] = true
	metadata, err := d.query("group:wallpaper", nil, nil)
	if err != nil {
		t.Fatal(err)
	}
	for key, resolved := range map[string]string{"wallpaper_palette": "slate", "wallpaper_appearance": "light"} {
		item := metadata[key].(map[string]any)
		if item["resolution"] != "retained" || item["resolved"] != resolved || item["runtime_constraint"] != "wallpaper-retained" {
			t.Fatal("hold metadata", item)
		}
	}
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

func colorFixture(t *testing.T, contrast string) (*Controller, *[]string) {
	t.Helper()
	d, commands := fixture(t)
	settings := values{"appearance": "dark", "transparency": contrast == "high",
		"wallpaper_palette": "grove", "wallpaper_appearance": "light",
		"launcher_opacity": 73, "contrast": contrast}
	d.saved = merged(d.saved, settings)
	d.effective = merged(d.saved, nil)
	if err := d.theme(true); err != nil {
		t.Fatal(err)
	}
	*commands = nil
	return d, commands
}
func colorControl(t *testing.T, d *Controller, key, value string) *previewControl {
	t.Helper()
	snapshot, candidate, err := d.prepareResource(key, value)
	if err != nil {
		t.Fatal(err)
	}
	return &previewControl{view: newID(8), saved: snapshot, resource: candidate}
}
func colorAppearance(available bool) values {
	return success(values{"generation": strings.Repeat("a", 48), "opaque_colors_preview": values{
		"version": 1, "scope": "opaque-colors", "plan_version": 2,
		"role_count": 14, "style_count": 2, "metadata_max": 65536,
		"preparation_timeout_ms": 5000, "lease_limit": 1, "slot_available": available}})
}
func colorResult(state, reason, recovery string) values {
	result := contrastResult(state, reason, recovery)
	body := result["result"].(map[string]any)
	body["scope"], body["plan_version"] = "opaque-colors", 2
	return result
}
func activeColor(t *testing.T, d *Controller, key, value string) {
	t.Helper()
	if err := d.preview.startResource(key, value, nil); err != nil {
		t.Fatal(err)
	}
	contrastReply(&d.preview, colorAppearance(true))
	contrastReply(&d.preview, colorResult("prepared", "", "not-applied"))
	contrastReply(&d.preview, colorResult("applying", "", "not-applied"))
	if err := d.preview.request("query"); err != nil {
		t.Fatal(err)
	}
	contrastReply(&d.preview, colorResult("active", "", "pending"))
	if d.preview.control == nil || d.preview.control.resource.state != "active" {
		t.Fatal("resource never became active")
	}
}

func TestOpaqueColorCandidatePreservesOtherPresentationAndPlanBounds(t *testing.T) {
	for _, contrast := range []string{"normal", "high"} {
		for _, key := range []string{"palette", "appearance"} {
			t.Run(contrast+"/"+key, func(t *testing.T) {
				d, commands := colorFixture(t, contrast)
				value := "ink"
				if key == "appearance" {
					value = "light"
				}
				before := merged(d.effective, nil)
				control := colorControl(t, d, key, value)
				plan, err := resourcePlan(control.resource.projection, 2)
				if err != nil || len(plan) != 338 || !strings.HasPrefix(plan, "2:") || !identity(plan[2:], 336) {
					t.Fatal("invalid finite plan", plan, err)
				}
				if control.resource.scope != "opaque-colors" || control.resource.planVersion != 2 ||
					control.resource.theme[7] != contrast || control.resource.theme[5] != "grove" || control.resource.theme[6] != "light" ||
					control.resource.theme[4] != 73 || !reflect.DeepEqual(d.effective, before) || len(*commands) != 0 {
					t.Fatal("candidate expanded selected setting scope")
				}
				if err := d.checkResource(control); err != nil {
					t.Fatal(err)
				}
				control.resource.projection.Roles[0].Foreground = "#123456"
				if d.checkResource(control) == nil {
					t.Fatal("altered projection admitted")
				}
			})
		}
	}
}
func TestOpaqueColorAdmissionRefusals(t *testing.T) {
	for _, reason := range []string{"transparent", "follow-wallpaper", "auto", "unchanged", "override", "file", "unsubmitted", "unknown"} {
		t.Run(reason, func(t *testing.T) {
			d, commands := colorFixture(t, "normal")
			key, value := "palette", "ink"
			switch reason {
			case "transparent":
				d.effective["transparency"] = true
			case "follow-wallpaper":
				d.effective["wallpaper_palette"] = "follow"
			case "auto":
				key, value = "appearance", "auto"
			case "unchanged":
				value = "slate"
			case "override":
				d.overrides[key] = "grove"
			case "file":
				if err := os.WriteFile(d.statePath, []byte("{}"), 0600); err != nil {
					t.Fatal(err)
				}
			case "unsubmitted":
				d.lastTheme = nil
			case "unknown":
				key, value = "wallpaper_palette", "ink"
			}
			if reason == "transparent" || reason == "follow-wallpaper" {
				if err := d.theme(true); err != nil {
					t.Fatal(err)
				}
			}
			*commands = nil
			if err := d.preview.startResource(key, value, nil); err == nil || d.preview.control != nil || len(*commands) != 0 {
				t.Fatal("refused candidate acquired a lease or emitted commands")
			}
		})
	}
}
func TestOpaqueColorKeepWallpaperAndSchedulePhase(t *testing.T) {
	d, _ := colorFixture(t, "normal")
	d.effective["wallpaper_palette"], d.effective["wallpaper_appearance"] = "follow", "follow"
	d.effective["keep_wallpaper"] = true
	if err := d.theme(true); err != nil {
		t.Fatal(err)
	}
	if _, _, err := d.prepareResource("palette", "dune"); err != nil {
		t.Fatal("retained wallpaper was refused", err)
	}
	d.effective["appearance"] = "auto"
	key, _, _, err := d.themePlan(d.effective, 720)
	if err != nil {
		t.Fatal(err)
	}
	d.lastTheme = key
	if _, _, err := d.prepareResourceAt("palette", "dune", 720); err != nil {
		t.Fatal(err)
	}
	if _, _, err := d.prepareResourceAt("palette", "dune", 0); err == nil {
		t.Fatal("changed automatic schedule phase admitted")
	}
}
func TestOpaqueColorEditorSelectsOneFieldFromCompleteDraft(t *testing.T) {
	for _, key := range []string{"palette", "appearance"} {
		t.Run(key, func(t *testing.T) {
			d, _ := colorFixture(t, "normal")
			if err := d.editSettings("all", false, nil); err != nil {
				t.Fatal(err)
			}
			argument, expected := d.editor.token, values{}
			for _, name := range d.editor.keys {
				value := d.saved[name]
				switch name {
				case "palette":
					value = "ink"
				case "appearance":
					value = "light"
				case "transparency":
					value = true
				case "wallpaper_palette", "wallpaper_appearance":
					value = "follow"
				case "motion":
					value = true
				}
				expected[name] = value
				argument += " " + fmt.Sprint(value)
			}
			if err := d.handle("preview-"+key+"-editor", argument); err != nil {
				t.Fatal(err)
			}
			control := d.preview.control
			if control == nil || d.editor != nil || !reflect.DeepEqual(control.editor.values, expected) || len(control.editor.values) != 11 {
				t.Fatal("complete draft transfer lost")
			}
			want := "ink"
			if key == "appearance" {
				want = "light"
			}
			if control.resource.key != key || control.resource.value != want || control.resource.theme[2] != false || control.resource.theme[3] != false || control.resource.theme[5] != "grove" || control.resource.theme[6] != "light" {
				t.Fatal("preview consumed unrelated draft values")
			}
			if key == "palette" && control.resource.theme[1] != "dark" {
				t.Fatal("palette consumed appearance draft")
			}
			if key == "appearance" && control.resource.theme[0] != "slate" {
				t.Fatal("appearance consumed palette draft")
			}
		})
	}
}
func TestOpaqueColorDirectedSaveOnlySelectedPreference(t *testing.T) {
	for _, contrast := range []string{"normal", "high"} {
		for _, key := range []string{"palette", "appearance"} {
			t.Run(contrast+"/"+key, func(t *testing.T) {
				d, commands := colorFixture(t, contrast)
				value := "ink"
				if key == "appearance" {
					value = "light"
				}
				before := merged(d.saved, nil)
				activeColor(t, d, key, value)
				if !strings.Contains(strings.Join(*commands, "\n"), " - 0 15000 2:") {
					t.Fatal("version2 plan was not submitted")
				}
				if err := d.preview.action(d.preview.control.view, "save"); err != nil {
					t.Fatal(err)
				}
				if _, err := os.Stat(d.statePath); !os.IsNotExist(err) {
					t.Fatal("write preceded native confirmation")
				}
				*commands = nil
				contrastReply(&d.preview, colorResult("terminal", "confirmed", "kept"))
				before[key] = value
				stored, _, err := loadSettings(d.statePath)
				if err != nil || !reflect.DeepEqual(stored, before) || d.preview.control != nil || d.runtimeUnsaved(key) {
					t.Fatal("wrong preference persisted", stored, err)
				}
				noThemeReplay(t, *commands)
				*commands = nil
				if err := d.theme(false); err != nil || len(*commands) != 0 {
					t.Fatal("timer replayed confirmed resource state", err)
				}
			})
		}
	}
}
func TestOpaqueColorBothAppearanceDirections(t *testing.T) {
	for _, pair := range [][2]string{{"dark", "light"}, {"light", "dark"}} {
		d, _ := colorFixture(t, "normal")
		d.saved["appearance"], d.effective["appearance"] = pair[0], pair[0]
		if err := d.theme(true); err != nil {
			t.Fatal(err)
		}
		control := colorControl(t, d, "appearance", pair[1])
		if err := d.confirmResource(control); err != nil || d.effective["appearance"] != pair[1] || d.lastThemeAppearance != pair[1] {
			t.Fatal("appearance direction failed", pair, err)
		}
	}
}
func TestThreeConfirmedUnsavedKeysSurviveTicksAndUnrelatedEdits(t *testing.T) {
	d, commands := colorFixture(t, "normal")
	lock := filepath.Join(filepath.Dir(d.statePath), ".desktop-state.lock")
	if err := os.Mkdir(lock, 0700); err != nil {
		t.Fatal(err)
	}
	for _, item := range [][2]string{{"palette", "ink"}, {"appearance", "light"}, {"contrast", "high"}} {
		control := colorControl(t, d, item[0], item[1])
		*commands = nil
		if err := d.confirmResource(control); err == nil || !d.runtimeUnsaved(item[0]) || d.effective[item[0]] != item[1] {
			t.Fatal("failed save lost confirmed runtime choice", item)
		}
		noThemeReplay(t, *commands)
		*commands = nil
		if err := d.theme(false); err != nil || len(*commands) != 0 {
			t.Fatal("tick replayed older saved state", err)
		}
	}
	if len(d.runtimeHeld) != 3 || d.saved["palette"] != "slate" || d.saved["appearance"] != "dark" || d.saved["contrast"] != "normal" {
		t.Fatal("overlay scope or saved state changed")
	}
	query, err := d.query("all", nil, nil)
	if err != nil {
		t.Fatal(err)
	}
	for _, key := range []string{"palette", "appearance", "contrast"} {
		m := query[key].(map[string]any)
		if m["source"] != "runtime-kept-preference-unsaved" || m["persistence_error"] == nil {
			t.Fatal("unsaved runtime source/error hidden", key, m)
		}
	}
	if err := os.Remove(lock); err != nil {
		t.Fatal(err)
	}
	if err := d.changeSettings(values{"motion": true}); err != nil {
		t.Fatal(err)
	}
	if len(d.runtimeHeld) != 3 || d.effective["palette"] != "ink" || d.effective["appearance"] != "light" || d.effective["contrast"] != "high" {
		t.Fatal("unrelated edit discarded held choices")
	}
	if err := d.changeSettings(values{"appearance": "dark"}); err != nil {
		t.Fatal(err)
	}
	if d.runtimeUnsaved("appearance") || !d.runtimeUnsaved("palette") || !d.runtimeUnsaved("contrast") {
		t.Fatal("explicit replacement cleared other overlays")
	}
	if err := d.reloadSettings(); err != nil || len(d.runtimeHeld) != 0 || d.effective["palette"] != "slate" || d.effective["contrast"] != "normal" {
		t.Fatal("reload failed to retire overlays", err)
	}
}
func TestOpaqueColorSuccessPreservesEarlierUnsavedKey(t *testing.T) {
	d, _ := colorFixture(t, "normal")
	lock := filepath.Join(filepath.Dir(d.statePath), ".desktop-state.lock")
	if err := os.Mkdir(lock, 0700); err != nil {
		t.Fatal(err)
	}
	if d.confirmResource(colorControl(t, d, "palette", "ink")) == nil {
		t.Fatal("write refusal missing")
	}
	if err := os.Remove(lock); err != nil {
		t.Fatal(err)
	}
	if err := d.confirmResource(colorControl(t, d, "appearance", "light")); err != nil {
		t.Fatal(err)
	}
	if !d.runtimeUnsaved("palette") || d.runtimeUnsaved("appearance") || d.effective["palette"] != "ink" || d.saved["palette"] != "slate" || d.saved["appearance"] != "light" {
		t.Fatal("later successful save erased earlier held value")
	}
	if err := d.changeSettings(values{"palette": "ink"}); err != nil || len(d.runtimeHeld) != 0 || d.saved["palette"] != "ink" {
		t.Fatal("explicitly saving held key did not clear it", err)
	}
}
func TestOpaqueColorRejectsOldKindAndRechecksBeforeApply(t *testing.T) {
	for _, phase := range []string{"apply", "result-kind", "confirm-file"} {
		t.Run(phase, func(t *testing.T) {
			d, commands := colorFixture(t, "normal")
			if phase == "apply" {
				if err := d.preview.startResource("palette", "ink", nil); err != nil {
					t.Fatal(err)
				}
				contrastReply(&d.preview, colorAppearance(true))
				if err := os.WriteFile(d.statePath, []byte("{}"), 0600); err != nil {
					t.Fatal(err)
				}
				*commands = nil
				contrastReply(&d.preview, colorResult("prepared", "", "not-applied"))
				if strings.Contains(strings.Join(*commands, "\n"), " apply ") {
					t.Fatal("stale file applied")
				}
			} else {
				activeColor(t, d, "palette", "ink")
				if phase == "confirm-file" {
					if err := os.WriteFile(d.statePath, []byte("{}"), 0600); err != nil {
						t.Fatal(err)
					}
					if err := d.preview.action(d.preview.control.view, "save"); err == nil {
						t.Fatal("stale file confirmed")
					}
				} else {
					if err := d.preview.request("query"); err != nil {
						t.Fatal(err)
					}
					contrastReply(&d.preview, contrastResult("active", "", "pending"))
					if d.preview.control != nil || d.saved["palette"] != "slate" {
						t.Fatal("old kind was accepted as version2")
					}
				}
			}
		})
	}
}
func TestResourcePlanRejectsUnknownVersion(t *testing.T) {
	d, _ := colorFixture(t, "normal")
	control := colorControl(t, d, "palette", "ink")
	if _, err := resourcePlan(control.resource.projection, 3); err == nil {
		t.Fatal("unknown version encoded")
	}
	if len(desktopsettings.Registry()) != 21 {
		t.Fatal("preview expanded registry")
	}
}

func TestOpaqueColorUnavailableCapabilityDoesNotPrepare(t *testing.T) {
	for _, cause := range []string{"missing", "occupied", "wrong-version"} {
		t.Run(cause, func(t *testing.T) {
			d, commands := colorFixture(t, "normal")
			if err := d.preview.startResource("palette", "ink", nil); err != nil {
				t.Fatal(err)
			}
			response := colorAppearance(cause != "occupied")
			body := response["result"].(map[string]any)
			if cause == "missing" {
				delete(body, "opaque_colors_preview")
			}
			if cause == "wrong-version" {
				body["opaque_colors_preview"].(map[string]any)["plan_version"] = 1
			}
			*commands = nil
			contrastReply(&d.preview, response)
			if d.preview.control != nil || strings.Contains(strings.Join(*commands, "\n"), " prepare ") {
				t.Fatal("unsupported preview was prepared")
			}
		})
	}
}
func TestOpaqueColorTerminalPathsRetainCompleteDraftWithoutSave(t *testing.T) {
	for _, reason := range []string{"reverted", "expired", "conflicted", "refused", "abandoned"} {
		t.Run(reason, func(t *testing.T) {
			d, commands := colorFixture(t, "normal")
			activeColor(t, d, "palette", "ink")
			plan, err := desktopsettings.Editor(d.snapshot(nil, nil), "all", false, nil, newID(16))
			if err != nil || len(plan.Keys) != 11 {
				t.Fatal("appearance editor scope changed", plan.Keys, err)
			}
			captured := values{}
			for _, key := range plan.Keys {
				captured[key] = d.saved[key]
			}
			captured["palette"], captured["motion"] = "ink", true
			d.preview.control.editor = &draft{scope: "all", generation: d.generation, values: captured}
			if err := d.preview.request("query"); err != nil {
				t.Fatal(err)
			}
			recovery := "restored"
			if reason == "conflicted" {
				recovery = "refused-conflict"
			}
			if reason == "refused" {
				recovery = "not-applied"
			}
			*commands = nil
			contrastReply(&d.preview, colorResult("terminal", reason, recovery))
			if d.preview.control != nil || d.editor == nil || d.saved["palette"] != "slate" || d.effective["palette"] != "slate" || len(d.runtimeHeld) != 0 {
				t.Fatal("terminal resource path saved or lost editor", reason)
			}
			if !reflect.DeepEqual(d.editor.keys, plan.Keys) {
				t.Fatal("terminal path returned a different editor scope", d.editor.keys)
			}
			if _, err := os.Stat(d.statePath); !os.IsNotExist(err) {
				t.Fatal("terminal recovery wrote preferences", err)
			}
			body := strings.Join(*commands, "\n")
			if !strings.Contains(body, "Choice palette_ink ink on") || !strings.Contains(body, "Choice motion_true true on") {
				t.Fatal("terminal path lost captured draft")
			}
		})
	}
}

func TestValidateAttributesOnlyInheritedRuntimeHolds(t *testing.T) {
	d, _ := colorFixture(t, "normal")
	lock := filepath.Join(filepath.Dir(d.statePath), ".desktop-state.lock")
	if err := os.Mkdir(lock, 0700); err != nil {
		t.Fatal(err)
	}
	for _, item := range [][2]string{{"palette", "ink"}, {"appearance", "light"}} {
		if d.confirmResource(colorControl(t, d, item[0], item[1])) == nil {
			t.Fatal("write refusal missing")
		}
	}
	before := merged(d.effective, nil)
	for _, changes := range []values{{"palette": "dune"}, {"palette": "ink"}, {"motion": true}} {
		candidate, effective, err := d.prepare(changes)
		if err != nil {
			t.Fatal(err)
		}
		query, err := d.query("all", candidate, effective)
		if err != nil {
			t.Fatal(err)
		}
		palette := query["palette"].(map[string]any)
		_, replacing := changes["palette"]
		if (palette["source"] == "runtime-kept-preference-unsaved") == replacing {
			t.Fatal("validate misattributed replacement or inherited palette hold", changes, palette)
		}
		appearance := query["appearance"].(map[string]any)
		if appearance["source"] != "runtime-kept-preference-unsaved" {
			t.Fatal("unrelated candidate lost appearance hold", changes, appearance)
		}
		if !reflect.DeepEqual(before, d.effective) || len(d.runtimeHeld) != 2 {
			t.Fatal("validation mutated observed held state")
		}
	}
	observed, err := d.query("all", nil, nil)
	if err != nil {
		t.Fatal(err)
	}
	for _, key := range []string{"palette", "appearance"} {
		if observed[key].(map[string]any)["source"] != "runtime-kept-preference-unsaved" {
			t.Fatal("observed snapshot lost hold", key)
		}
	}
	snapshot := d.snapshot(nil, nil)
	delete(snapshot.RuntimeHeld, "palette")
	if !d.runtimeUnsaved("palette") {
		t.Fatal("snapshot annotation borrowed mutable controller map")
	}
}

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
	"time"

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
func colorAppearance(available bool) values { return contrastAppearance(available) }
func colorResult(state, reason, recovery string) values {
	return contrastResult(state, reason, recovery)
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
				if control.resource.scope != "complete-bundle" || control.resource.planVersion != 3 ||
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
	for _, reason := range []string{"unchanged", "override", "file", "unsubmitted", "unknown"} {
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
				key, value = "motion", "true"
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
				if !strings.Contains(strings.Join(*commands, "\n"), " - 0 15000 3:") {
					t.Fatal("version3 plan was not submitted")
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
					oldResult := contrastResult("active", "", "pending")
					oldResult["result"].(map[string]any)["plan_version"] = 1
					contrastReply(&d.preview, oldResult)
					if !d.preview.recovering || d.saved["palette"] != "slate" {
						t.Fatal("old kind was accepted as version3")
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
	if len(desktopsettings.Registry()) != 26 {
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
				delete(body, "complete_bundle_preview")
			}
			if cause == "wrong-version" {
				body["complete_bundle_preview"].(map[string]any)["plan_version"] = 1
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

// Completion contract: each selected value is typed, bounded and owns only its
// one preference, even when the complete resource projection changes wallpaper.
func TestCompleteBundlePlanAndTypedCandidates(t *testing.T) {
	for _, item := range []struct {
		key   string
		value any
	}{
		{"palette", "ink"}, {"appearance", "light"}, {"contrast", "high"},
		{"transparency", true}, {"wallpaper_palette", "dune"},
		{"wallpaper_appearance", "dark"},
	} {
		t.Run(item.key, func(t *testing.T) {
			d, commands := colorFixture(t, "normal")
			before := merged(d.effective, nil)
			*commands = nil
			_, candidate, err := d.prepareResource(item.key, item.value)
			if err != nil {
				t.Fatal(err)
			}
			plan, err := candidate.nativePlan()
			if err != nil || candidate.planVersion != 3 || candidate.scope != "complete-bundle" || len(plan) > 701 || !strings.HasPrefix(plan, "3:") {
				t.Fatal("invalid complete bundle", plan, err)
			}
			fields := strings.Split(plan, ":")
			if len(fields) != 7 || len(fields[1]) != 336 || len(fields[2]) != 2 || len(fields[3]) != 3 || len(fields[4]) != 3 || len(fields[5]) != 6 {
				t.Fatal("wrong finite wire shape", fields)
			}
			if !reflect.DeepEqual(before, d.effective) || len(*commands) != 0 {
				t.Fatal("preparation published state")
			}
		})
	}
}

func TestCompleteBundlePathAndFlagBounds(t *testing.T) {
	d, commands := colorFixture(t, "normal")
	d.effective["wallpaper_palette"] = "follow"
	_ = d.theme(true)
	_, candidate, err := d.prepareResource("palette", "ink")
	if err != nil {
		t.Fatal(err)
	}
	candidate.path = strings.Repeat("p", 256)
	plan, err := candidate.nativePlan()
	if err != nil || len(plan) != 701 {
		t.Fatal("maximum path bound", len(plan), err)
	}
	*commands = nil
	if err := d.preview.submit("prepare", "Send_Preview {cookie} {serial} "+newID(8)+" prepare "+strings.Repeat("a", 48)+" - 0 15000 "+plan); err != nil {
		t.Fatal(err)
	}
	if len(*commands) != 1 || len((*commands)[0]) > 880 {
		t.Fatal("complete native command exceeded frozen bound")
	}
	candidate.path += "x"
	if _, err := candidate.nativePlan(); err == nil {
		t.Fatal("oversized pathname encoded")
	}
	candidate.path = "a\x00b"
	if _, err := candidate.nativePlan(); err == nil {
		t.Fatal("NUL pathname encoded")
	}
	candidate.path = ""
	candidate.rootMode = 3
	if _, err := candidate.nativePlan(); err == nil {
		t.Fatal("invalid root mode encoded")
	}
}

func TestCompleteBundleNoVisibleChangeRefusesBeforeMutation(t *testing.T) {
	d, commands := colorFixture(t, "high")
	*commands = nil
	for _, item := range []struct {
		key   string
		value any
	}{{"transparency", false}, {"launcher_opacity", 52}, {"keep_wallpaper", true}} {
		if err := d.preview.startResource(item.key, item.value, nil); err == nil || d.preview.control != nil || len(*commands) != 0 {
			t.Fatal("invisible candidate acquired preview", item)
		}
	}
}

func TestCompleteBundleRecoveryFencesThemeUntilAcknowledgement(t *testing.T) {
	d, commands := colorFixture(t, "normal")
	activeColor(t, d, "palette", "ink")
	*commands = nil
	if err := d.changeSettings(values{"motion": true}); err != nil {
		t.Fatal(err)
	}
	noThemeReplay(t, *commands)
	if d.preview.control == nil || !d.preview.recovering {
		t.Fatal("recovery reply was discarded")
	}
	*commands = nil
	contrastReply(&d.preview, colorResult("terminal", "abandoned", "restored"))
	if d.preview.control != nil || d.preview.blocked {
		t.Fatal("acknowledged recovery stayed fenced")
	}
	if !strings.Contains(strings.Join(*commands, "\n"), "Colorset ") {
		t.Fatal("deferred theme was not replayed")
	}
}

func TestCompleteBundleUnknownRecoveryNeverReplays(t *testing.T) {
	d, commands := colorFixture(t, "normal")
	activeColor(t, d, "palette", "ink")
	_ = d.changeSettings(values{"motion": true})
	d.preview.pending.deadline = time.Now().Add(-time.Second)
	*commands = nil
	d.preview.tick()
	_ = d.theme(true)
	noThemeReplay(t, *commands)
	if !d.preview.blocked {
		t.Fatal("unknown recovery released theme fence")
	}
}

func heldTestValue(t *testing.T, key string, value any) desktopsettings.PreviewValue {
	t.Helper()
	v, err := desktopsettings.PreviewValueFor(key, value)
	if err != nil {
		t.Fatal(err)
	}
	return v
}

func TestCompleteBundleAutoCapturesCandidatePhase(t *testing.T) {
	d, _ := colorFixture(t, "normal")
	snapshot, candidate, err := d.prepareResourceAt("appearance", "auto", 720)
	if err != nil || candidate.theme[1] != "light" {
		t.Fatal("current Auto phase not captured", err)
	}
	control := &previewControl{saved: snapshot, resource: candidate}
	if err := d.checkResourceAt(control, 720); err != nil {
		t.Fatal(err)
	}
	if err := d.checkResourceAt(control, 0); err == nil {
		t.Fatal("candidate phase race admitted")
	}
	if candidate.theme[1] != "light" || d.effective["appearance"] != "dark" {
		t.Fatal("phase recheck mutated captured state")
	}
}

func TestCompleteBundleBothContrastAndTransparencyDirections(t *testing.T) {
	for _, contrast := range []string{"normal", "high"} {
		d, _ := colorFixture(t, contrast)
		value := "high"
		if contrast == "high" {
			value = "normal"
		}
		_, candidate, err := d.prepareResource("contrast", value)
		if err != nil || candidate.preserveMenu {
			t.Fatal("contrast did not own both resolved scalars", err)
		}
		if contrast == "high" && (!candidate.transparent || candidate.menuOpacity != 94 || candidate.launcherOpacity != 73) {
			t.Fatal("normal contrast failed to restore requested transparency")
		}
		if contrast == "normal" && (candidate.transparent || candidate.menuOpacity != 100 || candidate.launcherOpacity != 100) {
			t.Fatal("high contrast did not resolve opaque scalars")
		}
	}
	for _, before := range []bool{false, true} {
		d, _ := colorFixture(t, "normal")
		d.effective["transparency"] = before
		_ = d.theme(true)
		_, candidate, err := d.prepareResource("transparency", !before)
		if err != nil || candidate.transparent == before || candidate.preserveMenu {
			t.Fatal("transparency direction failed", err)
		}
	}
}

func TestCompleteBundleFollowRetainIndependentAndPlainBackground(t *testing.T) {
	for _, png := range []bool{false, true} {
		d, _ := colorFixture(t, "normal")
		d.png = png
		d.effective["wallpaper_palette"] = "follow"
		_ = d.theme(true)
		_, candidate, err := d.prepareResource("palette", "ink")
		mode := 1
		if png {
			mode = 2
		}
		if err != nil || candidate.rootMode != mode || !candidate.preserveMenu {
			t.Fatal("followed wallpaper missing", err)
		}
		d.effective["keep_wallpaper"] = true
		_ = d.theme(true)
		_, candidate, err = d.prepareResource("palette", "ink")
		if err != nil || candidate.rootMode != 0 || candidate.path != "" {
			t.Fatal("retained wallpaper was replaced", err)
		}
		_, candidate, err = d.prepareResource("keep_wallpaper", false)
		// Retained source remains slate while the desired followed palette is
		// made different without publishing over the retained root.
		if err == nil {
			t.Fatal("unchanged unretain should refuse")
		}
		d.effective["palette"] = "ink"
		_ = d.theme(true)
		_, candidate, err = d.prepareResource("keep_wallpaper", false)
		if err != nil || candidate.rootMode != mode || candidate.value != false {
			t.Fatal("unretain did not prepare desired wallpaper", err)
		}
	}
}

func TestScalarSavedOpacityFailureRetainsTypedOverlay(t *testing.T) {
	d, commands := fixture(t)
	activePreview(t, d, true)
	if err := d.preview.action(d.preview.control.view, "save"); err != nil {
		t.Fatal(err)
	}
	lock := filepath.Join(filepath.Dir(d.statePath), ".desktop-state.lock")
	if err := os.Mkdir(lock, 0700); err != nil {
		t.Fatal(err)
	}
	*commands = nil
	replyTo(&d.preview, success(values{"scope": "menu-opacity", "state": "confirmed"}))
	if !d.runtimeUnsaved("launcher_opacity") || d.runtimeHeld["launcher_opacity"].Value() != 63 || d.effective["launcher_opacity"] != 63 || d.saved["launcher_opacity"] != 94 {
		t.Fatal("confirmed scalar choice lost on save failure")
	}
	noThemeReplay(t, *commands)
}

func TestCompleteBundleFurtherSettingsChangeRetainsRecoveryChannel(t *testing.T) {
	d, _ := colorFixture(t, "normal")
	activeColor(t, d, "palette", "ink")
	if err := d.changeSettings(values{"motion": true}); err != nil {
		t.Fatal(err)
	}
	pending := d.preview.pending
	if err := d.changeSettings(values{"motion": false}); err != nil {
		t.Fatal(err)
	}
	if d.preview.blocked || d.preview.pending != pending || !d.preview.recovering {
		t.Fatal("later settings edit discarded recovery acknowledgement")
	}
	contrastReply(&d.preview, colorResult("terminal", "abandoned", "restored"))
	if d.preview.control != nil || d.lastTheme[3] != false {
		t.Fatal("recovery replay did not use latest settings")
	}
}

func TestCompleteBundleChoiceMenuCapturesOneFullDraft(t *testing.T) {
	d, commands := colorFixture(t, "normal")
	argument, captured := contrastEditorDraft(t, d)
	*commands = nil
	if err := d.handle("preview-choose-editor", argument); err != nil {
		t.Fatal(err)
	}
	body := strings.Join(*commands, "\n")
	for _, key := range []string{"palette", "appearance", "contrast", "transparency", "keep_wallpaper", "wallpaper_palette", "wallpaper_appearance", "launcher_opacity"} {
		if !strings.Contains(body, "preview-resource-editor "+key+" "+argument) {
			t.Fatal("missing scoped choice", key, body)
		}
	}
	if strings.Count(body, "preview-resource-editor ") != 8 || d.editor == nil || d.preview.control != nil {
		t.Fatal("choice opened a lease or retired draft")
	}
	// Copy the current palette/font first, make only the chooser opaque,
	// then bind it before mapping. No global or launcher style may change.
	styleCommands := []string{
		"CopyMenuStyle * SlateSettingsPreviewChoiceStyle",
		"MenuStyle SlateSettingsPreviewChoiceStyle !Translucent",
		"ChangeMenuStyle SlateSettingsPreviewChoiceStyle SlateSettingsPreviewChoice",
	}
	previous, count := -1, 0
	popup := -1
	for index, command := range *commands {
		if command == "Popup SlateSettingsPreviewChoice" {
			popup = index
		}
		if !strings.HasPrefix(command, "CopyMenuStyle ") &&
			!strings.HasPrefix(command, "MenuStyle ") &&
			!strings.HasPrefix(command, "ChangeMenuStyle ") {
			continue
		}
		if count >= len(styleCommands) || command != styleCommands[count] || index <= previous {
			t.Fatal("chooser altered unrelated style or applied its style out of order", command)
		}
		previous, count = index, count+1
	}
	if count != len(styleCommands) || popup <= previous {
		t.Fatal("chooser mapped before its opaque private style", *commands)
	}
	if err := d.handle("preview-resource-editor", "contrast "+argument); err != nil {
		t.Fatal(err)
	}
	if d.preview.control.resource.value != "high" || !reflect.DeepEqual(d.preview.control.editor.values, captured) {
		t.Fatal("selected candidate consumed unrelated draft")
	}
}

func TestCompleteBundleTypedSavesAndFailuresKeepOnlyOnePreference(t *testing.T) {
	for _, fail := range []bool{false, true} {
		for _, item := range []struct {
			key   string
			value any
		}{
			{"palette", "ink"}, {"appearance", "light"}, {"contrast", "high"}, {"transparency", true},
			{"keep_wallpaper", false}, {"wallpaper_palette", "dune"}, {"wallpaper_appearance", "dark"},
		} {
			t.Run(fmt.Sprint(fail)+"/"+item.key, func(t *testing.T) {
				d, commands := colorFixture(t, "normal")
				if item.key == "keep_wallpaper" {
					d.saved["keep_wallpaper"], d.effective["keep_wallpaper"] = true, true
					d.saved["wallpaper_palette"], d.effective["wallpaper_palette"] = "follow", "follow"
					if err := d.theme(true); err != nil {
						t.Fatal(err)
					}
				}
				before := merged(d.saved, nil)
				snapshot, candidate, err := d.prepareResource(item.key, item.value)
				if err != nil {
					t.Fatal(err)
				}
				if fail {
					if err := os.Mkdir(filepath.Join(filepath.Dir(d.statePath), ".desktop-state.lock"), 0700); err != nil {
						t.Fatal(err)
					}
				}
				*commands = nil
				err = d.confirmResource(&previewControl{saved: snapshot, resource: candidate})
				if (err != nil) != fail || d.runtimeUnsaved(item.key) != fail || d.effective[item.key] != item.value {
					t.Fatal("typed confirmation outcome", err)
				}
				if !fail {
					before[item.key] = item.value
				}
				if !reflect.DeepEqual(d.saved, before) {
					t.Fatal("confirmation saved unrelated preferences")
				}
				if d.lastWallpaper != [2]string{candidate.theme[5].(string), candidate.theme[6].(string)} {
					t.Fatal("confirmed wallpaper cache is stale")
				}
				noThemeReplay(t, *commands)
				*commands = nil
				if err := d.theme(false); err != nil || len(*commands) != 0 {
					t.Fatal("confirmed typed projection replayed", err)
				}
			})
		}
	}
}

// Match resource_capability() in fvwm/appearance_preview.c: style_count is
// shared by all resource plans; the complete bundle adds no menu_count field.
func TestCompleteBundleNativeCapabilityUsesStyleCount(t *testing.T) {
	decoded, err := desktopsettings.DecodeJSON([]byte(`{
		"version":1,"scope":"complete-bundle","plan_version":3,
		"root_background":true,"root_owned":true,"buffered_transparency":true,
		"path_max":256,"token_max":701,"role_count":14,"style_count":2,
		"metadata_max":65536,"preparation_timeout_ms":5000,
		"lease_limit":1,"slot_available":true}`))
	if err != nil {
		t.Fatal(err)
	}
	capability := decoded.(map[string]any)
	candidate := &resourceCandidate{scope: "complete-bundle", planVersion: 3}
	if _, exists := capability["menu_count"]; exists {
		t.Fatal("fixture introduced duplicate native count")
	}
	if err := resourceCapability(capability, candidate); err != nil {
		t.Fatal("native capability refused", err)
	}
	capability["style_count"] = nil
	if err := resourceCapability(capability, candidate); err == nil {
		t.Fatal("missing native style count admitted")
	}
}

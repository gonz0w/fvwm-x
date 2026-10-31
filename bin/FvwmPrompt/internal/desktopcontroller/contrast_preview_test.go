// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
	"time"
)

func contrastReply(p *preview, result values) {
	data, _ := json.Marshal(result)
	prefix := "MFL_CORE_1 " + p.desktop.instance + " " + p.pending.serial + " "
	parts := 0
	for len(data) > 0 {
		n := min(256, len(data))
		p.reply(fmt.Sprintf("%spart %d %s", prefix, parts, hex.EncodeToString(data[:n])))
		parts++
		data = data[n:]
	}
	p.reply(fmt.Sprintf("%send %d", prefix, parts))
}

func contrastAppearance(available bool) values {
	return success(values{"generation": strings.Repeat("a", 48), "complete_bundle_preview": values{
		"version": 1, "scope": "complete-bundle", "plan_version": 3, "role_count": 14, "style_count": 2,
		"metadata_max": 65536, "preparation_timeout_ms": 5000, "lease_limit": 1, "slot_available": available,
		"root_background": true, "root_owned": true, "buffered_transparency": true, "path_max": 256, "token_max": 701}})
}

func contrastResult(state, reason, recovery string) values {
	generation := strings.Repeat("b", 48)
	if state == "preparing" || state == "prepared" || state == "applying" {
		generation = strings.Repeat("a", 48)
	}
	return success(values{"scope": "complete-bundle", "plan_version": 3,
		"state": state, "terminal_reason": reason, "recovery": recovery,
		"generation": generation, "lease": strings.Repeat("c", 48),
		"remaining_ms": 15000, "lease_limit": 1, "preparation_limit_ms": 5000})
}

func activeContrast(t *testing.T, d *Controller) {
	t.Helper()
	if err := d.preview.startContrast(nil); err != nil {
		t.Fatal(err)
	}
	activateContrastReplies(t, d)
}

func activateContrastReplies(t *testing.T, d *Controller) {
	t.Helper()
	contrastReply(&d.preview, contrastAppearance(true))
	contrastReply(&d.preview, contrastResult("prepared", "", "not-applied"))
	contrastReply(&d.preview, contrastResult("applying", "", "not-applied"))
	if err := d.preview.request("query"); err != nil {
		t.Fatal(err)
	}
	contrastReply(&d.preview, contrastResult("active", "", "pending"))
	if d.preview.control == nil || d.preview.control.resource.state != "active" {
		t.Fatal("contrast resource lease did not become active")
	}
}

func contrastEditorDraft(t *testing.T, d *Controller) (string, values) {
	t.Helper()
	if err := d.editSettings("all", false, nil); err != nil {
		t.Fatal(err)
	}
	argument := d.editor.token
	expected := values{}
	changes := values{"palette": "dune", "motion": true, "launcher_opacity": 61, "contrast": "high"}
	// Saved state also contains the internal retained-wallpaper pair. A Form
	// captures only its registry keys, never that non-editable state field.
	for _, key := range d.editor.keys {
		value := d.saved[key]
		if changed, found := changes[key]; found {
			value = changed
		}
		expected[key] = value
		argument += " " + fmt.Sprint(value)
	}
	return argument, expected
}

func contrastFixture(t *testing.T) (*Controller, *[]string, *previewControl) {
	t.Helper()
	d, commands := fixture(t)
	if err := d.theme(true); err != nil {
		t.Fatal(err)
	}
	snapshot, candidate, err := d.prepareResource("contrast", "high")
	if err != nil {
		t.Fatal(err)
	}
	return d, commands, &previewControl{view: newID(8), saved: snapshot, resource: candidate}
}

func noThemeReplay(t *testing.T, commands []string) {
	t.Helper()
	for _, command := range commands {
		for _, prefix := range []string{"Colorset ", "MenuStyle ", "CopyMenuStyle ", "Style * WindowShadeSteps"} {
			if strings.HasPrefix(command, prefix) {
				t.Fatal("resource preview replayed a theme side effect", command)
			}
		}
	}
}

func TestContrastCandidateOwnsOnlyFixedRoles(t *testing.T) {
	d, commands, control := contrastFixture(t)
	before := merged(d.effective, nil)
	*commands = nil
	if err := d.checkResource(control); err != nil {
		t.Fatal(err)
	}
	ids := []int{}
	for _, role := range control.resource.projection.Roles {
		ids = append(ids, role.Colorset)
	}
	if !reflect.DeepEqual(ids, []int{0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14}) ||
		!reflect.DeepEqual(d.effective, before) || len(*commands) != 0 {
		t.Fatal("candidate expanded its resource or state scope", ids)
	}
	control.resource.projection.Roles[0].Foreground = "#123456"
	if err := d.checkResource(control); err == nil {
		t.Fatal("modified candidate escaped pre-apply validation")
	}
}

func TestContrastCandidateRechecksGenerationPolicyAndFile(t *testing.T) {
	for _, cause := range []string{"generation", "override", "current-high", "file", "palette", "unsubmitted"} {
		t.Run(cause, func(t *testing.T) {
			d, commands, control := contrastFixture(t)
			switch cause {
			case "generation":
				d.generation = newID(16)
			case "override":
				d.overrides["contrast"] = "normal"
			case "current-high":
				d.effective["contrast"] = "high"
			case "file":
				if err := os.WriteFile(d.statePath, []byte("{}"), 0600); err != nil {
					t.Fatal(err)
				}
			case "palette":
				d.effective["palette"] = "dune"
			case "unsubmitted":
				d.lastTheme = nil
			}
			*commands = nil
			if err := d.checkResource(control); err == nil || len(*commands) != 0 {
				t.Fatal("stale candidate admitted or emitted commands")
			}
		})
	}
}

func TestContrastConfirmedSavesOnlyContrastAndRetainsDraft(t *testing.T) {
	d, commands, control := contrastFixture(t)
	before := merged(d.saved, nil)
	argument, draftBefore := contrastEditorDraft(t, d)
	e, changes, err := d.editorValues(argument)
	if err != nil {
		t.Fatal(err)
	}
	control.editor = &draft{scope: e.scope, generation: e.generation, values: changes}
	d.retireEditor()
	*commands = nil
	if err := d.confirmResource(control); err != nil {
		t.Fatal(err)
	}
	stored, _, err := loadSettings(d.statePath)
	before["contrast"] = "high"
	draftBefore["contrast"] = "high"
	if err != nil || !reflect.DeepEqual(stored, before) || !reflect.DeepEqual(control.editor.values, draftBefore) || d.runtimeUnsaved("contrast") {
		t.Fatal("confirmation saved unrelated drafts or lost draft values", stored, err)
	}
	noThemeReplay(t, *commands)
	*commands = nil
	if err := d.theme(false); err != nil || len(*commands) != 0 {
		t.Fatal("confirmation cache caused theme replay", err)
	}
}

func TestContrastConfirmedFileRaceKeepsRuntimeWithoutRetry(t *testing.T) {
	d, commands, control := contrastFixture(t)
	if err := os.WriteFile(d.statePath, []byte("{}"), 0600); err != nil {
		t.Fatal(err)
	}
	*commands = nil
	if err := d.confirmResource(control); err == nil {
		t.Fatal("external edit falsely reported saved")
	}
	data, _ := os.ReadFile(d.statePath)
	if string(data) != "{}" || d.saved["contrast"] != "normal" || d.effective["contrast"] != "high" || !d.runtimeUnsaved("contrast") {
		t.Fatal("save failure lost runtime or overwrote external edit")
	}
	noThemeReplay(t, *commands)
	*commands = nil
	if err := d.theme(false); err != nil || len(*commands) != 0 {
		t.Fatal("clock tick replaced runtime-kept high contrast", err)
	}
	query, err := d.query("contrast", nil, nil)
	if err != nil || query["contrast"].(map[string]any)["source"] != "runtime-kept-preference-unsaved" {
		t.Fatal("runtime-only state was hidden", query, err)
	}
}

func TestContrastConfirmedWriteFailureKeepsRuntimeAndUnrelatedEdits(t *testing.T) {
	d, commands, control := contrastFixture(t)
	lock := filepath.Join(filepath.Dir(d.statePath), ".desktop-state.lock")
	if err := os.Mkdir(lock, 0700); err != nil {
		t.Fatal(err)
	}
	*commands = nil
	if err := d.confirmResource(control); err == nil {
		t.Fatal("unwritable preference falsely reported saved")
	}
	if d.saved["contrast"] != "normal" || d.effective["contrast"] != "high" || !d.runtimeUnsaved("contrast") {
		t.Fatal("failed save did not retain honest runtime state")
	}
	noThemeReplay(t, *commands)
	if err := os.Remove(lock); err != nil {
		t.Fatal(err)
	}
	if err := d.changeSettings(values{"motion": true}); err != nil {
		t.Fatal(err)
	}
	if d.saved["contrast"] != "normal" || d.effective["contrast"] != "high" || !d.runtimeUnsaved("contrast") {
		t.Fatal("unrelated edit silently discarded runtime-kept contrast")
	}
	if err := d.reloadSettings(); err != nil || d.runtimeUnsaved("contrast") || d.effective["contrast"] != "normal" {
		t.Fatal("explicit reload did not replace runtime-only contrast", err)
	}
}

func TestContrastDirectedPrepareApplyConfirmAndSingleLease(t *testing.T) {
	d, commands, _ := contrastFixture(t)
	*commands = nil
	activeContrast(t, d)
	text := strings.Join(*commands, "\n")
	if !strings.Contains(text, " prepare "+strings.Repeat("a", 48)+" - 0 15000 3:") ||
		!strings.Contains(text, " apply "+strings.Repeat("a", 48)+" "+strings.Repeat("c", 48)+" 0 0 -") {
		t.Fatal("directed resource preparation or application missing", text)
	}
	for _, command := range *commands {
		if strings.Contains(command, " prepare ") {
			fields := strings.Fields(command)
			if len(fields) != 10 || len(fields[9]) > 701 || !strings.HasPrefix(fields[9], "3:") || !identity(strings.Split(fields[9], ":")[1], 336) {
				t.Fatal("resource plan exceeded fixed typed wire shape", command)
			}
		}
	}
	noThemeReplay(t, *commands)
	if err := d.preview.start(63, true, nil); err == nil {
		t.Fatal("opacity preview overlapped resource lease")
	}
	if err := d.preview.startContrast(nil); err == nil {
		t.Fatal("second resource preview overlapped lease")
	}
	control := d.preview.control
	if err := d.preview.action(control.view, "confirm"); err == nil {
		t.Fatal("resource UI admitted runtime-only confirm")
	}
	if err := d.preview.action(control.view, "save"); err != nil {
		t.Fatal(err)
	}
	contrastReply(&d.preview, contrastResult("recovery-pending", "", "pending"))
	if _, err := os.Stat(d.statePath); !os.IsNotExist(err) {
		t.Fatal("preference write preceded native confirmation")
	}
	if err := d.preview.request("query"); err != nil {
		t.Fatal(err)
	}
	*commands = nil
	contrastReply(&d.preview, contrastResult("terminal", "confirmed", "kept"))
	if d.preview.control != nil || d.saved["contrast"] != "high" || d.effective["launcher_opacity"] != 94 || d.effective["transparency"] != true {
		t.Fatal("resource confirmation lost requested state")
	}
	noThemeReplay(t, *commands)
}

func TestContrastRefusesUnavailableCapabilityAndOccupiedSlot(t *testing.T) {
	for _, available := range []bool{false, true} {
		t.Run(fmt.Sprint(available), func(t *testing.T) {
			d, commands, _ := contrastFixture(t)
			if err := d.preview.startContrast(nil); err != nil {
				t.Fatal(err)
			}
			*commands = nil
			response := contrastAppearance(available)
			if available {
				delete(response["result"].(map[string]any), "complete_bundle_preview")
			}
			contrastReply(&d.preview, response)
			if d.preview.control != nil || strings.Contains(strings.Join(*commands, "\n"), " prepare ") {
				t.Fatal("unavailable resource preview was submitted")
			}
		})
	}
}

func TestContrastRechecksBeforeApplyAndBeforeConfirm(t *testing.T) {
	for _, phase := range []string{"apply", "confirm"} {
		t.Run(phase, func(t *testing.T) {
			d, commands, _ := contrastFixture(t)
			if phase == "apply" {
				if err := d.preview.startContrast(nil); err != nil {
					t.Fatal(err)
				}
				contrastReply(&d.preview, contrastAppearance(true))
			} else {
				activeContrast(t, d)
			}
			if err := os.WriteFile(d.statePath, []byte("{}"), 0600); err != nil {
				t.Fatal(err)
			}
			*commands = nil
			if phase == "apply" {
				contrastReply(&d.preview, contrastResult("prepared", "", "not-applied"))
			} else if err := d.preview.action(d.preview.control.view, "save"); err == nil {
				t.Fatal("stale file admitted confirmation")
			}
			text := strings.Join(*commands, "\n")
			if strings.Contains(text, " apply ") || strings.Contains(text, " confirm ") || d.saved["contrast"] != "normal" {
				t.Fatal("file race escaped apply/confirm check", text)
			}
		})
	}
}

func TestContrastPostConfirmationRaceAndCoreLossPreserveDrafts(t *testing.T) {
	for _, outcome := range []string{"file-race", "core-loss", "cancel", "refused"} {
		t.Run(outcome, func(t *testing.T) {
			d, commands, _ := contrastFixture(t)
			argument, expectedDraft := contrastEditorDraft(t, d)
			if err := d.handle("preview-contrast-editor", argument); err != nil {
				t.Fatal(err)
			}
			if d.editor != nil || d.preview.control == nil || d.preview.control.editor == nil ||
				!reflect.DeepEqual(d.preview.control.editor.values, expectedDraft) {
				t.Fatal("actual editor did not transfer its complete draft")
			}
			activateContrastReplies(t, d)
			view := d.preview.control.view
			switch outcome {
			case "file-race":
				if err := d.preview.action(view, "save"); err != nil {
					t.Fatal(err)
				}
				if err := os.WriteFile(d.statePath, []byte("{}"), 0600); err != nil {
					t.Fatal(err)
				}
				*commands = nil
				contrastReply(&d.preview, contrastResult("terminal", "confirmed", "kept"))
				data, _ := os.ReadFile(d.statePath)
				if string(data) != "{}" || !d.runtimeUnsaved("contrast") || d.effective["contrast"] != "high" {
					t.Fatal("confirmed external edit did not stay runtime-only")
				}
			case "core-loss":
				if err := d.preview.request("query"); err != nil {
					t.Fatal(err)
				}
				d.preview.pending.deadline = time.Now().Add(-time.Second)
				d.preview.tick()
				// Recovery itself timed out: return the draft and retain the fence.
				d.preview.pending.deadline = time.Now().Add(-time.Second)
				d.preview.tick()
			case "cancel", "refused":
				if err := d.preview.action(view, "revert"); err != nil {
					t.Fatal(err)
				}
				reason, recovery := "reverted", "restored"
				if outcome == "refused" {
					reason, recovery = "refused", "not-applied"
				}
				contrastReply(&d.preview, contrastResult("terminal", reason, recovery))
			}
			if d.preview.control != nil || d.editor == nil || d.saved["contrast"] != "normal" {
				t.Fatal("terminal preview lost captured editor or falsely saved")
			}
			lines := strings.Join(*commands, "\n")
			for _, required := range []string{"Choice palette_dune dune on", "Choice motion_true true on", `Input launcher_opacity 3 "61"`} {
				if !strings.Contains(lines, required) {
					t.Fatal("unrelated editor draft lost", required)
				}
			}
		})
	}
}

func TestContrastRejectsMalformedNativeStateWithoutSaving(t *testing.T) {
	for _, field := range []string{"plan_version", "lease", "generation", "remaining_ms", "preparation_limit_ms", "state"} {
		t.Run(field, func(t *testing.T) {
			d, commands, _ := contrastFixture(t)
			activeContrast(t, d)
			if err := d.preview.request("query"); err != nil {
				t.Fatal(err)
			}
			response := contrastResult("active", "", "pending")
			result := response["result"].(map[string]any)
			result[field] = "invalid"
			if field == "state" {
				result[field] = "prepared"
			}
			*commands = nil
			contrastReply(&d.preview, response)
			if !d.preview.recovering || d.saved["contrast"] != "normal" ||
				!strings.Contains(strings.Join(*commands, "\n"), " abandon ") {
				t.Fatal("malformed resource state escaped bounded recovery")
			}
		})
	}
}

func TestContrastReplacementControllerNeverInheritsLease(t *testing.T) {
	d, _, _ := contrastFixture(t)
	activeContrast(t, d)
	oldInstance, oldView := d.instance, d.preview.control.view
	commands := []string{}
	replacement, err := New(d.root, d.statePath, "Linux", "unused", "unused", true,
		func(command string) error { commands = append(commands, command); return nil })
	if err != nil {
		t.Fatal(err)
	}
	defer replacement.Close()
	if replacement.instance == oldInstance || replacement.preview.control != nil ||
		replacement.runtimeUnsaved("contrast") || replacement.preview.pending != nil {
		t.Fatal("replacement inherited another controller's runtime ownership")
	}
	replacement.Handle("preview-control " + oldInstance + " " + oldView + " save")
	replacement.preview.reply("MFL_CORE_1 " + oldInstance + " 0000000000000001 end 1")
	if len(commands) != 0 || replacement.saved["contrast"] != "normal" {
		t.Fatal("old actor/view response mutated replacement")
	}
}

func TestContrastLateReplyCannotPrepareAfterDeadline(t *testing.T) {
	d, commands, _ := contrastFixture(t)
	if err := d.preview.startContrast(nil); err != nil {
		t.Fatal(err)
	}
	d.preview.pending.deadline = time.Now().Add(-time.Second)
	*commands = nil
	contrastReply(&d.preview, contrastAppearance(true))
	if d.preview.control != nil || strings.Contains(strings.Join(*commands, "\n"), " prepare ") {
		t.Fatal("late appearance result started native preparation")
	}
}

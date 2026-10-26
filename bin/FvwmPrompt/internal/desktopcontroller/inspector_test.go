// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"reflect"
	"strconv"
	"strings"
	"testing"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopinspect"
)

func inspectionResult(work *inspectionWork) desktopinspect.Result {
	request := work.request
	result := desktopinspect.Result{SchemaVersion: 1, Scope: request.Scope,
		Filter: request.Filter, Provider: request.Provider,
		SettingsGeneration: request.SettingsGeneration, Sections: []desktopinspect.Section{}}
	for _, scope := range inspectionScopes[1:] {
		if request.Scope != "all" && request.Scope != scope {
			continue
		}
		result.Sections = append(result.Sections, desktopinspect.Section{ID: scope,
			Title: title(scope), State: "observed", Generation: "fixture",
			Rows: []desktopinspect.Row{{Label: "One", Value: "Read-only"},
				{Label: "Two"}, {Label: "Three"}, {Label: "Four"}, {Label: "Five"}}})
	}
	return result
}
func finishInspectionFixture(d *Controller, value any, err error) {
	d.cancelQueued("inspection")
	delete(d.jobs, "inspection") // pollJobs releases the worker key before publication.
	d.finishInspection(job{key: "inspection", result: value, err: err})
}
func inspectorCallback(d *Controller, action string) error {
	token := d.inspection.frame
	if action == "close" {
		token = d.inspection.token
	}
	return d.handle("inspect-control", token+" "+action)
}
func TestInspectorRequestUsesOwnerSnapshotAndClonedPolicy(t *testing.T) {
	d, commands := fixture(t)
	if err := d.theme(true); err != nil {
		t.Fatal(err)
	}
	if err := d.editSettings("palette", false, nil); err != nil {
		t.Fatal(err)
	}
	editor := d.editor
	control, pending := &previewControl{}, &previewPending{}
	d.preview.control, d.preview.pending = control, pending
	before, _ := json.Marshal(d.snapshot(nil, nil))
	generation, theme := d.generation, append([]any(nil), d.lastTheme...)
	*commands = nil
	if err := d.handle("inspect", ""); err != nil {
		t.Fatal(err)
	}
	work := d.inspectionJob
	if work == nil || work.request.Scope != "all" || work.request.Provider != d.instance || work.request.SettingsGeneration != generation {
		t.Fatal("inspector lost owner identity", work)
	}
	settings, _ := d.query("all", nil, nil)
	settingsJSON, _ := json.Marshal(settings)
	if string(work.request.Settings) != string(settingsJSON) {
		t.Fatal("inspector invented a second settings snapshot")
	}
	source, err := d.literalSourceRequest()
	if err != nil || work.request.Source != source {
		t.Fatal("Check and inspector source scope differ", err)
	}
	color := work.request.Colors["surface"]
	d.lastThemeColors["surface"] = "#123456"
	if work.request.Colors["surface"] != color {
		t.Fatal("worker request aliases owner theme policy")
	}
	for _, command := range *commands {
		if strings.Contains(command, "Module FvwmMFL") || strings.HasPrefix(command, "Colorset ") || strings.Contains(command, "preview-control") {
			t.Fatal("inspection mutated another owner", command)
		}
	}
	after, _ := json.Marshal(d.snapshot(nil, nil))
	if string(before) != string(after) || generation != d.generation || !reflect.DeepEqual(theme, d.lastTheme) || d.editor != editor || d.preview.control != control || d.preview.pending != pending {
		t.Fatal("inspection changed settings, editor or preview")
	}
}
func TestInspectorClosedAndReplacedViewsIgnoreLateJobsAndControls(t *testing.T) {
	d, commands := fixture(t)
	if err := d.startInspection("settings"); err != nil {
		t.Fatal(err)
	}
	old := d.inspectionJob
	oldToken := d.inspection.token
	// Model a running worker: close cannot cancel/reuse its ownership record.
	d.queue = nil
	d.jobs["inspection"].running = true
	if err := inspectorCallback(d, "close"); err != nil {
		t.Fatal(err)
	}
	count := len(*commands)
	finishInspectionFixture(d, inspectionResult(old), nil)
	if d.inspection != nil || len(*commands) != count {
		t.Fatal("late result reopened a closed inspector")
	}
	if err := d.startInspection("settings"); err != nil {
		t.Fatal(err)
	}
	old = d.inspectionJob
	oldToken = d.inspection.token
	d.queue = nil
	d.jobs["inspection"].running = true
	if err := d.startInspection("theme"); err != nil {
		t.Fatal(err)
	}
	replacement := d.inspection.token
	if err := d.inspectionControl(oldToken + " close"); err != nil || d.inspection == nil {
		t.Fatal("retired callback acted on replacement", err)
	}
	finishInspectionFixture(d, inspectionResult(old), nil)
	if d.inspection.token != replacement || d.inspection.result != nil || d.inspectionJob == nil || d.inspectionJob.request.Scope != "theme" || len(d.jobs) != 1 {
		t.Fatal("old view published or replacement work was lost")
	}
	finishInspectionFixture(d, inspectionResult(d.inspectionJob), nil)
	if d.inspection.result == nil || d.inspection.result.Scope != "theme" {
		t.Fatal("replacement snapshot did not publish")
	}
}
func TestInspectorPagesFilterAndRefreshAreBoundToRenderedView(t *testing.T) {
	d, commands := fixture(t)
	if err := d.startInspection("settings"); err != nil {
		t.Fatal(err)
	}
	finishInspectionFixture(d, inspectionResult(d.inspectionJob), nil)
	oldFrame := d.inspection.frame
	if err := inspectorCallback(d, "page 1"); err != nil || d.inspection.page != 1 {
		t.Fatal("second page unavailable", err)
	}
	if err := d.inspectionControl(oldFrame + " page 0"); err != nil || d.inspection.page != 1 {
		t.Fatal("old page callback remained live", err)
	}
	filter := "two words; $(Exec touch sentinel)"
	encoded := "x" + base64.RawURLEncoding.EncodeToString([]byte(filter))
	if err := inspectorCallback(d, "filter commands "+encoded); err != nil {
		t.Fatal(err)
	}
	if d.inspectionJob.request.Filter != filter || d.inspectionJob.request.Scope != "commands" || d.inspection.page != 0 {
		t.Fatal("encoded filter did not remain data")
	}
	for i := 0; i < 10; i++ {
		if err := inspectorCallback(d, "refresh"); err != nil {
			t.Fatal(err)
		}
	}
	if len(d.jobs) != 1 || len(d.queue) != 1 || !d.inspection.pending {
		t.Fatal("refresh work was not bounded")
	}
	finishInspectionFixture(d, inspectionResult(d.inspectionJob), nil)
	if len(d.jobs) != 1 || d.inspection.pending || d.inspectionJob == nil {
		t.Fatal("coalesced refresh not admitted once")
	}
	finishInspectionFixture(d, inspectionResult(d.inspectionJob), nil)
	if len(d.jobs) != 0 || d.inspectionJob != nil {
		t.Fatal("refresh did not drain")
	}
	joined := strings.Join(*commands, "\n")
	for _, fragment := range []string{`Title "Configuration inspector"`, "Colorset 2", "ItemColorset 12", "Previous (F1)", "Next (F2)", "Refresh (F5)", `Button quit "Close" ^[`, "CloseCommand SendToModule FvwmDesktop inspect-control", "x$(inspect_filter:Base64)"} {
		if !strings.Contains(joined, fragment) {
			t.Fatal("missing native inspector control", fragment)
		}
	}
	for _, invalid := range []string{"x_", "x" + base64.RawURLEncoding.EncodeToString([]byte(strings.Repeat("é", 65))), "x" + base64.RawURLEncoding.EncodeToString([]byte("line\nline"))} {
		if err := inspectorCallback(d, "filter settings "+invalid); err == nil || len(d.jobs) != 0 {
			t.Fatal("invalid filter admitted", invalid)
		}
	}
}
func TestInspectorRejectsInvalidResultsAndRetainsStaleSnapshot(t *testing.T) {
	for _, failure := range []string{"worker", "provider", "generation", "scope", "filter", "row", "unknown"} {
		t.Run(failure, func(t *testing.T) {
			d, commands := fixture(t)
			if err := d.startInspection("settings"); err != nil {
				t.Fatal(err)
			}
			finishInspectionFixture(d, inspectionResult(d.inspectionJob), nil)
			previous := d.inspection.result
			if err := inspectorCallback(d, "refresh"); err != nil {
				t.Fatal(err)
			}
			result := inspectionResult(d.inspectionJob)
			var value any = result
			var err error
			switch failure {
			case "worker":
				err = errors.New("secret /home/alice/private/path")
			case "provider":
				result.Provider = "another-provider"
			case "generation":
				result.SettingsGeneration = "another-generation"
			case "scope":
				result.Scope = "theme"
			case "filter":
				result.Filter = "wrong"
			case "row":
				result.Sections[0].Rows[0].Value = strings.Repeat("a", 201)
			case "unknown":
				data, _ := json.Marshal(result)
				var fields map[string]any
				_ = json.Unmarshal(data, &fields)
				fields["extra"] = "unvalidated"
				value = fields
			}
			if failure != "unknown" {
				value = result
			}
			finishInspectionFixture(d, value, err)
			if d.inspection.result != previous || !strings.Contains(d.inspection.status, "stale") || strings.Contains(strings.Join(*commands, "\n"), "secret") {
				t.Fatal("invalid snapshot published or private error exposed")
			}
		})
	}
}
func TestInspectorCollectionAndRetainedSnapshotsExposeGenerationChanges(t *testing.T) {
	for _, changed := range []string{"settings", "provider", "theme"} {
		t.Run(changed, func(t *testing.T) {
			d, _ := fixture(t)
			if err := d.theme(true); err != nil {
				t.Fatal(err)
			}
			if err := d.startInspection("settings"); err != nil {
				t.Fatal(err)
			}
			result := inspectionResult(d.inspectionJob)
			switch changed {
			case "settings":
				d.changed(newID(16))
			case "provider":
				d.instance = newID(16)
			case "theme":
				d.lastTheme[1] = "changed-clock-phase"
			}
			finishInspectionFixture(d, result, nil)
			if d.inspection.result == nil || !strings.Contains(d.inspection.status, "Stale:") {
				t.Fatal("old generation displayed as fresh")
			}
			d.changed(newID(16))
			if !strings.Contains(d.inspection.status, "Settings changed") {
				t.Fatal("retained snapshot did not expose owner change")
			}
		})
	}
}
func TestInspectorPollHandlesFailureWithoutReopeningClosedView(t *testing.T) {
	d, commands := fixture(t)
	if err := d.startInspection("all"); err != nil {
		t.Fatal(err)
	}
	d.queue = nil
	d.jobs["inspection"].running = true
	d.active = 1
	if err := inspectorCallback(d, "close"); err != nil {
		t.Fatal(err)
	}
	count := len(*commands)
	d.completed <- job{key: "inspection", err: errors.New("private worker failure")}
	d.pollJobs()
	if d.active != 0 || len(d.jobs) != 0 || d.inspectionJob != nil || d.inspection != nil || len(*commands) != count {
		t.Fatal("completion leaked state or reopened a closed inspector")
	}
}

func TestInspectorViewCloseSurvivesResultRerender(t *testing.T) {
	for _, completionFirst := range []bool{false, true} {
		t.Run(strconv.FormatBool(completionFirst), func(t *testing.T) {
			d, commands := fixture(t)
			if err := d.startInspection("settings"); err != nil {
				t.Fatal(err)
			}
			work, token, frame := d.inspectionJob, d.inspection.token, d.inspection.frame
			// Model one running job and a close callback already sent by Form A.
			d.queue = nil
			d.jobs["inspection"].running = true
			if completionFirst {
				finishInspectionFixture(d, inspectionResult(work), nil)
				if d.inspection.frame == frame {
					t.Fatal("result did not replace rendered frame")
				}
			}
			if err := d.inspectionControl(token + " close"); err != nil || d.inspection != nil {
				t.Fatal("view close was invalidated by a result rerender", err)
			}
			count := len(*commands)
			if !completionFirst {
				finishInspectionFixture(d, inspectionResult(work), nil)
			}
			if len(*commands) != count || d.inspection != nil {
				t.Fatal("late completion reopened closed view")
			}
		})
	}
}

func TestInspectorEncodedTextAndFilterRoundTrip(t *testing.T) {
	d, commands := fixture(t)
	if err := d.startInspection("settings"); err != nil {
		t.Fatal(err)
	}
	filter := " 100% \"quoted\" \\ $value & café 👩\u200d💻 "
	if !inspectionFilter(filter) {
		t.Fatal("roundtrip fixture must use admitted filter bytes")
	}
	encoded := "x" + base64.RawURLEncoding.EncodeToString([]byte(filter))
	if err := inspectorCallback(d, "filter settings "+encoded); err != nil {
		t.Fatal(err)
	}
	input := ""
	for _, command := range *commands {
		if bytes, ok := strings.CutPrefix(command, "*SlateInspectorForm: InputHex inspect_filter 24 x"); ok {
			decoded, err := hex.DecodeString(bytes)
			if err != nil {
				t.Fatal(err)
			}
			input = string(decoded)
		}
	}
	if input != filter || d.inspection.filter != filter {
		t.Fatal("rendered input changed admitted literal bytes", input)
	}
	text := strings.Repeat(filter, 4)
	var joined strings.Builder
	for _, line := range inspectionText(nil, text) {
		if bytes, ok := strings.CutPrefix(line, "TextHex x"); ok {
			decoded, err := hex.DecodeString(bytes)
			if err != nil || !utf8.Valid(decoded) || utf8.RuneCount(decoded) > 72 {
				t.Fatal("invalid bounded encoded text", line, err)
			}
			joined.Write(decoded)
		}
	}
	if joined.String() != text {
		t.Fatal("encoded metadata text changed literal bytes")
	}
}

func TestInspectorFocusPolicyIsScopedLazyAndConfiguredOnce(t *testing.T) {
	d, commands := fixture(t)
	for _, command := range *commands {
		if command == "Style SlateInspectorForm FPGrabFocus, FPReleaseFocus" {
			t.Fatal("inspector focus policy was not opt-in")
		}
	}
	*commands = nil
	if err := d.startInspection("unknown"); err == nil || d.inspectionFocusConfigured || len(*commands) != 0 {
		t.Fatal("invalid request configured viewer focus policy")
	}
	if err := d.startInspection("settings"); err != nil {
		t.Fatal(err)
	}
	policy := "Style SlateInspectorForm FPGrabFocus, FPReleaseFocus"
	if len(*commands) == 0 || (*commands)[0] != policy || !d.inspectionFocusConfigured {
		t.Fatal("viewer policy must precede its first Form and worker publication")
	}
	finishInspectionFixture(d, inspectionResult(d.inspectionJob), nil)
	if err := inspectorCallback(d, "page 1"); err != nil {
		t.Fatal(err)
	}
	if err := inspectorCallback(d, "refresh"); err != nil {
		t.Fatal(err)
	}
	d.closeInspection()
	if err := d.startInspection("theme"); err != nil {
		t.Fatal(err)
	}
	finishInspectionFixture(d, inspectionResult(d.inspectionJob), nil)
	count := 0
	for _, command := range *commands {
		if strings.HasPrefix(command, "Style ") {
			if command != policy {
				t.Fatal("inspection configured another window's focus policy", command)
			}
			count++
		}
	}
	if count != 1 {
		t.Fatal("paging, refresh or replacement redefined focus policy", count)
	}
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
package desktopcontroller

import (
	"encoding/hex"
	"encoding/json"
	"fmt"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

func overviewCoreRow(kind, id string, index int) map[string]any {
	row := coreRow(kind, id, index)
	for key, value := range map[string]any{"desk": 2, "page_x": 0, "page_y": 1, "x": -64, "y": 12, "width": 640, "height": 480, "monitor_output": "18446744073709551615", "monitor_role": "0", "sticky_desks": false, "sticky_pages": false, "iconified": false, "fullscreen": false, "focused": true, "page_max_x": 2, "page_max_y": 1} {
		row[key] = value
	}
	return row
}
func overviewFixture(t *testing.T) (*Controller, *[]string) {
	t.Helper()
	d, commands := fixture(t)
	d.lastDesktop = &desktopsettings.DesktopMetrics{FormFont: "body", FormInputFont: "input", FormButtonFont: "button", FormDensity: "inherit"}
	if err := d.handle("overview", ""); err != nil {
		t.Fatal(err)
	}
	paletteCoreReply(t, d, corePage(0, 2, nil, []map[string]any{overviewCoreRow("window", "window-1", 0), overviewCoreRow("desktop", "desk-2", 1)}))
	paletteACK(t, d)
	return d, commands
}
func TestOverviewExclusiveProvidersFactsAndOwnedFormRoles(t *testing.T) {
	d, commands := overviewFixture(t)
	p := d.palette
	if !p.overview || len(p.model.Providers[2].Rows) != 0 || len(p.model.Providers[3].Rows) != 0 || p.model.Collection.VisibleCount() != 2 {
		t.Fatal("overview published other providers", p.model.Providers)
	}
	if got := p.model.Providers[0].Rows[0].Detail; !strings.Contains(got, "640x480-64+12") || !strings.Contains(got, "focused") {
		t.Fatal("structured frame facts lost", got)
	}
	text := strings.Join(*commands, "\n")
	for _, required := range []string{`Title "Window and workspace overview"`, `Font "body"`, `InputFont "input"`, `ButtonFont "button"`, `Mark window (F6)`, `Pointer Move (F7)`, `Move marked to workspace/page (F8)`} {
		if !strings.Contains(text, required) {
			t.Fatal("missing native overview control", required)
		}
	}
	oldCookie := p.cookie
	if err := d.startPalette(); err != nil || d.palette.overview || d.palette.cookie == oldCookie {
		t.Fatal("mode switch reused incompatible snapshot lifetime", err)
	}
	if d.paletteReply("MFL_CORE_1 " + oldCookie + " 0000000000000001 end 1") {
		t.Fatal("closed overview provider survived mode switch")
	}
}
func TestOverviewMarkMovePageBoundsAndStaleIdentity(t *testing.T) {
	d, commands := overviewFixture(t)
	p := d.palette
	window, desk := p.model.Providers[0].Rows[0], p.model.Providers[1].Rows[0]
	if err := d.overviewAction("mark", window, nil); err != nil {
		t.Fatal(err)
	}
	before := len(*commands)
	for _, page := range [][]string{{"-1", "0"}, {"3", "0"}, {"0", "2"}, {"0;Exec", "0"}, {"+0", "0"}} {
		if err := d.overviewAction("move", desk, page); err == nil {
			t.Fatal("invalid destination page accepted", page)
		}
	}
	if len(*commands) != before {
		t.Fatal("refused page emitted work")
	}
	if err := d.overviewAction("move", desk, []string{"2", "1"}); err != nil {
		t.Fatal(err)
	}
	if got := (*commands)[len(*commands)-1]; !strings.HasSuffix(got, " 0 move 1 2 1") {
		t.Fatal("source/destination not from one snapshot", got)
	}
	paletteCoreReply(t, d, map[string]any{"applied": true, "action": "move", "target": window.ID, "destination": desk.ID})
	if d.palette != nil {
		t.Fatal("acknowledged move did not retire view")
	}
	d, _ = overviewFixture(t)
	p = d.palette
	window = p.model.Providers[0].Rows[0]
	_ = d.overviewAction("mark", window, nil)
	d.paletteEvent(frame{kind: newPage})
	if err := d.overviewAction("drag", window, nil); err == nil {
		t.Fatal("topology invalidation left pointer action armed")
	}
	d.paletteTick()
	if p.marked != nil || p.pending == nil {
		t.Fatal("refresh retained stale marked allocation")
	}
}
func TestOverviewSignedSourcePagesDoNotRelaxDestinationBounds(t *testing.T) {
	d, commands := fixture(t)
	if err := d.startPaletteMode(true); err != nil {
		t.Fatal(err)
	}
	row := overviewCoreRow("window", "off-page-window", 0)
	row["page_x"], row["page_y"] = -1, -2
	paletteCoreReply(t, d, corePage(0, 2, nil, []map[string]any{
		row, overviewCoreRow("desktop", "desk-2", 1),
	}))
	paletteACK(t, d)
	p := d.palette
	if p.model.Collection.VisibleCount() != 2 || p.facts["off-page-window"].PageX != -1 || p.facts["off-page-window"].PageY != -2 {
		t.Fatal("signed observed source pages were discarded", p.facts)
	}
	window, desk := p.model.Providers[0].Rows[0], p.model.Providers[1].Rows[0]
	if !strings.Contains(window.Detail, "page-1,-2") {
		t.Fatal("signed source page facts were hidden", window.Detail)
	}
	if err := d.overviewAction("mark", window, nil); err != nil {
		t.Fatal(err)
	}
	before := len(*commands)
	if err := d.overviewAction("move", desk, []string{"-1", "0"}); err == nil || len(*commands) != before {
		t.Fatal("observed source pages relaxed destination validation", err)
	}
	if err := d.overviewAction("move", desk, []string{"0", "0"}); err != nil {
		t.Fatal("off-page source could not move to a valid destination", err)
	}
	if got := (*commands)[len(*commands)-1]; !strings.HasSuffix(got, " 0 move 1 0 0") {
		t.Fatal("source page entered destination command", got)
	}
}

func TestOverviewPointerSubmissionDoesNotClaimCompletion(t *testing.T) {
	d, commands := overviewFixture(t)
	window := d.palette.model.Providers[0].Rows[0]
	if err := d.overviewAction("drag", window, nil); err != nil {
		t.Fatal(err)
	}
	if got := (*commands)[len(*commands)-1]; !strings.HasSuffix(got, " 0 drag") {
		t.Fatal(got)
	}
	before := len(*commands)
	paletteCoreReply(t, d, map[string]any{"applied": false, "submitted": true, "action": "drag", "target": window.ID})
	var notice strings.Builder
	for _, command := range (*commands)[before:] {
		if data, ok := strings.CutPrefix(command, "*SlateNoticeForm: TextHex x"); ok {
			decoded, err := hex.DecodeString(data)
			if err != nil {
				t.Fatal("invalid native notice transport", err)
			}
			notice.Write(decoded)
		}
	}
	// The ordinary notice renderer splits a paragraph into72-rune rows.
	// Reassemble those rows before checking the full submitted-only message.
	if d.palette != nil || notice.String() != "Pointer Move was submitted/returned; inspect the window. Geometry completion is not confirmed." {
		t.Fatal("pointer submission reported completion")
	}
}
func TestOverviewFactsValidationAndLateReply(t *testing.T) {
	for _, mutation := range []func(map[string]any){
		func(r map[string]any) { delete(r, "desk") },
		func(r map[string]any) { r["monitor_output"] = 42 },
		func(r map[string]any) { r["monitor_role"] = "018" },
		func(r map[string]any) { r["fullscreen"] = "false" },
		func(r map[string]any) { r["focused"] = nil },
	} {
		d, _ := fixture(t)
		_ = d.startPaletteMode(true)
		row := overviewCoreRow("window", "win", 0)
		mutation(row)
		paletteCoreReply(t, d, corePage(0, 1, nil, []map[string]any{row}))
		if d.palette.model.Providers[0].State != "unavailable" {
			t.Fatal("invalid facts published")
		}
	}
	d, _ := fixture(t)
	_ = d.startPaletteMode(true)
	d.palette.pending.deadline = time.Now().Add(-time.Second)
	paletteCoreReply(t, d, corePage(0, 0, nil, []map[string]any{}))
	if d.palette.model.Providers[0].State != "unavailable" {
		t.Fatal("late reply revived stale view")
	}
}
func TestSharedCoreFragmentOrderingAndByteBound(t *testing.T) {
	data, parts := []byte{}, 0
	for _, fields := range [][]string{{"part", "1", "7b"}, {"part", "0", "zz"}, {"end", "0"}} {
		if _, err := coreFragment(fields, &data, &parts, 2); err == nil || parts != 0 || len(data) != 0 {
			t.Fatal("malformed fragment changed assembly")
		}
	}
	if done, err := coreFragment([]string{"part", "0", "7b7d"}, &data, &parts, 2); done || err != nil {
		t.Fatal(done, err)
	}
	if _, err := coreFragment([]string{"part", "1", "20"}, &data, &parts, 2); err == nil {
		t.Fatal("byte limit lost")
	}
	if done, err := coreFragment([]string{"end", "1"}, &data, &parts, 2); !done || err != nil || !json.Valid(data) {
		t.Fatal(done, err)
	}
}

func TestScratchpadManagerPagesStaleCallbacksAndPassiveUpdates(t *testing.T) {
	d, commands := fixture(t)
	d.lastDesktop = &desktopsettings.DesktopMetrics{FormFont: "body", FormInputFont: "input", FormButtonFont: "button", FormDensity: "inherit"}
	for i := 0; i < 16; i++ {
		if err := d.configureScratchpad(fmt.Sprintf("s%d Class - app", i)); err != nil {
			t.Fatal(err)
		}
	}
	if err := d.startScratchpads(); err != nil {
		t.Fatal(err)
	}
	old := d.scratchpads.frame
	text := strings.Join(*commands, "\n")
	if strings.Count(text, `Button continue "Toggle `) != 1 || !strings.Contains(text, `*SlateScratchpadsForm: Font "body"`) {
		t.Fatal("manager did not page bounded owned roles")
	}
	if err := d.scratchpadControl(old + " page 15"); err != nil || d.scratchpads.page != 15 {
		t.Fatal("last definition inaccessible", err)
	}
	before := len(*commands)
	if err := d.scratchpadControl(old + " toggle s0"); err != nil || len(*commands) != before {
		t.Fatal("retired manager callback acted", err)
	}
	d.scratchpads.definitions[15].state = "uncertain"
	d.scratchpads.viewDirty = true
	d.scratchpads.dirty = false
	d.scratchpadTick()
	updates := strings.Join((*commands)[before:], "\n")
	if !strings.Contains(updates, "SetLabel scratchpad_state") || strings.Contains(updates, "Module FvwmForm") || strings.Contains(updates, "FPGrabFocus") {
		t.Fatal("observation update stole focus", updates)
	}
}

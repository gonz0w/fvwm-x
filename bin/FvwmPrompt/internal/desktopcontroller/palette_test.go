// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package desktopcontroller

import (
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/actionpalette"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopworker"
	"github.com/fvwmorg/FvwmPrompt/internal/viewbinding"
)

func nativePalettePacket(generation uint32, count int) string {
	data := make([]byte, 6+(count+7)/8)
	binary.BigEndian.PutUint32(data, generation)
	return "x" + base64.RawURLEncoding.EncodeToString(data)
}
func paletteACK(t *testing.T, d *Controller) {
	t.Helper()
	for budget := 0; d.palette != nil && d.palette.native.wait != ""; budget++ {
		if budget > 100 {
			t.Fatal("unbounded native batches")
		}
		n := &d.palette.native
		phase := strings.Split(n.phase, ":")
		count := len(n.ids)
		if phase[0] == "commit" {
			count = len(n.nextIDs)
		}
		gen := n.generation
		if phase[0] == "commit" {
			gen = n.next
		}
		if !d.paletteNativeReply(fmt.Sprintf("FVWM_FORM_LIST %s %s %s 0 %s", n.wait, phase[1], phase[2], nativePalettePacket(gen, count))) {
			t.Fatal("native ACK ignored")
		}
	}
}
func paletteCoreReply(t *testing.T, d *Controller, result any) {
	t.Helper()
	q := d.palette.pending
	cookie := d.palette.cookie
	data, _ := json.Marshal(map[string]any{"result": result})
	parts := 0
	for start := 0; start < len(data); start += 256 {
		end := min(start+256, len(data))
		if !d.paletteReply(fmt.Sprintf("MFL_CORE_1 %s %s part %d %s", cookie, q.serial, parts, hex.EncodeToString(data[start:end]))) {
			t.Fatal("part ignored")
		}
		parts++
	}
	d.paletteReply(fmt.Sprintf("MFL_CORE_1 %s %s end %d", cookie, q.serial, parts))
}
func corePage(offset, total int, next *int, rows []map[string]any) map[string]any {
	return map[string]any{"state": "observed", "scope": "core-palette", "generation": strings.Repeat("a", 48), "offset": offset, "next_offset": next, "total": total, "truncated": false, "coverage": "managed windows; advertised desktops", "rows": rows}
}
func coreRow(kind, id string, index int) map[string]any {
	return map[string]any{"kind": kind, "id": id, "index": index, "label": id, "detail": "literal $%\\\"", "icon": "other", "enabled": true}
}
func TestPaletteProvidersCoalescingAndLateReply(t *testing.T) {
	d, commands := fixture(t)
	d.apps = []desktopworker.Application{{Name: "App", Argv: []string{"/bin/true"}}}
	d.appGeneration = "app-v1"
	d.appBinding.known = true
	if err := d.startPalette(); err != nil {
		t.Fatal(err)
	}
	p := d.palette
	oldCookie := p.cookie
	oldSerial := p.pending.serial
	for i := 0; i < 20; i++ {
		_ = d.refreshPalette()
	}
	if p.binding.Stats().Started != 1 || !p.binding.Pending() {
		t.Fatal(p.binding.Stats())
	}
	paletteCoreReply(t, d, corePage(0, 1, nil, []map[string]any{coreRow("window", "Old", 0)}))
	if p.binding.Stats().Started != 2 || p.model.Providers[0].State == "observed" {
		t.Fatal("superseded query published")
	}
	paletteCoreReply(t, d, corePage(0, 2, nil, []map[string]any{coreRow("window", "Window", 0), coreRow("desktop", "Desk", 1)}))
	paletteACK(t, d)
	if len(p.model.Providers[0].Rows) != 1 || len(p.model.Providers[1].Rows) != 1 || len(p.model.Providers[2].Rows) != 1 || len(p.model.Providers[3].Rows) != 8 {
		t.Fatal("provider absent", p.model.Providers)
	}
	before := len(*commands)
	d.paletteTick()
	if len(*commands) != before {
		t.Fatal("idle palette polled")
	}
	d.closePalette()
	if !strings.Contains(strings.Join((*commands)[before:], "\n"), "Send_Palette "+oldCookie) {
		t.Fatal("snapshot not released")
	}
	_ = d.startPalette()
	if d.paletteReply("MFL_CORE_1 " + oldCookie + " " + oldSerial + " end 1") {
		t.Fatal("old lifetime consumed")
	}
	if d.preview.pending != nil {
		t.Fatal("palette interfered with preview")
	}
}
func TestPaletteApplicationIdentityAndNativeBounds(t *testing.T) {
	d, commands := fixture(t)
	d.apps = []desktopworker.Application{{Name: strings.Repeat("é$", 200), Argv: []string{"old"}}}
	d.appGeneration = "apps"
	d.appBinding.known = true
	_ = d.startPalette()
	paletteACK(t, d)
	p := d.palette
	app := p.model.Providers[2].Rows[0]
	d.apps[0].Argv = []string{"new"}
	if d.paletteActivate(app) == nil {
		t.Fatal("changed full app identity acted")
	}
	for _, c := range *commands {
		if len(c) > 1000 {
			t.Fatalf("overlength command %d", len(c))
		}
	}
	d.appBinding.lost = true
	_ = d.paletteProviders()
	if _, err := p.model.Resolve(p.model.Collection.Generation, actionpalette.ID(actionpalette.Application, app.ID)); err == nil {
		t.Fatal("lost apps acted")
	}
	if err := d.paletteState(strings.Repeat("t", 64)); err != nil {
		t.Fatal(err)
	}
	p.native.deadline = time.Now().Add(-time.Second)
	p.native.wait = "pending"
	d.paletteTick()
	if d.palette != nil {
		t.Fatal("native timeout retained view")
	}
}
func TestPaletteMalformedCoreAndSelection(t *testing.T) {
	d, _ := fixture(t)
	_ = d.startPalette()
	p := d.palette
	d.paletteReply("MFL_CORE_1 " + p.cookie + " " + p.pending.serial + " part 1 00")
	if p.model.Providers[0].State != "unavailable" {
		t.Fatal("malformed core not lost")
	}
	paletteACK(t, d)
	if d.paletteControl(p.token+" activate x "+nativePalettePacket(p.native.generation+1, len(p.native.ids))) == nil {
		t.Fatal("stale native generation acted")
	}
	if err := d.paletteControl(p.token + " filter x" + base64.RawURLEncoding.EncodeToString([]byte("Settings")) + " " + nativePalettePacket(p.native.generation, len(p.native.ids))); err != nil {
		t.Fatal(err)
	}
	paletteACK(t, d)
	if p.model.Collection.Filter() != "Settings" || p.model.Collection.VisibleCount() != 1 {
		t.Fatal("filter failed")
	}
}

func TestPaletteActionFocusGuardAndReloadRetirement(t *testing.T) {
	d, commands := fixture(t)
	_ = d.startPalette()
	paletteCoreReply(t, d, corePage(0, 1, nil, []map[string]any{coreRow("window", "Live", 0)}))
	paletteACK(t, d)
	p := d.palette
	before := len(*commands)
	if err := d.paletteActivate(p.model.Providers[0].Rows[0]); err != nil {
		t.Fatal(err)
	}
	sent := strings.Join((*commands)[before:], "\n")
	guard := strings.Index(sent, "Style SlatePaletteForm !FPReleaseFocus")
	dispatch := strings.Index(sent, "Send_Palette ")
	if guard < 0 || dispatch < guard || !strings.Contains(sent, "UpdateStyles") {
		t.Fatal("action lacks ordered focus guard", sent)
	}
	paletteCoreReply(t, d, map[string]any{"applied": true, "action": "focus", "target": "Wrong"})
	if d.palette == nil || d.palette.model.Providers[0].State != "unavailable" {
		t.Fatal("wrong action ACK accepted")
	}
	if !strings.Contains(strings.Join((*commands)[before:], "\n"), "Style SlatePaletteForm FPReleaseFocus") {
		t.Fatal("refused action cannot cancel with prior focus")
	}
	oldCookie := p.cookie
	if err := d.reloadSettings(); err != nil {
		t.Fatal(err)
	}
	if d.palette != nil || !strings.Contains(strings.Join(*commands, "\n"), "Send_Palette "+oldCookie) {
		t.Fatal("reload retained palette")
	}
}
func TestPaletteDuplicateApplicationsRetainDistinctRows(t *testing.T) {
	d, _ := fixture(t)
	app := desktopworker.Application{Name: "Identical", Argv: []string{"/bin/true"}}
	d.apps = []desktopworker.Application{app, app}
	d.appGeneration = "apps"
	d.appBinding.known = true
	if err := d.startPalette(); err != nil {
		t.Fatal(err)
	}
	rows := d.palette.model.Providers[2].Rows
	if len(rows) != 2 || rows[0].ID == rows[1].ID {
		t.Fatal("duplicate app metadata lost distinct entries")
	}
}
func TestPalettePresentationRetainsUnicodeAndEscapesExpansion(t *testing.T) {
	text := paletteDisplay("Résumé 配置 — $%\\\"&", 160)
	if !strings.Contains(text, "Résumé 配置 — ") || strings.ContainsAny(text, "$%\\\"&") || !strings.Contains(text, "[U+0024]") {
		t.Fatal(text)
	}
	d, _ := fixture(t)
	d.apps = []desktopworker.Application{{Name: "Résumé", Argv: []string{"/bin/true"}}}
	d.appGeneration = "apps"
	d.appBinding.known = true
	_ = d.startPalette()
	if err := d.palette.model.Filter("RÉSUMÉ"); err != nil || d.palette.model.Collection.VisibleCount() != 1 {
		t.Fatal("Unicode literal filter", err)
	}
}

func TestPaletteInvalidRefreshCannotPoisonRetainedProviders(t *testing.T) {
	d, _ := fixture(t)
	_ = d.startPalette()
	paletteCoreReply(t, d, corePage(0, 1, nil, []map[string]any{coreRow("window", "Retained", 0)}))
	paletteACK(t, d)
	_ = d.refreshPalette()
	before := d.palette.binding.Stats()
	bad := coreRow("window", "Invalid", 0)
	bad["label"] = ""
	paletteCoreReply(t, d, corePage(0, 1, nil, []map[string]any{bad}))
	paletteACK(t, d)
	p := d.palette
	after := p.binding.Stats()
	if after.State != viewbinding.Failed || after.InFlight || after.Published != before.Published || after.Rejected != before.Rejected+1 {
		t.Fatal("invalid candidate recorded a successful publication", before, after)
	}
	if p.model.Providers[0].State != "unavailable" || len(p.model.Providers[0].Rows) != 1 || p.model.Providers[0].Rows[0].ID != "Retained" {
		t.Fatal("invalid candidate poisoned retained snapshot")
	}
	if _, err := p.model.Resolve(p.model.Collection.Generation, actionpalette.ID(actionpalette.Window, "Retained")); err == nil {
		t.Fatal("retained failed-provider row acted")
	}
}

func TestPaletteEventInvalidationIsOpenOnlyCoalescedAndIgnoresOwnForm(t *testing.T) {
	d, commands := fixture(t)
	packet := frame{kind: paletteConfigure, body: make([]byte, 24)}
	binary.NativeEndian.PutUint64(packet.body, 101)
	before := len(*commands)
	d.paletteEvent(packet)
	if len(*commands) != before {
		t.Fatal("closed palette observes events")
	}
	_ = d.startPalette()
	paletteCoreReply(t, d, corePage(0, 0, nil, []map[string]any{}))
	paletteACK(t, d)
	p := d.palette
	started := p.binding.Stats().Started
	deadline := p.deadline
	for i := 0; i < 100; i++ {
		d.paletteEvent(packet)
	}
	if p.binding.Stats().Started != started || !p.eventDirty {
		t.Fatal("packet handler started work")
	}
	d.paletteTick()
	if p.binding.Stats().Started != started+1 || p.eventDirty || p.deadline != deadline {
		t.Fatal("event batch did not coalesce or renewed user lifetime")
	}
	own := frame{kind: paletteResName, body: make([]byte, 24)}
	binary.NativeEndian.PutUint64(own.body, 202)
	own.body = append(own.body, []byte("SlatePaletteForm\x00")...)
	d.paletteEvent(own)
	binary.NativeEndian.PutUint64(packet.body, 202)
	d.paletteEvent(packet)
	if p.eventDirty {
		t.Fatal("own Form configured a refresh loop")
	}
	d.closePalette()
	if got := (*commands)[len(*commands)-1]; got != fmt.Sprintf("SET_MASK %d", regularDesktopMask) {
		t.Fatal("regular window event subscription survived close", got)
	}
}

func TestPaletteEventStormDoesNotRepublishLoadingLabels(t *testing.T) {
	d, commands := fixture(t)
	_ = d.startPalette()
	paletteACK(t, d)
	p := d.palette
	// The first query remains outstanding. Repeated event batches can only
	// retain a latest pending refresh, never refill either native queue.
	packet := frame{kind: paletteConfigure, body: make([]byte, 24)}
	binary.NativeEndian.PutUint64(packet.body, 101)
	d.paletteEvent(packet)
	d.paletteTick()
	paletteACK(t, d)
	before := len(*commands)
	for i := 0; i < 100; i++ {
		d.paletteEvent(packet)
		d.paletteTick()
	}
	if len(*commands) != before || p.binding.Stats().Started != 1 || !p.binding.Pending() {
		t.Fatal("event storm republished unchanged loading state")
	}
}

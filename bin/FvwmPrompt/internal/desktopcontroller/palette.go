// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"
	"unicode"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/actionpalette"
	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/viewbinding"
)

type paletteQuery struct {
	serial, phase  string
	target, action string
	parts          int
	data           []byte
	deadline       time.Time
}
type paletteView struct {
	eventDirty     bool
	window         uint64
	token, cookie  string
	model          actionpalette.Model
	binding        *viewbinding.Binding
	ticket         viewbinding.Ticket
	serial         uint64
	pending        *paletteQuery
	core           []actionpalette.Row
	coreGeneration string
	total          int
	truncated      bool
	native         paletteNative
	deadline       time.Time
}
type palettePage struct {
	State      string `json:"state"`
	Scope      string `json:"scope"`
	Generation string `json:"generation"`
	Offset     int    `json:"offset"`
	Next       *int   `json:"next_offset"`
	Total      int    `json:"total"`
	Truncated  bool   `json:"truncated"`
	Coverage   string `json:"coverage"`
	Rows       []struct {
		ID      string             `json:"id"`
		Index   int                `json:"index"`
		Kind    actionpalette.Kind `json:"kind"`
		Label   string             `json:"label"`
		Detail  string             `json:"detail"`
		Icon    string             `json:"icon"`
		Enabled bool               `json:"enabled"`
	} `json:"rows"`
}

func (d *Controller) startPalette() error {
	if d.closed || !d.ready {
		return errors.New("Desktop is not ready")
	}
	if d.palette != nil {
		return d.refreshPalette()
	}
	b, _ := viewbinding.New("core")
	p := &paletteView{token: newID(16), cookie: newID(16), binding: b, deadline: time.Now().Add(5 * time.Minute)}
	d.palette = p
	d.emit(fmt.Sprintf("SET_MASK %d", coreReply|configSourceMessage|nativeEcho))
	d.emit(fmt.Sprintf("SET_MASK %d", regularDesktopMask|paletteEventMask))
	d.emit("Style SlatePaletteForm FPGrabFocus, FPReleaseFocus")
	for i := range p.model.Providers {
		p.model.Providers[i] = actionpalette.Provider{Version: "pending", State: "loading", Coverage: "Pending initial observation"}
	}
	if err := d.paletteProviders(); err != nil {
		return err
	}
	d.paletteForm()
	return d.refreshPalette()
}
func (d *Controller) closePalette() {
	if d.palette == nil {
		return
	}
	p := d.palette
	p.binding.Close()
	d.palette = nil
	d.emit(fmt.Sprintf("Send_Palette %s %016x - -", p.cookie, p.serial+1))
	d.emit("KillModule FvwmForm SlatePaletteForm")
	d.emit(fmt.Sprintf("SET_MASK %d", coreReply|configSourceMessage))
	d.emit(fmt.Sprintf("SET_MASK %d", regularDesktopMask))
}
func (d *Controller) paletteCapabilities() map[string]bool {
	return map[string]bool{"desktop": !d.closed && d.ready, "worker": !d.closed && d.worker != "", "terminal": !d.closed && len(d.terminal) > 0}
}
func paletteLabel(s string) string {
	s = strings.ToValidUTF8(s, "�")
	s = strings.Map(func(r rune) rune {
		if unicode.IsControl(r) {
			return '�'
		}
		return r
	}, s)
	for len(s) > 160 {
		_, n := utf8.DecodeLastRuneInString(s)
		s = s[:len(s)-n]
	}
	if s == "" {
		return "Unnamed application"
	}
	return s
}
func (d *Controller) paletteProviders() error {
	p := d.palette
	if p == nil {
		return nil
	}
	providers := p.model.Providers
	state, reason := "observed", ""
	if d.appBinding == nil || !d.appBinding.known || d.appBinding.lost {
		state = "unavailable"
		reason = "Application discovery unavailable; Refresh to retry"
	}
	version := d.appGeneration
	if version == "" {
		version = "pending"
	}
	apps := actionpalette.Provider{Version: version, State: state, Reason: reason, Coverage: "Existing committed application discovery; first 500 entries", Truncated: len(d.apps) > actionpalette.MaxApps}
	seen := map[string]int{}
	for i, a := range d.apps[:min(len(d.apps), actionpalette.MaxApps)] {
		data, _ := json.Marshal(a)
		digest := hash(data)
		id := digest + "-" + strconv.Itoa(seen[digest])
		seen[digest]++
		apps.Rows = append(apps.Rows, actionpalette.Row{ID: id, Kind: actionpalette.Application, Label: paletteLabel(a.Name), Icon: "file", Version: version, Index: i, Enabled: state == "observed"})
	}
	providers[2] = apps
	actions := actionpalette.Provider{Version: d.generation, State: "observed", Coverage: "Eight registered desktop actions; capabilities rechecked at activation"}
	for _, a := range actionpalette.Registry() {
		actions.Rows = append(actions.Rows, actionpalette.Row{ID: string(a.Intent), Kind: actionpalette.Registered, Label: a.Label, Icon: "other", Version: d.generation, Intent: a.Intent, Enabled: actionpalette.Allowed(a.Intent, d.paletteCapabilities())})
	}
	providers[3] = actions
	if err := p.model.Replace(providers); err != nil {
		return err
	}
	d.palettePublish()
	return d.fatal
}
func (d *Controller) refreshPalette() error {
	p := d.palette
	if p == nil {
		return nil
	}
	p.deadline = time.Now().Add(5 * time.Minute)
	return d.invalidatePalette()
}
func (d *Controller) invalidatePalette() error {
	p := d.palette
	if p == nil {
		return nil
	}
	if err := p.binding.Observe("core", viewbinding.Version{Producer: d.instance, Revision: "core"}); err != nil {
		return err
	}
	if err := p.binding.Invalidate(); err != nil {
		return err
	}
	// Old snapshots remain visible but cannot act once a refresh supersedes them.
	for i := 0; i < 2; i++ {
		p.model.Providers[i].State = "loading"
		p.model.Providers[i].Reason = "Refreshing core metadata"
	}
	_ = d.paletteProviders()
	d.paletteBegin()
	return d.fatal
}
func (d *Controller) paletteBegin() {
	p := d.palette
	if p == nil || p.pending != nil {
		return
	}
	ticket, ok := p.binding.Begin()
	if !ok {
		return
	}
	p.ticket = ticket
	p.core = nil
	p.coreGeneration = ""
	p.total = 0
	p.truncated = false
	d.paletteSubmit("query", 0)
}
func (d *Controller) paletteSubmit(phase string, offset int) {
	p := d.palette
	if p == nil {
		return
	}
	if p.serial == ^uint64(0) {
		d.paletteFailed("Core query identity exhausted")
		return
	}
	p.serial++
	serial := fmt.Sprintf("%016x", p.serial)
	p.pending = &paletteQuery{serial: serial, phase: phase, deadline: time.Now().Add(5 * time.Second)}
	gen := p.coreGeneration
	if gen == "" {
		gen = "-"
	}
	d.emit(fmt.Sprintf("Send_Query %s %s palette %d 16 %s", p.cookie, serial, offset, gen))
}
func (d *Controller) paletteFailed(reason string) {
	p := d.palette
	if p == nil {
		return
	}
	p.pending = nil
	d.emit("Style SlatePaletteForm FPReleaseFocus")
	d.emit("UpdateStyles")
	p.binding.Complete(p.ticket, false)
	for i := 0; i < 2; i++ {
		p.model.Providers[i].State = "unavailable"
		p.model.Providers[i].Reason = reason
	}
	_ = d.paletteProviders()
	d.paletteBegin()
}
func (d *Controller) paletteReply(message string) bool {
	p := d.palette
	if p == nil || p.pending == nil {
		return false
	}
	q := p.pending
	prefix := "MFL_CORE_1 " + p.cookie + " " + q.serial + " "
	if !strings.HasPrefix(message, prefix) {
		return false
	}
	f := strings.Fields(strings.TrimPrefix(message, prefix))
	if len(f) == 3 && f[0] == "part" && f[1] == strconv.Itoa(q.parts) && len(f[2]) >= 2 && len(f[2]) <= 512 && len(f[2])%2 == 0 {
		data, err := hex.DecodeString(f[2])
		if err == nil && len(q.data)+len(data) <= 16384 {
			q.data = append(q.data, data...)
			q.parts++
			return true
		}
	}
	if len(f) != 2 || f[0] != "end" || f[1] != strconv.Itoa(q.parts) || q.parts == 0 {
		d.paletteFailed("Malformed core reply; Refresh to retry")
		return true
	}
	p.pending = nil
	decoded, err := desktopsettings.DecodeJSON(q.data)
	response, ok := decoded.(map[string]any)
	if err != nil || !ok || !exact(response, "result") {
		d.paletteFailed("Core provider refused the request; Refresh to retry")
		return true
	}
	data, _ := json.Marshal(response["result"])
	if q.phase == "activate" {
		var r struct {
			Applied bool   `json:"applied"`
			Action  string `json:"action"`
			Target  string `json:"target"`
		}
		if json.Unmarshal(data, &r) != nil || !r.Applied || r.Target != q.target || r.Action != q.action {
			d.paletteFailed("Core action was refused; Refresh to retry")
		} else {
			d.closePalette()
		}
		return true
	}
	object, valid := response["result"].(map[string]any)
	if !valid || !exact(object, "state", "scope", "generation", "offset", "next_offset", "total", "truncated", "coverage", "rows") {
		d.paletteFailed("Invalid core metadata; Refresh to retry")
		return true
	}
	rawRows, valid := object["rows"].([]any)
	if !valid {
		d.paletteFailed("Invalid core rows; Refresh to retry")
		return true
	}
	for _, raw := range rawRows {
		r, ok := raw.(map[string]any)
		if !ok || !exact(r, "id", "index", "kind", "label", "detail", "icon", "enabled") {
			d.paletteFailed("Invalid core row; Refresh to retry")
			return true
		}
	}
	var page palettePage
	if json.Unmarshal(data, &page) != nil || page.State != "observed" || page.Scope != "core-palette" || !identity(page.Generation, 48) || page.Offset != len(p.core) || page.Total < 0 || page.Total > 640 || len(page.Rows) > 16 || !actionpalette.Text(page.Coverage, 256) {
		d.paletteFailed("Invalid core metadata; Refresh to retry")
		return true
	}
	if p.coreGeneration != "" && (p.coreGeneration != page.Generation || p.total != page.Total || p.truncated != page.Truncated) {
		d.paletteFailed("Core snapshot changed; Refresh to retry")
		return true
	}
	p.coreGeneration = page.Generation
	p.total = page.Total
	p.truncated = page.Truncated
	for _, r := range page.Rows {
		if r.Index != len(p.core) || len(p.core) >= 640 || (r.Kind != actionpalette.Window && r.Kind != actionpalette.Desktop) {
			d.paletteFailed("Invalid core row identity; Refresh to retry")
			return true
		}
		p.core = append(p.core, actionpalette.Row{ID: r.ID, Kind: r.Kind, Label: r.Label, Detail: r.Detail, Icon: r.Icon, Version: page.Generation, Index: r.Index, Enabled: r.Enabled})
	}
	if page.Next != nil {
		if *page.Next != len(p.core) || *page.Next >= page.Total || len(page.Rows) == 0 {
			d.paletteFailed("Invalid core continuation; Refresh to retry")
		} else {
			d.paletteSubmit("query", *page.Next)
		}
		return true
	}
	if len(p.core) != page.Total {
		d.paletteFailed("Incomplete core inventory; Refresh to retry")
		return true
	}
	providers := p.model.Providers
	for i := 0; i < 2; i++ {
		providers[i] = actionpalette.Provider{Version: page.Generation, State: "observed", Coverage: page.Coverage, Truncated: page.Truncated}
	}
	for _, r := range p.core {
		i := 0
		if r.Kind == actionpalette.Desktop {
			i = 1
		}
		providers[i].Rows = append(providers[i].Rows, r)
	}
	// Replace validates the whole candidate before the binding records success.
	// Detach public mutable collection storage; reconciliation reads the old
	// private projection and publishes its own slices/maps without editing it.
	candidate := p.model
	candidate.Collection.Entries = append([]collection.Entry(nil), p.model.Collection.Entries...)
	candidate.Collection.Selected = make(map[string]bool, len(p.model.Collection.Selected))
	for id, selected := range p.model.Collection.Selected {
		candidate.Collection.Selected[id] = selected
	}
	if err := candidate.Replace(providers); err != nil {
		d.paletteFailed("Core row validation failed; Refresh to retry")
		return true
	}
	if p.binding.Complete(p.ticket, true) {
		p.model = candidate
		d.palettePublish()
	}

	d.paletteBegin()
	return true
}
func (d *Controller) paletteControl(argument string) error {
	words := strings.Fields(argument)
	p := d.palette
	if len(words) < 2 {
		return errors.New("Invalid palette callback")
	}
	if p == nil || words[0] != p.token {
		return nil
	}
	if words[1] == "close" && len(words) == 2 {
		d.closePalette()
		return d.fatal
	}
	if len(words) != 4 {
		return errors.New("Invalid palette callback")
	}
	query, err := base64.RawURLEncoding.Strict().DecodeString(strings.TrimPrefix(words[2], "x"))
	if err != nil || !strings.HasPrefix(words[2], "x") || !actionpalette.Text(string(query), 128) {
		return errors.New("Palette query must contain at most 128 UTF-8 bytes")
	}
	if p.native.wait != "" || p.native.dirty {
		return errors.New("Palette is updating; try again")
	}
	packet, err := palettePacket(words[3], p.native.generation)
	if err != nil {
		return err
	}
	if err = p.model.Collection.SelectNativeView(p.model.Collection.Generation, p.native.ids, packet); err != nil {
		return err
	}
	if words[1] == "activate" {
		r, err := p.model.Resolve(p.model.Collection.Generation, p.model.Collection.Focus)
		if err != nil {
			return err
		}
		return d.paletteActivate(r)
	}
	if err = p.model.Filter(string(query)); err != nil {
		return err
	}
	if words[1] == "refresh" {
		d.requestMenuJob("apps")
		return d.refreshPalette()
	}
	if words[1] != "filter" {
		return errors.New("Unknown palette callback")
	}
	d.palettePublish()
	return d.fatal
}
func (d *Controller) paletteDisarmFocus() {
	d.emit("Style SlatePaletteForm !FPReleaseFocus")
	d.emit("UpdateStyles")
}
func (d *Controller) paletteActivate(r actionpalette.Row) error {
	p := d.palette
	if p == nil {
		return nil
	}
	switch r.Kind {
	case actionpalette.Window, actionpalette.Desktop:
		if p.eventDirty || p.pending != nil || p.binding.State() != viewbinding.Current || r.Version != p.coreGeneration {
			return errors.New("Core selection is stale; Refresh first")
		}
		p.serial++
		serial := fmt.Sprintf("%016x", p.serial)
		action := "focus"
		if r.Kind == actionpalette.Desktop {
			action = "desktop"
		}
		p.pending = &paletteQuery{serial: serial, phase: "activate", target: r.ID, action: action, deadline: time.Now().Add(5 * time.Second)}
		// Disable this window's focus-return before the shared engine focuses the
		// target. The later Form destruction cannot restore the prior client.
		d.paletteDisarmFocus()
		d.emit(fmt.Sprintf("Send_Palette %s %s %s %d", p.cookie, serial, r.Version, r.Index))
		return d.fatal
	case actionpalette.Application:
		if err := d.requireApplications(); err != nil {
			return err
		}
		if r.Version != d.appGeneration || r.Index < 0 || r.Index >= len(d.apps) {
			return errors.New("Application selection is stale")
		}
		data, _ := json.Marshal(d.apps[r.Index])
		digest := hash(data)
		occurrence := 0
		for _, app := range d.apps[:r.Index] {
			previous, _ := json.Marshal(app)
			if hash(previous) == digest {
				occurrence++
			}
		}
		if digest+"-"+strconv.Itoa(occurrence) != r.ID {
			return errors.New("Application identity changed")
		}
		d.paletteDisarmFocus()
		d.closePalette()
		return d.launchApplication(strconv.Itoa(r.Index))
	case actionpalette.Registered:
		if r.Version != d.generation || !actionpalette.Allowed(r.Intent, d.paletteCapabilities()) {
			return errors.New("Action capability changed; Refresh first")
		}
		d.paletteDisarmFocus()
		d.closePalette()
		switch r.Intent {
		case actionpalette.Settings:
			return d.handle("settings", "")
		case actionpalette.Appearance:
			d.emit("Menu SlateAppearance Monitor c c")
		case actionpalette.Inspector:
			return d.startInspection("all")
		case actionpalette.CheckConfig:
			return d.startCheck()
		case actionpalette.RefreshApps:
			d.requestMenuJob("apps")
		case actionpalette.Terminal:
			d.term(nil, "")
		case actionpalette.Files:
			home, _ := os.UserHomeDir()
			d.openDirectory(home)
		case actionpalette.Run:
			d.emit("Module FvwmForm SlateRunForm")
		}
	}
	return d.fatal
}
func (d *Controller) paletteTick() {
	p := d.palette
	if p == nil {
		return
	}
	now := time.Now()
	if p.eventDirty {
		p.eventDirty = false
		_ = d.invalidatePalette()
	}
	if !now.Before(p.deadline) {
		d.closePalette()
		return
	}
	if p.pending != nil && !now.Before(p.pending.deadline) {
		d.paletteFailed("Core reply timed out; Refresh to retry")
	}
	if p.native.wait != "" && !now.Before(p.native.deadline) {
		d.closePalette()
		d.notice("Palette native view timed out; reopen it to retry")
	}
}
func (d *Controller) paletteState(token string) error {
	if !identifier(token) {
		return errors.New("Invalid palette diagnostic token")
	}
	p := d.palette
	state := map[string]any{"open": p != nil}
	if p != nil {
		counts := [4]int{}
		states := [4]string{}
		for i, v := range p.model.Providers {
			counts[i] = len(v.Rows)
			states[i] = v.State
		}
		state["counts"] = counts
		state["states"] = states
		state["binding"] = p.binding.Stats()
		state["native_ready"] = p.native.wait == "" && !p.native.dirty
		state["visible"] = p.model.Collection.VisibleCount()
	}
	data, _ := json.Marshal(state)
	commands := []string{"Echo FVWM_PALETTE_STATE " + token + " " + hex.EncodeToString(data)}
	if p != nil {
		view := map[string]any{"query": p.model.Collection.Filter(), "token": p.token, "generation": p.native.generation, "total": len(p.model.Collection.Entries)}
		data, _ = json.Marshal(view)
		commands = append(commands, "Echo FVWM_PALETTE_VIEW "+token+" "+hex.EncodeToString(data))
	}
	for _, command := range commands {
		if len(command) > 1000 {
			return errors.New("Palette diagnostic limit")
		}
	}
	for _, command := range commands {
		d.emit(command)
	}
	return d.fatal
}

// Packets only invalidate metadata. The existing tick admits at most one
// refresh after a packet batch; the binding retains one latest pending intent.
func (d *Controller) paletteEvent(packet frame) {
	p := d.palette
	if p == nil {
		return
	}
	if packet.kind == newPage || packet.kind == newDesk {
		p.eventDirty = true
		return
	}
	if len(packet.body) < 24 {
		return
	}
	window := binary.NativeEndian.Uint64(packet.body)
	if packet.kind == paletteResName {
		name, _, _ := strings.Cut(string(packet.body[24:]), "\x00")
		if name == "SlatePaletteForm" {
			p.window = window
			return
		}
		return
	}
	if window != 0 && window == p.window {
		return
	}
	p.eventDirty = true
}

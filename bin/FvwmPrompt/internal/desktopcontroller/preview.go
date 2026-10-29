// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/hex"
	"errors"
	"fmt"
	"math"
	"strconv"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

type previewSnapshot struct {
	generation string
	revision   *string
}
type previewControl struct {
	view, generation, lease string
	opacity, retries        int
	saved                   *previewSnapshot
	editor                  *draft
	resource                *resourceCandidate
}
type previewPending struct {
	phase, serial string
	data          []byte
	parts         int
	deadline      time.Time
}
type preview struct {
	desktop  *Controller
	serial   uint64
	control  *previewControl
	pending  *previewPending
	queued   string
	nextPoll time.Time
}

func previewPresentation(settings values) error {
	presentation, err := desktopsettings.ResolvePresentation(settings)
	if err != nil {
		return err
	}
	if presentation.Contrast == "high" {
		return errors.New("High contrast requires opaque menus; disable high contrast before previewing launcher opacity")
	}
	return nil
}

// The event loop may receive a settings transition between any two core
// replies. Recheck both current policy and a transferred editor candidate.
func (p *preview) allowed(e *draft) error {
	if err := previewPresentation(p.desktop.effective); err != nil {
		return err
	}
	if e != nil {
		_, effective, err := p.desktop.prepare(e.values)
		if err != nil {
			return err
		}
		return previewPresentation(effective)
	}
	return nil
}

// Run only after a validated settings transition has committed. Abandon uses
// the existing core recovery path and must precede opaque theme publication.
func (p *preview) constrain() {
	if p.control != nil && p.control.resource != nil {
		p.finish("Settings changed; resource preview recovery requested.", true, false)
		return
	}
	presentation, err := desktopsettings.ResolvePresentation(p.desktop.effective)
	if err == nil && p.control != nil && presentation.Contrast == "high" {
		p.finish("High contrast enabled; launcher opacity preview ended.", true, false)
	}
}

func (d *Controller) preparePreview(opacity int) (*previewSnapshot, error) {
	if err := previewPresentation(d.effective); err != nil {
		return nil, err
	}
	if d.persistenceError != "" {
		return nil, errors.New(d.persistenceError)
	}
	if _, ok := d.overrides["launcher_opacity"]; ok {
		return nil, errors.New("Launcher opacity has a module configuration override")
	}
	if d.effective["transparency"] != true {
		return nil, errors.New("Enable transparency before previewing launcher opacity")
	}
	if _, err := desktopsettings.Value("launcher_opacity", opacity, false); err != nil {
		return nil, err
	}
	return &previewSnapshot{d.generation, d.revision}, nil
}
func (d *Controller) checkPreview(control *previewControl) error {
	if control != nil && control.resource != nil {
		return d.checkResource(control)
	}
	if control == nil || control.saved == nil {
		return errors.New("This preview has no saved-preference candidate")
	}
	if _, err := d.preparePreview(control.opacity); err != nil {
		return err
	}
	if control.saved.generation != d.generation {
		return errors.New("Settings changed; reopen the preview before saving")
	}
	_, revision, err := loadSettings(d.statePath)
	if err != nil {
		return err
	}
	if !sameRevision(revision, control.saved.revision) {
		return errors.New("Settings changed externally; reload before saving")
	}
	return nil
}
func (d *Controller) savePreview(control *previewControl) error {
	if err := d.checkPreview(control); err != nil {
		return err
	}
	changes := values{"launcher_opacity": control.opacity}
	candidate, err := desktopsettings.Checked(merged(d.saved, changes))
	if err != nil {
		return err
	}
	effective, err := desktopsettings.Checked(merged(d.effective, changes))
	if err != nil {
		return err
	}
	revision, err := saveSettings(d.statePath, candidate, control.saved.revision)
	if err != nil {
		return err
	}
	d.saved, d.effective, d.revision = candidate, effective, revision
	// Only opacity was core-confirmed: do not replay a full theme after commit.
	if d.lastTheme != nil {
		d.lastTheme[4] = effective["launcher_opacity"]
	}
	return nil
}
func (p *preview) submit(phase, command string) error {
	if p.pending != nil || p.serial == math.MaxUint64 {
		return errors.New("Preview reply channel is unavailable")
	}
	p.serial++
	serial := fmt.Sprintf("%016x", p.serial)
	p.pending = &previewPending{phase: phase, serial: serial, deadline: time.Now().Add(5 * time.Second)}
	p.desktop.emit(strings.NewReplacer("{cookie}", p.desktop.instance, "{serial}", serial).Replace(command))
	return p.desktop.fatal
}
func (p *preview) request(op string) error {
	item := p.control
	if item == nil {
		return errors.New("Preview controls are stale")
	}
	if op == "confirm" || op == "save" {
		if err := p.allowedControl(); err != nil {
			return err
		}
	}
	if op == "save" {
		if err := p.desktop.checkPreview(item); err != nil {
			return err
		}
	}
	coreOp := op
	if op == "save" {
		coreOp = "confirm"
	}
	generation, lease := "-", "-"
	if coreOp == "confirm" || coreOp == "revert" {
		generation, lease = item.generation, item.lease
	}
	err := p.submit(op, "Send_Preview {cookie} {serial} "+item.view+" "+coreOp+" "+generation+" "+lease+" 0 0 -")
	if err == nil && op == "save" && item.resource != nil {
		item.resource.confirming = true
	}
	return err
}
func (p *preview) start(opacity int, saved bool, e *draft) error {
	if p.control != nil || !p.desktop.ready {
		return errors.New("A preview is already pending or desktop is not ready")
	}
	if opacity < 0 || opacity > 100 {
		return errors.New("Invalid menu opacity")
	}
	if err := p.allowed(e); err != nil {
		return err
	}
	var snapshot *previewSnapshot
	var err error
	if saved {
		snapshot, err = p.desktop.preparePreview(opacity)
		if err != nil {
			return err
		}
	}
	p.control = &previewControl{view: newID(8), opacity: opacity, retries: 1, saved: snapshot, editor: e}
	return p.submit("inspect", "Send_Query {cookie} {serial} appearance 0 8 -")
}
func (p *preview) action(view, op string) error {
	if p.control == nil || p.control.view != view || p.control.lease == "" || (op != "confirm" && op != "revert" && op != "save") {
		return errors.New("Preview controls are stale")
	}
	if p.control.resource != nil && (op == "confirm" || op == "save" && p.control.resource.state != "active") {
		return errors.New("Resource preview is not ready to save")
	}
	if op != "revert" {
		if err := p.allowedControl(); err != nil {
			return err
		}
	}
	if op == "save" {
		if err := p.desktop.checkPreview(p.control); err != nil {
			return err
		}
	}
	if p.pending != nil {
		if p.queued != "revert" {
			p.queued = op
		}
		return nil
	}
	return p.request(op)
}
func (p *preview) finish(message string, abandon, committed bool) {
	var e *draft
	if p.control != nil {
		e = p.control.editor
	}
	resume := e != nil && (committed || e.generation == p.desktop.generation || p.control.resource != nil)
	if abandon && p.control != nil {
		p.pending = nil
		_ = p.request("abandon")
	}
	p.desktop.emit("KillModule FvwmForm SlatePreview-" + p.desktop.instance)
	p.pending, p.control, p.queued = nil, nil, ""
	p.desktop.notice(message)
	if resume {
		if err := p.desktop.editSettings(e.scope, false, e.values); err != nil {
			p.desktop.notice(err.Error())
		}
	}
}
func (p *preview) show() error {
	lines, err := desktopsettings.Preview(p.desktop.instance, p.control.view, p.control.opacity, p.control.saved != nil)
	if p.control.resource != nil {
		lines, err = desktopsettings.ResourcePreview(p.desktop.instance, p.control.view, p.control.resource.key, p.control.resource.value)
	}
	if err != nil {
		return err
	}
	p.desktop.form("SlatePreview-"+p.desktop.instance, lines, false)
	return p.desktop.fatal
}
func (p *preview) reply(message string) {
	pending := p.pending
	if pending == nil {
		return
	}
	prefix := "MFL_CORE_1 " + p.desktop.instance + " " + pending.serial + " "
	if !strings.HasPrefix(message, prefix) {
		return
	}
	if !time.Now().Before(pending.deadline) {
		p.finish("Preview reply timed out; core recovery remains responsible.", true, false)
		return
	}
	if err := p.acceptReply(strings.Fields(message[len(prefix):])); err != nil {
		p.finish("Preview result unavailable; core recovery remains responsible.", true, false)
	}
}
func (p *preview) acceptReply(fields []string) error {
	pending := p.pending
	if len(fields) == 3 && fields[0] == "part" && fields[1] == strconv.Itoa(pending.parts) && len(fields[2]) >= 2 && len(fields[2]) <= 512 && len(fields[2])%2 == 0 && identity(fields[2], len(fields[2])) {
		if len(pending.data)+len(fields[2])/2 > 8192 {
			return errors.New("Preview reply exceeded its limit")
		}
		data, _ := hex.DecodeString(fields[2])
		pending.data = append(pending.data, data...)
		pending.parts++
		return nil
	}
	if len(fields) != 2 || fields[0] != "end" || fields[1] != strconv.Itoa(pending.parts) || pending.parts == 0 {
		return errors.New("Invalid preview reply")
	}
	decoded, err := desktopsettings.DecodeJSON(pending.data)
	if err != nil {
		return err
	}
	response, ok := decoded.(map[string]any)
	if !ok {
		return errors.New("Invalid preview response")
	}
	p.pending = nil
	if p.control.resource != nil {
		return p.acceptResourceReply(pending, response)
	}
	if pending.phase != "revert" && pending.phase != "abandon" {
		if err := p.allowed(p.control.editor); err != nil {
			return err
		}
	}
	if exact(response, "error") {
		details, _ := response["error"].(map[string]any)
		if pending.phase == "start" && details["code"] == "stale_generation" && p.control.retries > 0 {
			p.control.retries--
			return p.submit("inspect", "Send_Query {cookie} {serial} appearance 0 8 -")
		}
		p.finish("Preview refused; inspect current appearance before retrying.", true, false)
		return nil
	}
	if !exact(response, "result") {
		return errors.New("Invalid preview response")
	}
	result, ok := response["result"].(map[string]any)
	if !ok {
		return errors.New("Invalid preview result")
	}
	phase := pending.phase
	if phase == "inspect" {
		if p.control.saved != nil {
			if err := p.desktop.checkPreview(p.control); err != nil {
				return err
			}
		}
		generation := stringAt(result, "generation")
		if !identity(generation, 48) {
			return errors.New("Appearance generation unavailable")
		}
		return p.submit("start", "Send_Preview {cookie} {serial} "+p.control.view+" start "+generation+" - "+strconv.Itoa(p.control.opacity)+" 15000 "+hex.EncodeToString([]byte("SlateLauncherStyle")))
	}
	if result["scope"] != "menu-opacity" {
		return errors.New("Invalid preview scope")
	}
	state := stringAt(result, "state")
	if state == "active" {
		generation, lease := stringAt(result, "generation"), stringAt(result, "lease")
		if !identity(generation, 48) || !identity(lease, 48) {
			return errors.New("Invalid preview identity")
		}
		p.control.generation, p.control.lease = generation, lease
		if phase == "start" {
			if err := p.show(); err != nil {
				return err
			}
		}
		p.nextPoll = time.Now().Add(time.Second)
		if p.queued != "" {
			op := p.queued
			p.queued = ""
			if err := p.request(op); err != nil {
				p.desktop.notice(err.Error())
			}
		}
		return nil
	}
	if phase == "save" && state == "confirmed" {
		generation := newID(16)
		if err := p.desktop.savePreview(p.control); err != nil {
			p.finish("Runtime opacity kept; preference was not saved. Reload settings before retrying.", false, false)
		} else {
			p.desktop.changed(generation)
			_ = p.desktop.settingsMenu("all")
			p.finish("Launcher opacity saved. Runtime confirmation completed; rendering is unobserved.", false, true)
		}
		return nil
	}
	message := map[string]string{"confirmed": "Menu opacity kept for this runtime; preferences unchanged.", "reverted": "Previous menu opacity restored.",
		"expired": "Preview expired; previous menu opacity restored.", "conflicted": "Appearance changed elsewhere; automatic restore refused.", "abandoned": "Previous menu opacity restored."}[state]
	if message == "" {
		message = "Preview is no longer active."
	}
	p.finish(message, false, false)
	return nil
}
func (p *preview) tick() {
	now := time.Now()
	if p.pending != nil && !now.Before(p.pending.deadline) {
		p.finish("Preview reply timed out; core recovery remains responsible.", true, false)
	} else if p.control != nil && p.control.lease != "" && p.pending == nil && !now.Before(p.nextPoll) {
		if err := p.request("query"); err != nil {
			p.finish(err.Error(), true, false)
		}
	}
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/json"
	"errors"
	"fmt"
	"reflect"
	"strconv"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

type contrastCandidate struct {
	projection desktopsettings.ThemeProjection
	theme      []any
	state      string
	confirming bool
	shown      bool
}

func (p *preview) allowedControl() error {
	if p.control != nil && p.control.contrast != nil {
		return p.desktop.checkContrast(p.control)
	}
	return p.allowed(p.control.editor)
}

func (p *preview) startContrast(e *draft) error {
	if p.control != nil || p.pending != nil || !p.desktop.ready {
		return errors.New("A preview is already pending or desktop is not ready")
	}
	snapshot, candidate, err := p.desktop.prepareContrast()
	if err != nil {
		return err
	}
	p.control = &previewControl{view: newID(8), saved: snapshot, editor: e, contrast: candidate}
	return p.submit("inspect", "Send_Query {cookie} {serial} appearance 0 8 -")
}

// The versioned native plan has a fixed order and no user command fragments.
// The projection already validated all colors; recheck its exact finite shape
// before encoding so future role additions cannot silently expand this lease.
func contrastPlan(projection desktopsettings.ThemeProjection) (string, error) {
	ids := [...]int{0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14}
	if len(projection.Roles) != len(ids) {
		return "", errors.New("Invalid high contrast role count")
	}
	var encoded strings.Builder
	encoded.WriteString("1:")
	for i, role := range projection.Roles {
		if role.Colorset != ids[i] {
			return "", errors.New("Invalid high contrast role order")
		}
		for _, color := range []string{role.Foreground, role.Background, role.Highlight, role.Shadow} {
			if len(color) != 7 || color[0] != '#' || !identity(strings.ToLower(color[1:]), 6) {
				return "", errors.New("Invalid high contrast role color")
			}
			encoded.WriteString(strings.ToLower(color[1:]))
		}
	}
	return encoded.String(), nil
}

func contrastCapability(value any) error {
	capability, ok := value.(map[string]any)
	if !ok || capability["version"] != json.Number("1") ||
		capability["scope"] != "high-contrast" || capability["role_count"] != json.Number("14") ||
		capability["style_count"] != json.Number("2") || capability["metadata_max"] != json.Number("65536") ||
		capability["preparation_timeout_ms"] != json.Number("5000") || capability["lease_limit"] != json.Number("1") {
		return errors.New("High contrast resource preview capability is unavailable")
	}
	if capability["slot_available"] != true {
		return errors.New("Another native preview or preparation owns the resource slot")
	}
	return nil
}

func (p *preview) acceptContrastReply(pending *previewPending, response values) error {
	item := p.control
	if exact(response, "error") {
		details, _ := response["error"].(map[string]any)
		code := stringAt(details, "code")
		if !identifier(code) {
			return errors.New("Invalid high contrast refusal")
		}
		p.finish("High contrast preview refused: "+code+". Core recovery remains responsible.", true, false)
		return nil
	}
	if !exact(response, "result") {
		return errors.New("Invalid high contrast response")
	}
	result, ok := response["result"].(map[string]any)
	if !ok {
		return errors.New("Invalid high contrast result")
	}
	if pending.phase == "inspect" {
		if err := contrastCapability(result["resource_preview"]); err != nil {
			p.finish(err.Error(), false, false)
			return nil
		}
		if err := p.desktop.checkContrast(item); err != nil {
			p.finish(err.Error(), false, false)
			return nil
		}
		generation := stringAt(result, "generation")
		if !identity(generation, 48) {
			return errors.New("Appearance generation unavailable")
		}
		plan, err := contrastPlan(item.contrast.projection)
		if err != nil {
			return err
		}
		item.generation = generation
		return p.submit("prepare", "Send_Preview {cookie} {serial} "+item.view+" prepare "+generation+" - 0 15000 "+plan)
	}
	if result["scope"] != "high-contrast" || result["plan_version"] != json.Number("1") {
		return errors.New("Invalid high contrast preview scope or version")
	}
	state := stringAt(result, "state")
	remaining, number := result["remaining_ms"].(json.Number)
	remainingMS, numberErr := strconv.ParseUint(string(remaining), 10, 32)
	if !number || numberErr != nil || remainingMS > 30000 ||
		result["preparation_limit_ms"] != json.Number("5000") || result["lease_limit"] != json.Number("1") {
		return errors.New("Invalid high contrast preview bounds")
	}
	generation, lease := stringAt(result, "generation"), stringAt(result, "lease")
	if !identity(lease, 48) || item.lease != "" && lease != item.lease {
		return errors.New("Invalid high contrast lease identity")
	}
	if state != "terminal" && !identity(generation, 48) {
		return errors.New("Invalid high contrast generation")
	}
	if state != "terminal" && stringAt(result, "terminal_reason") != "" {
		return errors.New("Invalid high contrast terminal marker")
	}
	if !contrastTransition(item.contrast.state, state) {
		return errors.New("Invalid high contrast preview transition")
	}
	if (state == "preparing" || state == "prepared") && generation != item.generation {
		return errors.New("High contrast preparation generation changed")
	}
	if state == "active" && item.contrast.state == "active" && generation != item.generation {
		return errors.New("Active high contrast generation changed")
	}
	if state == "terminal" {
		item.contrast.state = "terminal"
		return p.finishContrast(result)
	}
	item.generation, item.lease = generation, lease
	item.contrast.state = state
	switch state {
	case "prepared":
		if err := p.desktop.checkContrast(item); err != nil {
			p.finish(err.Error(), true, false)
			return nil
		}
		return p.submit("apply", "Send_Preview {cookie} {serial} "+item.view+" apply "+generation+" "+lease+" 0 0 -")
	case "preparing", "applying", "recovery-pending":
		p.nextPoll = time.Now().Add(100 * time.Millisecond)
		return nil
	case "active":
		if err := p.desktop.checkContrast(item); err != nil {
			p.finish(err.Error(), true, false)
			return nil
		}
		if !item.contrast.shown {
			if err := p.show(); err != nil {
				return err
			}
			item.contrast.shown = true
		}
		p.nextPoll = time.Now().Add(time.Second)
		if p.queued != "" {
			op := p.queued
			p.queued = ""
			return p.request(op)
		}
		return nil
	}
	return fmt.Errorf("Invalid high contrast preview state: %s", state)
}

func contrastTransition(previous, next string) bool {
	if next == "terminal" || next == "recovery-pending" {
		return previous != "terminal"
	}
	switch previous {
	case "preparing":
		return next == "preparing" || next == "prepared"
	case "prepared", "applying":
		return next == "applying" || next == "active"
	case "active":
		return next == "active"
	}
	return false
}

func (p *preview) finishContrast(result values) error {
	reason, recovery := stringAt(result, "terminal_reason"), stringAt(result, "recovery")
	if reason == "confirmed" {
		if !p.control.contrast.confirming || recovery != "kept" ||
			stringAt(result, "generation") != p.control.generation {
			return errors.New("Unexpected high contrast confirmation")
		}
		if err := p.desktop.confirmContrast(p.control); err != nil {
			p.finish("Runtime high contrast kept; preference was not saved. Reload settings before retrying.", false, false)
		} else {
			p.finish("High contrast saved. Native resource confirmation completed; rendering is unobserved.", false, true)
		}
		return nil
	}
	message := map[string]string{
		"reverted":   "High contrast preview cancelled; previous resources restored.",
		"expired":    "High contrast preview expired; previous resources restored.",
		"abandoned":  "High contrast preview abandoned; previous resources restored.",
		"conflicted": "Appearance changed elsewhere; automatic restore refused.",
		"refused":    "High contrast preparation refused; previous resources retained.",
	}[reason]
	if message == "" {
		return errors.New("Invalid high contrast terminal result")
	}
	if reason == "refused" {
		preparationError := stringAt(result, "preparation_error")
		if len(preparationError) > 256 {
			return errors.New("Invalid high contrast preparation refusal")
		}
		if preparationError != "" {
			message += " " + preparationError
		}
	}
	if recovery == "refused-conflict" {
		message = "Appearance changed elsewhere; automatic restore refused."
	} else if recovery != "restored" && recovery != "not-applied" {
		return errors.New("Invalid high contrast recovery result")
	}
	p.finish(message, false, false)
	return nil
}

// Candidate construction never consumes editor values: the temporary resource
// change is precisely normal -> high in the currently submitted presentation.
func (d *Controller) prepareContrast() (*previewSnapshot, *contrastCandidate, error) {
	if !d.ready || d.persistenceError != "" {
		return nil, nil, errors.New("High contrast preview requires readable settings and a ready desktop")
	}
	if _, overridden := d.overrides["contrast"]; overridden {
		return nil, nil, errors.New("Contrast has a module configuration override")
	}
	if d.effective["contrast"] != "normal" {
		return nil, nil, errors.New("High contrast is already active")
	}
	now := time.Now()
	key, _, _, err := d.themePlan(d.effective, now.Hour()*60+now.Minute())
	if err != nil {
		return nil, nil, err
	}
	if len(d.lastTheme) != 8 || !reflect.DeepEqual(key, d.lastTheme) {
		return nil, nil, errors.New("Current theme is unavailable or changing; reopen the preview")
	}
	projection, err := desktopsettings.ProjectTheme(d.palettes[key[0].(string)][key[1].(string)], key[1].(string), "high")
	if err != nil {
		return nil, nil, err
	}
	_, revision, err := loadSettings(d.statePath)
	if err != nil {
		return nil, nil, err
	}
	if !sameRevision(revision, d.revision) {
		return nil, nil, errors.New("Settings changed externally; reload before previewing")
	}
	return &previewSnapshot{d.generation, d.revision}, &contrastCandidate{
		projection: projection, theme: append([]any(nil), key...), state: "preparing"}, nil
}

func (d *Controller) checkContrast(control *previewControl) error {
	if control == nil || control.contrast == nil || control.saved == nil {
		return errors.New("High contrast preview controls are stale")
	}
	snapshot, candidate, err := d.prepareContrast()
	if err != nil {
		return err
	}
	if snapshot.generation != control.saved.generation ||
		!sameRevision(snapshot.revision, control.saved.revision) ||
		!reflect.DeepEqual(candidate.theme, control.contrast.theme) ||
		!reflect.DeepEqual(candidate.projection, control.contrast.projection) {
		return errors.New("Settings or theme changed; reopen the high contrast preview")
	}
	return nil
}

// Confirmation has already committed the native resource change. Even a failed
// preference write must remember that runtime state, so clock ticks cannot
// silently replay normal contrast. Explicit contrast edits/reload replace it.
func (d *Controller) confirmContrast(control *previewControl) error {
	err := d.checkContrast(control)
	if err == nil {
		candidate, checkedErr := desktopsettings.Checked(merged(d.saved, values{"contrast": "high"}))
		if checkedErr != nil {
			err = checkedErr
		} else {
			var revision *string
			revision, err = saveSettings(d.statePath, candidate, control.saved.revision)
			if err == nil {
				d.saved, d.revision = candidate, revision
			}
		}
	}
	d.effective = merged(d.effective, values{"contrast": "high"})
	d.runtimeContrast = err != nil
	if len(d.lastTheme) == 8 {
		d.lastTheme = append([]any(nil), d.lastTheme...)
		d.lastTheme[7] = "high"
	}
	if err == nil && control.editor != nil {
		if _, included := control.editor.values["contrast"]; included {
			control.editor.values["contrast"] = "high"
		}
	}
	d.changed(newID(16))
	_ = d.settingsMenu("all")
	return err
}

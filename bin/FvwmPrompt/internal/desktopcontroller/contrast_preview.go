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

type resourceCandidate struct {
	key, value      string
	scope           string
	planVersion     int
	projection      desktopsettings.ThemeProjection
	baseline, theme []any
	state           string
	confirming      bool
	shown           bool
}

func (c *resourceCandidate) capability() string {
	if c.planVersion == 1 {
		return "resource_preview"
	}
	return "opaque_colors_preview"
}
func (c *resourceCandidate) label() string {
	if c.key == "contrast" {
		return "high contrast"
	}
	return c.key
}
func (p *preview) allowedControl() error {
	if p.control != nil && p.control.resource != nil {
		return p.desktop.checkResource(p.control)
	}
	return p.allowed(p.control.editor)
}
func (p *preview) startContrast(e *draft) error {
	return p.startResource("contrast", "high", e)
}
func (p *preview) startResource(key, value string, e *draft) error {
	if p.control != nil || p.pending != nil || !p.desktop.ready {
		return errors.New("A preview is already pending or desktop is not ready")
	}
	snapshot, candidate, err := p.desktop.prepareResource(key, value)
	if err != nil {
		return err
	}
	p.control = &previewControl{view: newID(8), saved: snapshot, editor: e, resource: candidate}
	return p.submit("inspect", "Send_Query {cookie} {serial} appearance 0 8 -")
}

// Both fixed plans share exactly fourteen roles and the resource lease. The
// second kind changes only role colors; the core retains actual menu scalars.
func resourcePlan(projection desktopsettings.ThemeProjection, version int) (string, error) {
	ids := [...]int{0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14}
	if len(projection.Roles) != len(ids) || (version != 1 && version != 2) {
		return "", errors.New("Invalid resource plan shape or version")
	}
	var encoded strings.Builder
	encoded.WriteString(strconv.Itoa(version) + ":")
	for i, role := range projection.Roles {
		if role.Colorset != ids[i] {
			return "", errors.New("Invalid resource role order")
		}
		for _, color := range []string{role.Foreground, role.Background, role.Highlight, role.Shadow} {
			if len(color) != 7 || color[0] != '#' || !identity(strings.ToLower(color[1:]), 6) {
				return "", errors.New("Invalid resource role color")
			}
			encoded.WriteString(strings.ToLower(color[1:]))
		}
	}
	return encoded.String(), nil
}
func resourceCapability(value any, candidate *resourceCandidate) error {
	capability, ok := value.(map[string]any)
	if !ok || capability["version"] != json.Number("1") ||
		capability["scope"] != candidate.scope || capability["role_count"] != json.Number("14") ||
		capability["style_count"] != json.Number("2") || capability["metadata_max"] != json.Number("65536") ||
		capability["preparation_timeout_ms"] != json.Number("5000") || capability["lease_limit"] != json.Number("1") ||
		candidate.planVersion == 2 && capability["plan_version"] != json.Number("2") {
		return errors.New("Appearance resource preview capability is unavailable")
	}
	if capability["slot_available"] != true {
		return errors.New("Another native preview or preparation owns the resource slot")
	}
	return nil
}

func (p *preview) acceptResourceReply(pending *previewPending, response values) error {
	item := p.control
	if exact(response, "error") {
		details, _ := response["error"].(map[string]any)
		code := stringAt(details, "code")
		if !identifier(code) {
			return errors.New("Invalid resource refusal")
		}
		p.finish("Resource preview refused: "+code+". Core recovery remains responsible.", true, false)
		return nil
	}
	if !exact(response, "result") {
		return errors.New("Invalid resource response")
	}
	result, ok := response["result"].(map[string]any)
	if !ok {
		return errors.New("Invalid resource result")
	}
	if pending.phase == "inspect" {
		if err := resourceCapability(result[item.resource.capability()], item.resource); err != nil {
			p.finish(err.Error(), false, false)
			return nil
		}
		if err := p.desktop.checkResource(item); err != nil {
			p.finish(err.Error(), false, false)
			return nil
		}
		generation := stringAt(result, "generation")
		if !identity(generation, 48) {
			return errors.New("Appearance generation unavailable")
		}
		plan, err := resourcePlan(item.resource.projection, item.resource.planVersion)
		if err != nil {
			return err
		}
		item.generation = generation
		return p.submit("prepare", "Send_Preview {cookie} {serial} "+item.view+" prepare "+generation+" - 0 15000 "+plan)
	}
	if result["scope"] != item.resource.scope || result["plan_version"] != json.Number(strconv.Itoa(item.resource.planVersion)) {
		return errors.New("Invalid resource preview scope or version")
	}
	state := stringAt(result, "state")
	remaining, number := result["remaining_ms"].(json.Number)
	remainingMS, numberErr := strconv.ParseUint(string(remaining), 10, 32)
	if !number || numberErr != nil || remainingMS > 30000 ||
		result["preparation_limit_ms"] != json.Number("5000") || result["lease_limit"] != json.Number("1") {
		return errors.New("Invalid resource preview bounds")
	}
	generation, lease := stringAt(result, "generation"), stringAt(result, "lease")
	if !identity(lease, 48) || item.lease != "" && lease != item.lease {
		return errors.New("Invalid resource lease identity")
	}
	if state != "terminal" && !identity(generation, 48) {
		return errors.New("Invalid resource generation")
	}
	if state != "terminal" && stringAt(result, "terminal_reason") != "" {
		return errors.New("Invalid resource terminal marker")
	}
	if !resourceTransition(item.resource.state, state) {
		return errors.New("Invalid resource preview transition")
	}
	if (state == "preparing" || state == "prepared") && generation != item.generation {
		return errors.New("Resource preparation generation changed")
	}
	if state == "active" && item.resource.state == "active" && generation != item.generation {
		return errors.New("Active resource generation changed")
	}
	if state == "terminal" {
		item.resource.state = "terminal"
		return p.finishResource(result)
	}
	item.generation, item.lease = generation, lease
	item.resource.state = state
	switch state {
	case "prepared":
		if err := p.desktop.checkResource(item); err != nil {
			p.finish(err.Error(), true, false)
			return nil
		}
		return p.submit("apply", "Send_Preview {cookie} {serial} "+item.view+" apply "+generation+" "+lease+" 0 0 -")
	case "preparing", "applying", "recovery-pending":
		p.nextPoll = time.Now().Add(100 * time.Millisecond)
		return nil
	case "active":
		if err := p.desktop.checkResource(item); err != nil {
			p.finish(err.Error(), true, false)
			return nil
		}
		if !item.resource.shown {
			if err := p.show(); err != nil {
				return err
			}
			item.resource.shown = true
		}
		p.nextPoll = time.Now().Add(time.Second)
		if p.queued != "" {
			op := p.queued
			p.queued = ""
			return p.request(op)
		}
		return nil
	}
	return fmt.Errorf("Invalid resource preview state: %s", state)
}

func resourceTransition(previous, next string) bool {
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

func (p *preview) finishResource(result values) error {
	reason, recovery := stringAt(result, "terminal_reason"), stringAt(result, "recovery")
	if reason == "confirmed" {
		if !p.control.resource.confirming || recovery != "kept" ||
			stringAt(result, "generation") != p.control.generation {
			return errors.New("Unexpected resource confirmation")
		}
		if err := p.desktop.confirmResource(p.control); err != nil {
			p.finish("Runtime "+p.control.resource.label()+" kept; preference was not saved. Reload settings before retrying.", false, false)
		} else {
			p.finish(title(p.control.resource.label())+" saved. Native resource confirmation completed; rendering is unobserved.", false, true)
		}
		return nil
	}
	message := map[string]string{
		"reverted":   "Resource preview cancelled; previous resources restored.",
		"expired":    "Resource preview expired; previous resources restored.",
		"abandoned":  "Resource preview abandoned; previous resources restored.",
		"conflicted": "Appearance changed elsewhere; automatic restore refused.",
		"refused":    "Resource preparation refused; previous resources retained.",
	}[reason]
	if message == "" {
		return errors.New("Invalid resource terminal result")
	}
	if reason == "refused" {
		preparationError := stringAt(result, "preparation_error")
		if len(preparationError) > 256 {
			return errors.New("Invalid resource preparation refusal")
		}
		if preparationError != "" {
			message += " " + preparationError
		}
	}
	if recovery == "refused-conflict" {
		message = "Appearance changed elsewhere; automatic restore refused."
	} else if recovery != "restored" && recovery != "not-applied" {
		return errors.New("Invalid resource recovery result")
	}
	p.finish(message, false, false)
	return nil
}

// Preparation reads current submitted state, never unrelated editor fields.
func (d *Controller) prepareResource(key, value string) (*previewSnapshot, *resourceCandidate, error) {
	now := time.Now()
	return d.prepareResourceAt(key, value, now.Hour()*60+now.Minute())
}
func (d *Controller) prepareResourceAt(key, value string, minute int) (*previewSnapshot, *resourceCandidate, error) {
	if !d.ready || d.persistenceError != "" {
		return nil, nil, errors.New("Appearance preview requires readable settings and a ready desktop")
	}
	if _, overridden := d.overrides[key]; overridden {
		return nil, nil, fmt.Errorf("%s has a module configuration override", title(key))
	}
	if _, err := desktopsettings.Value(key, value, false); err != nil {
		return nil, nil, err
	}
	version, scope := 2, "opaque-colors"
	switch key {
	case "contrast":
		if value != "high" || d.effective["contrast"] != "normal" {
			return nil, nil, errors.New("High contrast is already active or the candidate is unsupported")
		}
		version, scope = 1, "high-contrast"
	case "appearance":
		if value != "light" && value != "dark" {
			return nil, nil, errors.New("Choose Light or Dark before previewing appearance; Auto uses the schedule")
		}
	case "palette":
	default:
		return nil, nil, errors.New("This setting has no native resource preview")
	}
	if d.effective[key] == value {
		return nil, nil, errors.New("This appearance value is already active")
	}
	baseline, _, _, err := d.themePlan(d.effective, minute)
	if err != nil {
		return nil, nil, err
	}
	if len(d.lastTheme) != 8 || !reflect.DeepEqual(baseline, d.lastTheme) {
		return nil, nil, errors.New("Current theme is unavailable or changing; reopen the preview")
	}
	if version == 2 {
		presentation, err := desktopsettings.ResolvePresentation(d.effective)
		if err != nil {
			return nil, nil, err
		}
		if presentation.Transparency {
			return nil, nil, errors.New("Disable transparency or enable high contrast before previewing palette or appearance")
		}
	}
	settings, err := desktopsettings.Checked(merged(d.effective, values{key: value}))
	if err != nil {
		return nil, nil, err
	}
	theme, projection, _, err := d.themePlan(settings, minute)
	if err != nil {
		return nil, nil, err
	}
	if baseline[5] != theme[5] || baseline[6] != theme[6] {
		return nil, nil, errors.New("Keep the current wallpaper or choose independent wallpaper settings before previewing")
	}
	_, revision, err := loadSettings(d.statePath)
	if err != nil {
		return nil, nil, err
	}
	if !sameRevision(revision, d.revision) {
		return nil, nil, errors.New("Settings changed externally; reload before previewing")
	}
	return &previewSnapshot{d.generation, d.revision}, &resourceCandidate{key: key, value: value,
		planVersion: version, scope: scope, projection: projection,
		baseline: append([]any(nil), baseline...), theme: append([]any(nil), theme...), state: "preparing"}, nil
}
func (d *Controller) checkResource(control *previewControl) error {
	if control == nil || control.resource == nil || control.saved == nil {
		return errors.New("Appearance preview controls are stale")
	}
	old := control.resource
	snapshot, candidate, err := d.prepareResource(old.key, old.value)
	if err != nil {
		return err
	}
	if snapshot.generation != control.saved.generation ||
		!sameRevision(snapshot.revision, control.saved.revision) || candidate.scope != old.scope || candidate.planVersion != old.planVersion ||
		!reflect.DeepEqual(candidate.baseline, old.baseline) || !reflect.DeepEqual(candidate.theme, old.theme) ||
		!reflect.DeepEqual(candidate.projection, old.projection) {
		return errors.New("Settings or theme changed; reopen the appearance preview")
	}
	return nil
}
func (d *Controller) runtimeUnsaved(key string) bool { _, ok := d.runtimeHeld[key]; return ok }

// Native confirmation has already kept the resource bundle. Remember each kept
// value even if the file changed or its write fails; later ticks and unrelated
// edits must not replay the older saved setting over it. Only these three enum
// keys can acquire an overlay; no arbitrary registry entries are retained.
func (d *Controller) confirmResource(control *previewControl) error {
	if control == nil || control.resource == nil || control.saved == nil {
		return errors.New("Appearance preview controls are stale")
	}
	resource := control.resource
	if resource.key != "contrast" && resource.key != "palette" && resource.key != "appearance" {
		return errors.New("Invalid confirmed resource setting")
	}
	if _, err := desktopsettings.Value(resource.key, resource.value, false); err != nil {
		return err
	}
	if len(resource.theme) != 8 {
		return errors.New("Invalid confirmed resource theme")
	}
	paletteName, paletteOK := resource.theme[0].(string)
	mode, modeOK := resource.theme[1].(string)
	if !paletteOK || !modeOK || d.palettes[paletteName][mode] == nil {
		return errors.New("Invalid confirmed resource palette")
	}
	err := d.checkResource(control)
	if err == nil {
		candidate, checkedErr := desktopsettings.Checked(merged(d.saved, values{resource.key: resource.value}))
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
	d.effective = merged(d.effective, values{resource.key: resource.value})
	if err != nil {
		if d.runtimeHeld == nil {
			d.runtimeHeld = map[string]string{}
		}
		d.runtimeHeld[resource.key] = resource.value
	} else {
		delete(d.runtimeHeld, resource.key)
	}
	// The fixed resource operation owns colors only (and version1 opacity).
	// Cache its submitted projection without calling theme's wallpaper,
	// animation or whole-menu side effects.
	d.lastTheme = append([]any(nil), resource.theme...)
	d.lastThemeAppearance = mode
	palette := d.palettes[paletteName][mode]
	d.lastThemeColors = make(map[string]string, len(palette))
	for name, color := range palette {
		d.lastThemeColors[name] = color
	}
	d.button("appearance", title(paletteName))
	if err == nil && control.editor != nil {
		if _, included := control.editor.values[resource.key]; included {
			control.editor.values[resource.key] = resource.value
		}
	}
	d.changed(newID(16))
	_ = d.settingsMenu("all")
	return err
}

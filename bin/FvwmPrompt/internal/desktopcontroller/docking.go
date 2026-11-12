// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
package desktopcontroller

import (
	"encoding/base64"
	"encoding/hex"
	"errors"
	"fmt"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

const (
	dockingForm       = "SlateDockingForm"
	dockingReplyLimit = 2048
	dockingReplyWait  = 5 * time.Second
	dockingPlanWait   = 15 * time.Second
)

type dockingEditor struct {
	profile  dockingProfile
	revision *string
}

type dockingPlan struct {
	name, generation, monitor         string
	profileRevision, settingsRevision *string
	windows, matched                  int
	desk                              *int
	panel, fallback                   bool
	serial                            uint64
	deadline                          time.Time
}

type dockingRequest struct {
	serial   uint64
	apply    bool
	plan     *dockingPlan
	deadline time.Time
	data     []byte
	parts    int
}

type dockingControl struct {
	cookie, token string
	serial        uint64
	editor        *dockingEditor
	plan          *dockingPlan
	pending       *dockingRequest
	panelAdapter  bool
}

func (d *Controller) dockingDirectory() string {
	return filepath.Join(filepath.Dir(d.statePath), "docking-profiles")
}

func (d *Controller) dockingCallback() string {
	d.docking.token = newID(16)
	return "Command SendToModule FvwmDesktop dock-control " + d.docking.token + " "
}

func (d *Controller) showDockingForm(lines []string) {
	closeCommand := "SendToModule FvwmDesktop dock-control " + d.docking.token + " close"
	lines = append([]string{"CloseCommand " + closeCommand}, lines...)
	lines = append(lines, "Line right", `Button quit "Close" ^[`, "Command "+closeCommand)
	d.form(dockingForm, lines, true)
}

func (d *Controller) retireDockingForm() {
	d.docking.token = ""
	d.emit("KillModule FvwmForm " + dockingForm)
}

func (d *Controller) startDocking(name string) error {
	if !d.ready || d.closed || d.docking.pending != nil {
		return errors.New("Docking profiles are busy or unavailable")
	}
	if name != "" {
		return d.editDocking(name)
	}
	d.docking.editor, d.docking.plan = nil, nil
	callback := d.dockingCallback()
	d.showDockingForm([]string{`Title "Docking profiles"`, "Line left",
		`Text "Profile name"`, "InputHex dock_name 24 x", `Button continue "Open" ^M`,
		callback + "open x$(dock_name:Base64)"})
	return d.fatal
}

func (d *Controller) editDocking(name string) error {
	profile, revision, err := loadDockingProfile(d.dockingDirectory(), name)
	if err != nil {
		return err
	}
	if revision == nil {
		profile.Name = name
	}
	d.docking.plan = nil
	d.docking.editor = &dockingEditor{profile: profile, revision: revision}
	d.showDockingEditor()
	return d.fatal
}

func dockingInput(name string, width int, value string) string {
	return fmt.Sprintf("InputHex %s %d x%s", name, width, hex.EncodeToString([]byte(value)))
}

func (d *Controller) showDockingEditor() {
	editor := d.docking.editor
	p := editor.profile
	desk := "-"
	if p.Desk != nil {
		desk = strconv.Itoa(*p.Desk)
	}
	panelOn, panelOff := "off", "on"
	if p.Panel {
		panelOn, panelOff = "on", "off"
	}
	callback := d.dockingCallback()
	lines := []string{`Title "Docking profile"`, "Line left", "Text " + quote(p.Name),
		"Line left", `Text "RandR connector"`, dockingInput("dock_connector", 24, p.Connector),
		"Line left", `Text "Workspace (- keeps current)"`, dockingInput("dock_desk", 10, desk),
		"Selection dock_panel single", "Line left",
		"Choice dock_panel_no off " + panelOff + ` "Keep panel"`,
		"Choice dock_panel_yes on " + panelOn + ` "Move panel on Apply"`,
		"Line left", fmt.Sprintf(`Label dock_count 28 "%d window selectors"`, len(p.Windows)),
		"Line left", `Text "WM_CLASS instance"`, dockingInput("dock_instance", 24, ""),
		"Line left", `Text "WM_CLASS class"`, dockingInput("dock_class", 24, ""),
		"Line left", `Text "Pixel offsets from work area"`, dockingInput("dock_x", 7, "0"), dockingInput("dock_y", 7, "0"),
		"Line left", `Button continue "Add window"`,
		callback + "add x$(dock_instance:Base64) x$(dock_class:Base64) x$(dock_x:Base64) x$(dock_y:Base64)",
		`Button continue "Remove last"`, callback + "remove",
		"Line left", `Button continue "Save"`, callback + "save x$(dock_connector:Base64) x$(dock_desk:Base64) $(dock_panel)",
	}
	if editor.revision != nil {
		lines = append(lines, `Button continue "Preview saved"`, callback+"preview")
	}
	d.showDockingForm(lines)
}

func dockingTextArgument(encoded string, maximum int) (string, error) {
	if len(encoded) == 0 || len(encoded) > 1+base64.RawURLEncoding.EncodedLen(maximum) || encoded[0] != 'x' {
		return "", errors.New("Invalid docking field")
	}
	raw, err := base64.RawURLEncoding.Strict().DecodeString(encoded[1:])
	if err != nil || len(raw) > maximum {
		return "", errors.New("Invalid docking field")
	}
	return string(raw), nil
}

func dockingDecimal(text string, low, high int) (int, bool) {
	number, err := strconv.Atoi(text)
	return number, err == nil && strconv.Itoa(number) == text && number >= low && number <= high
}

func (d *Controller) updateDockingWindows(words []string) error {
	editor := d.docking.editor
	switch {
	case len(words) == 1 && words[0] == "remove":
		n := len(editor.profile.Windows)
		if n == 0 {
			return errors.New("No window selector to remove")
		}
		editor.profile.Windows[n-1] = dockingWindow{}
		editor.profile.Windows = editor.profile.Windows[:n-1]
	case len(words) == 5 && words[0] == "add":
		instance, ie := dockingTextArgument(words[1], 256)
		class, ce := dockingTextArgument(words[2], 256)
		xText, xe := dockingTextArgument(words[3], 7)
		yText, ye := dockingTextArgument(words[4], 7)
		x, xOK := dockingDecimal(xText, -dockingOffsetLimit, dockingOffsetLimit)
		y, yOK := dockingDecimal(yText, -dockingOffsetLimit, dockingOffsetLimit)
		if ie != nil || ce != nil || xe != nil || ye != nil || !xOK || !yOK ||
			!validDockingText(instance) || !validDockingText(class) {
			return errors.New("Enter exact WM_CLASS values and signed pixel offsets")
		}
		// The connector is saved separately, so an unfinished header cannot
		// prevent an otherwise valid selector from joining the draft.
		if len(editor.profile.Windows) >= dockingWindowLimit || len(instance)+len(class) > dockingSelectorLimit {
			return errors.New("Docking window selector limit exceeded")
		}
		for _, window := range editor.profile.Windows {
			if window.Instance == instance && window.Class == class {
				return errors.New("Duplicate docking window selector")
			}
		}
		editor.profile.Windows = append(editor.profile.Windows,
			dockingWindow{Instance: instance, Class: class, OffsetX: x, OffsetY: y})
	default:
		return errors.New("Invalid docking window action")
	}
	d.emit(fmt.Sprintf(`SendToModule %s SetLabel dock_count "%d window selectors"`,
		dockingForm, len(editor.profile.Windows)))
	return d.fatal
}

func (d *Controller) saveDockingEditor(words []string) error {
	if len(words) != 3 || !oneOf(words[2], "on", "off") {
		return errors.New("Invalid docking settings")
	}
	connector, ce := dockingTextArgument(words[0], 256)
	deskText, de := dockingTextArgument(words[1], 10)
	if ce != nil || de != nil {
		return errors.New("Invalid docking settings")
	}
	editor := d.docking.editor
	candidate := editor.profile
	candidate.Connector, candidate.Desk, candidate.Panel = connector, nil, words[2] == "on"
	if deskText != "-" {
		desk, ok := dockingDecimal(deskText, 0, dockingDeskLimit)
		if !ok {
			return errors.New("Workspace must be a nonnegative desk number or -")
		}
		candidate.Desk = &desk
	}
	revision, err := saveDockingProfile(d.dockingDirectory(), candidate, editor.revision)
	if err != nil {
		return err
	}
	editor.profile, editor.revision = candidate, revision
	d.showDockingEditor()
	return d.fatal
}

func (d *Controller) dockingAction(argument string) error {
	if len(argument) > 1024 {
		return errors.New("Docking control exceeds its limit")
	}
	words := splitFields(argument)
	if len(words) < 2 {
		return errors.New("Invalid docking control")
	}
	state := &d.docking
	if state.token == "" || words[0] != state.token {
		return nil
	}
	action := words[1]
	if action == "close" && len(words) == 2 {
		state.token = ""
		state.editor, state.plan = nil, nil
		// An Apply already sent to the core has no cancel operation.
		if state.pending != nil && !state.pending.apply {
			state.pending = nil
		}
		return nil
	}
	if state.pending != nil {
		return errors.New("Docking is waiting for the core")
	}
	switch {
	case action == "open" && len(words) == 3:
		name, err := dockingTextArgument(words[2], 32)
		if err != nil {
			return err
		}
		return d.editDocking(name)
	case action == "apply" && len(words) == 2 && state.plan != nil:
		return d.applyDocking()
	case state.editor == nil:
		return errors.New("Invalid docking control")
	case action == "preview" && len(words) == 2:
		return d.previewDocking(state.editor.profile.Name)
	case action == "add" || action == "remove":
		return d.updateDockingWindows(words[1:])
	case action == "save":
		return d.saveDockingEditor(words[2:])
	default:
		return errors.New("Invalid docking control")
	}
}

func (d *Controller) currentDockingSettings(plan *dockingPlan) bool {
	if d.generation != plan.generation || !sameRevision(d.revision, plan.settingsRevision) {
		return false
	}
	_, actual, err := loadSettings(d.statePath)
	return err == nil && sameRevision(actual, plan.settingsRevision)
}

func (d *Controller) nextDockingSerial() uint64 {
	state := &d.docking
	state.serial++
	return state.serial
}

func (d *Controller) sendDocking(serial uint64, command string) {
	d.emit(fmt.Sprintf("Send_Dock %s %016x %s", d.docking.cookie, serial, command))
}

func (d *Controller) previewDocking(name string) error {
	profile, revision, err := loadDockingProfile(d.dockingDirectory(), name)
	if err != nil {
		return err
	}
	if revision == nil {
		return errors.New("Save the docking profile before preview")
	}
	if len(profile.Windows) == 0 && profile.Desk == nil && !profile.Panel {
		return errors.New("Add a window, workspace, or panel choice before preview")
	}
	plan := &dockingPlan{
		name: name, generation: d.generation, profileRevision: revision,
		settingsRevision: d.revision, windows: len(profile.Windows), desk: profile.Desk, panel: profile.Panel,
	}
	if plan.panel && !d.docking.panelAdapter {
		return errors.New("Panel docking is unavailable in the active desktop configuration")
	}
	if !d.currentDockingSettings(plan) {
		return errors.New("Desktop settings changed externally; reload before preview")
	}
	// Reserve separate serials for both halves of the prepared operation.
	if d.docking.cookie == "" || d.docking.serial >= ^uint64(0)-1 {
		d.docking.cookie, d.docking.serial = newID(16), 0
	}
	plan.serial = d.nextDockingSerial()
	d.docking.pending = &dockingRequest{serial: plan.serial, plan: plan, deadline: time.Now().Add(dockingReplyWait)}
	desk := "-"
	if profile.Desk != nil {
		desk = strconv.Itoa(*profile.Desk)
	}
	panel := 0
	if profile.Panel {
		panel = 1
	}
	d.sendDocking(plan.serial, fmt.Sprintf("begin %s %s %d %d", hex.EncodeToString([]byte(profile.Connector)), desk, plan.windows, panel))
	for _, w := range profile.Windows {
		d.sendDocking(plan.serial, fmt.Sprintf("record %s %s %d %d",
			hex.EncodeToString([]byte(w.Instance)), hex.EncodeToString([]byte(w.Class)), w.OffsetX, w.OffsetY))
	}
	d.sendDocking(plan.serial, "prepare")
	return d.fatal
}

func (d *Controller) applyDocking() error {
	plan := d.docking.plan
	d.docking.plan = nil
	d.retireDockingForm()
	if !time.Now().Before(plan.deadline) {
		return errors.New("Docking preview expired; preview again")
	}
	_, revision, err := loadDockingProfile(d.dockingDirectory(), plan.name)
	if err != nil || !sameRevision(revision, plan.profileRevision) || !d.currentDockingSettings(plan) {
		return errors.New("Docking profile or desktop settings changed; preview again")
	}
	if plan.panel && !d.docking.panelAdapter {
		return errors.New("Panel docking is unavailable in the active desktop configuration")
	}
	serial := d.nextDockingSerial()
	d.docking.pending = &dockingRequest{serial: serial, apply: true, plan: plan, deadline: time.Now().Add(dockingReplyWait)}
	d.sendDocking(serial, fmt.Sprintf("apply %016x", plan.serial))
	return d.fatal
}

func (d *Controller) showDockingPlan(plan *dockingPlan) {
	lines := []string{`Title "Apply docking profile"`}
	lines = diagnosticText(lines, plan.name+" → "+plan.monitor)
	if plan.fallback {
		lines = diagnosticText(lines, "Connector missing. Using the primary monitor.")
	}
	lines = diagnosticText(lines, fmt.Sprintf("%d matching windows, %d skipped", plan.matched, plan.windows-plan.matched))
	if plan.desk != nil {
		lines = diagnosticText(lines, fmt.Sprintf("Select workspace %d on this monitor.", *plan.desk))
	}
	if plan.panel {
		lines = diagnosticText(lines, "Move the desktop panel to this monitor.")
	}
	callback := d.dockingCallback()
	d.showDockingForm(append(lines, "Line right", `Button continue "Apply"`, callback+"apply"))
}

type dockingResult struct {
	changed      int
	fallback     bool
	monitor      string
	panelApplied bool
}

func decodeDockingResult(data []byte, apply bool, count int) (dockingResult, error) {
	decoded, decodeErr := desktopsettings.DecodeJSON(data)
	fields, ok := decoded.(desktopsettings.Values)
	if decodeErr != nil || !ok || len(fields) != 1 {
		return dockingResult{}, errors.New("Invalid docking reply")
	}
	if failure, ok := fields["error"].(desktopsettings.Values); ok && len(failure) == 1 {
		switch failure["code"] {
		case "stale-plan":
			return dockingResult{}, errors.New("Docking placement changed; preview again")
		case "ambiguous-selector":
			return dockingResult{}, errors.New("Multiple windows match a docking selector")
		case "unavailable":
			return dockingResult{}, errors.New("Docking destination is unavailable")
		case "busy":
			return dockingResult{}, errors.New("The core is busy with another docking operation")
		default:
			return dockingResult{}, errors.New("The core refused the docking request")
		}
	}
	result, ok := fields["result"].(desktopsettings.Values)
	if !ok || len(result) != 4 {
		return dockingResult{}, errors.New("Invalid docking result")
	}
	key := "matched"
	if apply {
		key = "applied"
	}
	changed, changedOK := dockingInteger(result[key], 0, count)
	skipped, skippedOK := dockingInteger(result["skipped"], 0, count)
	fallback, fallbackOK := result["fallback"].(bool)
	if !changedOK || !skippedOK || changed+skipped != count || !fallbackOK {
		return dockingResult{}, errors.New("Invalid docking counts")
	}
	out := dockingResult{changed: changed, fallback: fallback}
	if apply {
		out.panelApplied, ok = result["panel_applied"].(bool)
		if !ok {
			return dockingResult{}, errors.New("Invalid docking panel result")
		}
	} else {
		out.monitor, ok = result["monitor"].(string)
		if !ok || !validDockingText(out.monitor) {
			return dockingResult{}, errors.New("Invalid docking monitor")
		}
	}
	return out, nil
}

func (d *Controller) dockingReply(message string) bool {
	state := &d.docking
	if state.cookie == "" || !strings.HasPrefix(message, "MFL_CORE_1 "+state.cookie+" ") {
		return false
	}
	pending := state.pending
	if pending == nil {
		return true
	}
	prefix := fmt.Sprintf("MFL_CORE_1 %s %016x ", state.cookie, pending.serial)
	if !strings.HasPrefix(message, prefix) {
		return true
	}
	if !time.Now().Before(pending.deadline) {
		d.dockingTick(time.Now())
		return true
	}
	done, err := coreFragment(strings.Fields(message[len(prefix):]), &pending.data, &pending.parts, dockingReplyLimit)
	if err == nil && !done {
		return true
	}
	state.pending = nil
	if err != nil {
		d.notice(err.Error())
		return true
	}
	plan := pending.plan
	result, err := decodeDockingResult(pending.data, pending.apply, plan.windows)
	if err != nil {
		d.notice(err.Error())
		return true
	}
	if !pending.apply {
		plan.matched, plan.fallback, plan.monitor = result.changed, result.fallback, result.monitor
		plan.deadline = time.Now().Add(dockingPlanWait)
		state.editor, state.plan = nil, plan
		d.showDockingPlan(plan)
		return true
	}
	if result.changed > plan.matched || result.fallback != plan.fallback || (result.panelApplied && !plan.panel) {
		d.notice("Docking reply does not match its preview")
		return true
	}
	summary := fmt.Sprintf("Docking applied to %d windows. %d skipped.", result.changed, plan.windows-result.changed)
	if result.panelApplied {
		summary += " Panel assigned to " + plan.monitor + "."
	} else if plan.panel {
		summary += " Panel assignment was skipped."
	}
	d.notice(summary)
	return true
}

func (d *Controller) dockingTick(now time.Time) {
	state := &d.docking
	if pending := state.pending; pending != nil && !now.Before(pending.deadline) {
		state.pending = nil
		if pending.apply {
			d.notice("The docking result was not received. Inspect placement before trying again.")
		} else {
			d.notice("Docking preview timed out.")
		}
	}
	if plan := state.plan; plan != nil && !now.Before(plan.deadline) {
		state.plan = nil
		d.retireDockingForm()
		d.notice("Docking preview expired.")
	}
}

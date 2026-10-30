// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"errors"
	"fmt"
	"os"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
)

type sessionConfirmation struct {
	token, action, generation, owner, operation, provider string
	config                                                sessionservice.Config
	deadline                                              time.Time
}

type sessionActionView struct {
	allowed bool
	detail  string
}

// A configured endpoint is an intended owner even before the first handshake
// and after loss. Failed/ambiguous parent work must never select a native retry.
func (d *Controller) sessionOwnerExpected() bool {
	return d.session.ownerSeen || d.session.snapshot != nil || os.Getenv(sessionservice.SocketEnvironment) != ""
}

func sessionOperationTargets() []string {
	return []string{"session-status", "session-reprobe", "notification-test", "notification-close", "logout", "power-status", "power-reprobe", "poweroff", "reboot", "suspend", "lock"}
}
func sessionPowerWork(verb string) bool {
	switch verb {
	case "poweroff", "reboot", "suspend", "lock", "power-probe":
		return true
	}
	return false
}
func sessionRefusedOutcome(outcome string) bool {
	switch outcome {
	case "unavailable", "denied", "authentication-required", "inhibited", "stale", "failed", "outcome-unconfirmed", "duplicate", "secure-readiness-unconfirmed":
		return true
	}
	return false
}
func sessionOutcomeMessage(verb, outcome string) string {
	if verb == "lock" {
		return "Locker: " + shortSessionText(outcome, 32) + "; protection unconfirmed."
	}
	if verb == "suspend" {
		return "Suspend unavailable: secure readiness unconfirmed."
	}
	if verb == "power-probe" {
		return "Power provider observed; permission is per action."
	}
	return title(verb) + ": " + shortSessionText(outcome, 32) + "; completion unconfirmed."
}

// All native consumers share this policy projection. Installed portable tools
// can receive a request, but executable presence says nothing about permission.
// login1's explicit yes is the sole authorization for its power mutation.
func (d *Controller) sessionAction(action string) sessionActionView {
	if action == "suspend" {
		return sessionActionView{detail: "unavailable: secure readiness unconfirmed"}
	}
	s := d.session.snapshot
	if s == nil {
		return sessionActionView{detail: "unavailable: session owner unobserved"}
	}
	if action == "lock" {
		switch s.Locker.State {
		case "starting", "running":
			return sessionActionView{detail: s.Locker.State + "; protection unconfirmed"}
		case "installed", "exited":
			return sessionActionView{allowed: true, detail: "request-capable; protection unconfirmed"}
		default:
			return sessionActionView{detail: s.Locker.State + "; protection unconfirmed"}
		}
	}
	if s.Config.Power != d.requestedSessionConfig().Power || d.session.configure {
		return sessionActionView{detail: "unavailable: policy handshake pending"}
	}
	if s.Power.State != "ready" {
		return sessionActionView{detail: s.Power.State + "; refresh provider"}
	}
	for _, c := range s.Power.Capabilities {
		if c.Action != action {
			continue
		}
		if s.Power.Provider == "portable" && c.Permission == "unknown" {
			return sessionActionView{allowed: true, detail: "authorization unknown; request-capable"}
		}
		if c.Permission == "yes" {
			return sessionActionView{allowed: true, detail: "available"}
		}
		switch c.Permission {
		case "no":
			return sessionActionView{detail: "denied"}
		case "challenge":
			return sessionActionView{detail: "authentication required"}
		case "inhibited", "inhibitor-blocked", "challenge-inhibitor-blocked":
			return sessionActionView{detail: "inhibited (" + c.Permission + ")"}
		case "na":
			return sessionActionView{detail: "unavailable"}
		default:
			return sessionActionView{detail: "unknown; request refused"}
		}
	}
	return sessionActionView{detail: "unknown; request refused"}
}

func (d *Controller) sessionProviderRows() []string {
	provider := "unavailable"
	if s := d.session.snapshot; s != nil {
		provider = s.Power.Provider
	}
	rows := []string{fmt.Sprintf("Power: saved %v; live %s", d.saved["session_power"], provider)}
	for _, action := range []string{"poweroff", "reboot", "suspend", "lock"} {
		rows = append(rows, title(action)+": "+d.sessionAction(action).detail)
	}
	return rows
}
func (d *Controller) sessionControlsIdentity() string {
	text := ""
	for _, action := range []string{"lock", "poweroff", "reboot"} {
		text += fmt.Sprintf("%s:%t;", action, d.sessionAction(action).allowed)
	}
	return text
}
func (d *Controller) sessionMenu() desktopsettings.Menu {
	rows := []desktopsettings.Row{{"Session and services...", "SendToModule FvwmDesktop session-panel"}, {"Session settings...", "SendToModule FvwmDesktop edit-settings group:session"}}
	for _, action := range []string{"lock", "poweroff", "reboot", "suspend"} {
		view := d.sessionAction(action)
		command := "Greyed"
		if d.sessionOwnerExpected() {
			if view.allowed {
				command = "SendToModule FvwmDesktop confirm " + action
				if action == "lock" {
					command = "SendToModule FvwmDesktop lock"
				}
			}
		} else if action != "suspend" {
			view.detail = "native request; permission unknown"
			if action == "lock" {
				if found("xlock") != "" || found("slock") != "" {
					command = "SendToModule FvwmDesktop lock"
				}
			} else if d.workerAvailable() {
				command = "SendToModule FvwmDesktop confirm " + action
			}
		}
		rows = append(rows, desktopsettings.Row{sessionPanelText(title(action) + ": " + view.detail), command})
	}
	rows = append(rows, desktopsettings.Row{"Probe power provider", "SendToModule FvwmDesktop session-power-probe"}, desktopsettings.Row{"Log out...", "SendToModule FvwmDesktop confirm logout"}, desktopsettings.Row{"Restart FVWM", "Restart"})
	return desktopsettings.Menu{Name: "SlateSession", Title: "Session", Rows: rows}
}

func publicSessionProviders(s sessionservice.Snapshot) (sessionservice.PowerObservation, sessionservice.LockerObservation) {
	p, l := s.Power, s.Locker
	p.Capabilities = append([]sessionservice.PowerCapability(nil), p.Capabilities...)
	// Omit transport identities and bound text before JSON's worst-case escaping.
	// The provider generation still binds the observed source without exposing
	// another public mutation token or growing the unchanged 8 KiB reply cap.
	p.BusID, p.ServerOwner = "", ""
	p.Detail, l.Detail = shortSessionText(p.Detail, 48), shortSessionText(l.Detail, 48)
	for i := range p.Capabilities {
		p.Capabilities[i].Detail = shortSessionText(p.Capabilities[i].Detail, 48)
	}
	return p, l
}

func (d *Controller) dismissSessionConfirmation() {
	if d.session.confirmation != nil {
		d.session.confirmation = nil
		d.emit("KillModule FvwmForm SlateSessionForm")
	}
}
func (d *Controller) confirmSessionAction(action string) error {
	if action == "logout" {
		d.dismissSessionConfirmation()
		d.form("SlateSessionForm", []string{`Title "Session"`, "Line left", `Text "Confirm logout? Save your work first."`, "Line expand", `Button quit "Cancel" ^[`, "Command Nop", `Button quit "Logout"`, "Command SendToModule FvwmDesktop session logout"}, false)
		return d.fatal
	}
	if action != "poweroff" && action != "reboot" {
		return errors.New("Unknown session confirmation")
	}
	if d.sessionOwnerExpected() && !d.sessionAction(action).allowed {
		return errors.New("Power capability unavailable; refresh Session and services")
	}
	d.dismissSessionConfirmation()
	c := &sessionConfirmation{token: newID(16), action: action, generation: d.generation, config: d.requestedSessionConfig(), deadline: time.Now().Add(time.Minute)}
	if s := d.session.snapshot; s != nil {
		c.owner, c.operation, c.provider = s.Generation, s.Operation, s.Power.Generation
	}
	d.session.confirmation = c
	detail := d.sessionAction(action).detail
	if !d.sessionOwnerExpected() {
		detail = "Native fallback; authorization and completion unknown."
	}
	prefix := "SendToModule FvwmDesktop session-confirm " + d.instance + " " + c.token + " "
	d.form("SlateSessionForm", []string{`Title "Session"`, "Line left", "Text " + quote("Confirm "+action+"? Save your work first."), "Line left", "Text " + quote(detail), "Line expand", `Button quit "Cancel" ^[`, "Command " + prefix + "cancel", "Button quit " + quote(title(action)), "Command " + prefix + "submit", `Timeout 600 Nop ""`}, false)
	return d.fatal
}
func (d *Controller) sessionConfirmationControl(argument string) error {
	words, err := wordCount(argument, 3)
	if err != nil {
		return err
	}
	if words[0] != d.instance {
		return nil
	}
	c := d.session.confirmation
	if c == nil || c.token != words[1] {
		return errors.New("Session confirmation is stale; reopen it")
	}
	d.dismissSessionConfirmation()
	if words[2] == "cancel" {
		return d.fatal
	}
	if words[2] != "submit" {
		return errors.New("Unknown confirmation action")
	}
	stale := c.generation != d.generation || c.config != d.requestedSessionConfig() || !time.Now().Before(c.deadline)
	if s := d.session.snapshot; s != nil {
		stale = stale || c.owner != s.Generation || c.operation != s.Operation || c.provider != s.Power.Generation || !d.sessionAction(c.action).allowed
	} else {
		stale = stale || c.owner != "" || d.sessionOwnerExpected()
	}
	if stale {
		return errors.New("Session confirmation is stale; refresh the provider")
	}
	d.power(c.action)
	return d.fatal
}

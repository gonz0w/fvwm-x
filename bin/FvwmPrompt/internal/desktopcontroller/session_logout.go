// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

const logoutClientLimit = 512
const logoutPageLimit = 128

type logoutClient struct {
	Target    string `json:"target"`
	CanDelete bool   `json:"can_delete"`
	WMDelete  bool   `json:"wm_delete"`
	Title     string `json:"title"`
}
type logoutPage struct {
	State      string         `json:"state"`
	Scope      string         `json:"scope"`
	Coverage   string         `json:"coverage"`
	Generation string         `json:"generation"`
	Clients    []logoutClient `json:"clients"`
	Offset     int            `json:"offset"`
	Next       *int           `json:"next_offset"`
	Total      int            `json:"total"`
	Truncated  bool           `json:"truncated"`
	XSMP       bool           `json:"xsmp"`
}
type sessionLogout struct {
	cookie                     string
	serial                     uint64
	pending                    *previewPending
	deadline, nextPoll         time.Time
	phase, message, generation string
	initial, observed          []logoutClient
	total, index               int
	xsmp, handoff              bool
	record                     *operation
}

func (d *Controller) logoutActive() bool {
	l := d.session.logout
	return l != nil && (l.phase == "collecting" || l.phase == "deleting" || l.phase == "waiting" || l.phase == "committing" || l.phase == "delegating")
}
func (d *Controller) startLogout(record *operation) error {
	if !d.ready || d.closed {
		return errors.New("Desktop is not ready")
	}
	if d.logoutActive() {
		return errors.New("Logout is already pending; use Cancel to stop further requests")
	}
	if old := d.session.logout; old != nil && old.pending != nil {
		return errors.New("Previous logout request is awaiting acknowledgement; no new logout was started")
	}
	l := &sessionLogout{cookie: newID(16), deadline: time.Now().Add(30 * time.Second), phase: "collecting", message: "Collecting all applications before requesting any closes.", record: record}
	d.session.logout = l
	if d.session.view == nil {
		d.session.view = &sessionView{token: newID(16), deadline: time.Now().Add(5 * time.Minute)}
	}
	d.logoutQuery(0)
	d.showSession()
	return d.fatal
}
func (d *Controller) logoutSend(phase, command string) {
	l := d.session.logout
	if l == nil || !d.logoutActive() || l.pending != nil {
		return
	}
	l.serial++
	serial := fmt.Sprintf("%016x", l.serial)
	l.pending = &previewPending{phase: phase, serial: serial, deadline: time.Now().Add(5 * time.Second)}
	d.emit(fmt.Sprintf(command, l.cookie, serial))
}
func (d *Controller) logoutQuery(offset int) {
	l := d.session.logout
	if l == nil {
		return
	}
	gen := l.generation
	if offset == 0 {
		gen = "-"
		l.generation = ""
		l.observed = nil
		l.total = 0
	}
	d.logoutSend("query", fmt.Sprintf("Send_Query %%s %%s session-clients %d %d %s", offset, logoutPageLimit, gen))
}
func (d *Controller) logoutAction(op, target, generation string) {
	d.logoutSend(op, "Send_Session %s %s "+op+" "+target+" "+generation)
}
func (d *Controller) finishLogout(state, message string) {
	l := d.session.logout
	if l == nil {
		return
	}
	l.phase, l.message, l.pending = state, message, nil
	if l.record != nil && l.record.finished.IsZero() {
		l.record.application = "cooperative-requests-only"
		l.record.result = values{"state": state, "logout_completion": "unconfirmed", "closed_applications_restorable": false}
		terminal := "failed"
		if state == "cancelled" {
			terminal = "cancelled"
		} else if state == "requested" || state == "delegated" {
			terminal = "completed"
		}
		if terminal == "failed" {
			l.record.err = "logout_not_completed"
		}
		d.operationChanged(l.record, terminal)
	}
	d.showSession()
}
func (d *Controller) cancelLogout() error {
	l := d.session.logout
	if l == nil || !d.logoutActive() {
		return errors.New("No cooperative logout is pending")
	}
	if l.handoff {
		return errors.New("Final logout request already submitted; cancellation is unavailable here")
	}
	d.stopLogout("cancelled", "Cancelled; no further requests. An already sent close may still complete.")
	return d.fatal
}

// Validate an entire page before retaining it. Never silently use a truncated
// inventory or a mixed generation to decide which application may be closed.
func (l *sessionLogout) addPage(raw values) (bool, error) {
	if !exact(raw, "state", "scope", "coverage", "generation", "clients", "offset", "next_offset", "total", "truncated", "xsmp") {
		return false, errors.New("Invalid application snapshot")
	}
	array, ok := raw["clients"].([]any)
	if !ok || len(array) > logoutPageLimit {
		return false, errors.New("Invalid application list")
	}
	for _, v := range array {
		r, ok := v.(map[string]any)
		if !ok || !exact(r, "target", "can_delete", "wm_delete", "title") {
			return false, errors.New("Invalid application record")
		}
		if _, ok := r["can_delete"].(bool); !ok {
			return false, errors.New("Invalid Delete capability")
		}
		if _, ok := r["wm_delete"].(bool); !ok {
			return false, errors.New("Invalid WM_DELETE capability")
		}
	}
	if _, ok := raw["xsmp"].(bool); !ok {
		return false, errors.New("Invalid session ownership")
	}
	if _, ok := raw["truncated"].(bool); !ok {
		return false, errors.New("Invalid snapshot completeness")
	}
	for _, key := range []string{"offset", "total"} {
		n, ok := raw[key].(json.Number)
		if !ok {
			return false, errors.New("Invalid application page count")
		}
		value, err := strconv.Atoi(string(n))
		if err != nil || value < 0 || value > logoutClientLimit {
			return false, errors.New("Invalid application page count")
		}
	}
	if raw["next_offset"] != nil {
		n, ok := raw["next_offset"].(json.Number)
		if !ok {
			return false, errors.New("Invalid application continuation")
		}
		if _, err := strconv.Atoi(string(n)); err != nil {
			return false, errors.New("Invalid application continuation")
		}
	}
	page, err := resultAs[logoutPage](raw)
	if err != nil || page.State != "observed" || page.Scope != "session-clients" || page.Coverage != "managed-applications-excluding-active-module-classes-and-dock-desktop" || !identity(page.Generation, 48) || page.Offset != len(l.observed) || page.Total < 0 || page.Total > logoutClientLimit || page.Truncated || len(page.Clients) > logoutPageLimit || page.Offset+len(page.Clients) > page.Total {
		return false, errors.New("Incomplete application snapshot")
	}
	if page.Offset != 0 && (page.Generation != l.generation || page.Total != l.total || page.XSMP != l.xsmp) {
		return false, errors.New("Application snapshot changed during collection")
	}
	seen := map[string]bool{}
	for _, c := range l.observed {
		seen[c.Target] = true
	}
	for _, c := range page.Clients {
		if !identity(c.Target, 48) || seen[c.Target] || !validText(c.Title, 80) {
			return false, errors.New("Invalid application allocation identity")
		}
		seen[c.Target] = true
	}
	end := page.Offset + len(page.Clients)
	if page.Next == nil && end != page.Total || page.Next != nil && (*page.Next != end || end >= page.Total || len(page.Clients) == 0) {
		return false, errors.New("Incomplete application continuation")
	}
	l.generation, l.total, l.xsmp = page.Generation, page.Total, page.XSMP
	l.observed = append(l.observed, page.Clients...)
	return page.Next == nil, nil
}

// No requests are emitted until all original applications support cooperative
// deletion. Later observations may shrink but cannot add allocation identities.
func logoutEligible(clients []logoutClient) bool {
	for _, c := range clients {
		if !c.CanDelete {
			return false
		}
	}
	return true
}
func logoutSubset(initial, current []logoutClient) bool {
	allowed := map[string]bool{}
	for _, c := range initial {
		allowed[c.Target] = true
	}
	for _, c := range current {
		if !allowed[c.Target] {
			return false
		}
	}
	return true
}
func (d *Controller) acceptLogoutReply(pending *previewPending, response values) error {
	l := d.session.logout
	raw, ok := response["result"].(map[string]any)
	if !ok || !exact(response, "result") {
		return errors.New("Core refused cooperative logout")
	}
	if pending.phase == "query" {
		complete, err := l.addPage(raw)
		if err != nil {
			return err
		}
		if !complete {
			d.logoutQuery(len(l.observed))
			return d.fatal
		}
		if l.phase == "collecting" {
			l.initial = append([]logoutClient(nil), l.observed...)
			if l.xsmp {
				l.phase = "delegating"
				l.handoff = true
				l.message = "Handing logout to external XSMP; cancellation unavailable."
				d.logoutAction("delegate-xsmp", "-", "-")
				d.showSession()
				return d.fatal
			}
			if !logoutEligible(l.initial) {
				return errors.New("An application cannot close cooperatively; no close requests were sent")
			}
			l.phase = "deleting"
			l.index = 0
			return d.logoutDeleteNext()
		}
		if l.xsmp || !logoutSubset(l.initial, l.observed) {
			return errors.New("New applications or session ownership changed; logout stopped")
		}
		if len(l.observed) == 0 {
			l.phase = "committing"
			l.handoff = true
			l.message = "Final logout submitted; cancellation is unavailable."
			d.logoutAction("quit-if-empty", "-", l.generation)
		} else {
			l.nextPoll = time.Now().Add(500 * time.Millisecond)
			l.message = fmt.Sprintf("Waiting for %d applications; no force-close or retry.", len(l.observed))
		}
		d.showSession()
		return d.fatal
	}
	if !exact(raw, "state", "operation") && !exact(raw, "state", "operation", "reason") && !exact(raw, "state", "operation", "delegated", "cancel_supported") {
		return errors.New("Invalid logout action reply")
	}
	if reason, exists := raw["reason"]; exists && !validText(reason, 256) {
		return errors.New("Invalid logout reason")
	}
	state, op := stringAt(raw, "state"), stringAt(raw, "operation")
	if op != pending.phase {
		return errors.New("Logout action identity changed")
	}
	switch op {
	case "delete":
		if state != "requested" || !exact(raw, "state", "operation") {
			return errors.New("Application close was refused or stale; logout stopped")
		}
		l.index++
		return d.logoutDeleteNext()
	case "quit-if-empty":
		if state != "requested" || !exact(raw, "state", "operation") {
			return errors.New("Desktop is no longer empty; logout stopped")
		}
		d.finishLogout("requested", "Final logout requested; core exit is not yet confirmed.")
	case "delegate-xsmp":
		if state != "requested" || !exact(raw, "state", "operation", "delegated", "cancel_supported") || raw["delegated"] != true || raw["cancel_supported"] != false {
			return errors.New("External XSMP logout was unavailable")
		}
		d.finishLogout("delegated", "External XSMP owns logout; cancellation is unavailable here.")
	default:
		return errors.New("Unknown logout action reply")
	}
	return d.fatal
}
func (d *Controller) logoutDeleteNext() error {
	l := d.session.logout
	if l.index < len(l.initial) {
		l.message = fmt.Sprintf("Requesting cooperative close %d of %d.", l.index+1, len(l.initial))
		d.logoutAction("delete", l.initial[l.index].Target, "-")
	} else {
		l.phase = "waiting"
		l.message = "Waiting for application replies; desktop remains available."
		d.logoutQuery(0)
	}
	d.showSession()
	return d.fatal
}
func (d *Controller) logoutReply(message string) bool {
	l := d.session.logout
	if l == nil || l.pending == nil {
		return false
	}
	p := l.pending
	prefix := "MFL_CORE_1 " + l.cookie + " " + p.serial + " "
	if !strings.HasPrefix(message, prefix) {
		return false
	}
	if d.logoutActive() && (!time.Now().Before(p.deadline) || !time.Now().Before(l.deadline)) {
		d.stopLogout("failed", d.logoutTimeoutMessage())
	}
	f := strings.Fields(strings.TrimPrefix(message, prefix))
	if len(f) == 3 && f[0] == "part" && f[1] == strconv.Itoa(p.parts) && len(f[2]) >= 2 && len(f[2]) <= 512 && len(f[2])%2 == 0 && identity(f[2], len(f[2])) && len(p.data)+len(f[2])/2 <= 32768 {
		data, _ := hex.DecodeString(f[2])
		p.data = append(p.data, data...)
		p.parts++
		return true
	}
	if len(f) != 2 || f[0] != "end" || f[1] != strconv.Itoa(p.parts) || p.parts == 0 {
		d.stopLogout("failed", "Invalid core reply; no further close requests sent.")
		return true
	}
	l.pending = nil
	if !d.logoutActive() {
		l.message += " Request acknowledged."
		d.showSession()
		return true
	}
	decoded, err := desktopsettings.DecodeJSON(p.data)
	response, ok := decoded.(map[string]any)
	if err == nil && ok {
		err = d.acceptLogoutReply(p, response)
	} else {
		err = errors.New("Invalid application snapshot")
	}
	if err != nil {
		d.finishLogout("failed", err.Error()+". Desktop stays running.")
	}
	return true
}
func (d *Controller) sessionTick() {
	if v := d.session.view; v != nil && !time.Now().Before(v.deadline) {
		d.closeSessionPanel()
	}
	l := d.session.logout
	if l == nil || !d.logoutActive() {
		return
	}
	now := time.Now()
	if !now.Before(l.deadline) || l.pending != nil && !now.Before(l.pending.deadline) {
		d.stopLogout("failed", d.logoutTimeoutMessage())
		return
	}
	if l.phase == "waiting" && l.pending == nil && !now.Before(l.nextPoll) {
		d.logoutQuery(0)
	}
}

func (d *Controller) stopLogout(state, message string) {
	l := d.session.logout
	if l == nil {
		return
	}
	pending := l.pending
	d.finishLogout(state, message)
	// A submitted request cannot be recalled. Retain just its bounded framing
	// record; only its acknowledgement or module restart releases new admission.
	l.pending = pending
}
func (d *Controller) logoutTimeoutMessage() string {
	if d.session.logout != nil && d.session.logout.handoff {
		return "Final request outcome unknown; no retry issued."
	}
	return "Logout timed out; no further requests. An already sent close may finish."
}

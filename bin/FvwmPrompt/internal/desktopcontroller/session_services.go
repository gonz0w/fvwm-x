// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"context"
	"errors"
	"fmt"
	"os"
	"reflect"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
)

type sessionView struct {
	token, frame string
	projection   string
	deadline     time.Time
}
type sessionWork struct {
	verb   string
	config sessionservice.Config
	record *operation
}

// The event loop owns the cached observation and one request. Workers only
// return typed bounded replies; they never publish or mutate controller state.
type sessionControl struct {
	view             *sessionView
	snapshot         *sessionservice.Snapshot
	work             *sessionWork
	configure        bool
	ownerSeen        bool
	message          string
	lastNotification uint32
	logout           *sessionLogout
	confirmation     *sessionConfirmation
}

func sessionConfig(v values) sessionservice.Config {
	bus, _ := v["session_bus"].(string)
	notifications, _ := v["session_notifications"].(string)
	dnd, _ := v["notification_dnd"].(bool)
	power, _ := v["session_power"].(string)
	return sessionservice.Config{Bus: bus, Notifications: notifications, DND: dnd, Power: power}
}
func (d *Controller) requestedSessionConfig() sessionservice.Config {
	return sessionConfig(merged(d.saved, d.overrides))
}
func onlySession(changes values) bool {
	if len(changes) == 0 {
		return false
	}
	for key := range changes {
		if !desktopsettings.SessionKey(key) {
			return false
		}
	}
	return true
}
func (d *Controller) sessionObserved() bool { return d.session.snapshot != nil }
func (d *Controller) retainActiveSession(effective values) {
	// A preference commit cannot attest to live service application. Keep the
	// last owner observation until the post-commit handshake actually succeeds.
	for _, key := range []string{"session_bus", "session_notifications", "notification_dnd", "session_power"} {
		effective[key] = d.effective[key]
	}
}
func (d *Controller) adoptSession(snapshot sessionservice.Snapshot) {
	d.session.ownerSeen = true
	snapshot.Items = append([]sessionservice.Item(nil), snapshot.Items...)
	snapshot.Power.Capabilities = append([]sessionservice.PowerCapability(nil), snapshot.Power.Capabilities...)
	old := d.session.snapshot
	if old != nil && (old.Generation != snapshot.Generation || old.Items[3].Operation != snapshot.Items[3].Operation) {
		d.session.lastNotification = 0
	}
	changed := old == nil || old.Generation != snapshot.Generation || old.Config != snapshot.Config || old.ActiveBusPolicy != snapshot.ActiveBusPolicy || !reflect.DeepEqual(old.Items, snapshot.Items) || !reflect.DeepEqual(old.Power, snapshot.Power) || old.Locker != snapshot.Locker
	if c := d.session.confirmation; c != nil && (c.owner != snapshot.Generation || c.operation != snapshot.Operation || c.provider != snapshot.Power.Generation || c.config != snapshot.Config) {
		d.dismissSessionConfirmation()
	}
	d.session.snapshot = &snapshot
	d.effective["session_bus"] = snapshot.ActiveBusPolicy
	d.effective["session_notifications"] = snapshot.Config.Notifications
	d.effective["notification_dnd"] = snapshot.Config.DND
	d.effective["session_power"] = snapshot.Config.Power
	if changed {
		d.sessionSettingsChanged(false)
	}
}
func (d *Controller) applySessionPolicy() {
	d.session.configure = true
	if os.Getenv(sessionservice.SocketEnvironment) == "" {
		d.session.snapshot = nil
		d.session.message = "Preferences saved; live session owner unavailable."
		if d.session.view != nil {
			d.showSession()
		}
		return
	}
	if d.session.work == nil {
		_ = d.startSessionRequest("status", nil)
	}
}
func (d *Controller) startSessionRequest(verb string, record *operation) error {
	if d.closed || !d.ready {
		return errors.New("Desktop is not ready")
	}
	if d.session.work != nil {
		return errors.New("A session request is pending; refresh after it finishes")
	}
	request := sessionservice.RequestMessage{Version: sessionservice.Version, Verb: verb}
	if verb == "notify-failure" {
		request.Verb = "notify"
	}
	cfg := d.requestedSessionConfig()
	if verb != "status" {
		s := d.session.snapshot
		if s == nil {
			return errors.New("Session owner unavailable; Refresh before sending a request")
		}
		request.Generation, request.Operation = s.Generation, s.Operation
		if verb == "poweroff" || verb == "reboot" || verb == "suspend" {
			request.ProviderGeneration = s.Power.Generation
		}
	}
	switch verb {
	case "poweroff", "reboot":
		if !d.sessionAction(verb).allowed {
			return errors.New("Power capability is unavailable; refresh the provider")
		}
	case "configure":
		request.Config = &cfg
	case "notify":
		if cfg.DND {
			d.session.message = "Do not disturb: notification suppressed."
			return nil
		}
		request.Summary = "FVWM-X notification test"
		request.Body = "The existing notification server received this test."
	case "notify-failure":
		request.Summary = "FVWM-X desktop service"
		request.Body = "A desktop service request failed. Open Session and services to inspect readiness."
	case "close":
		if d.session.lastNotification == 0 {
			return errors.New("No notification owned by this desktop is available to close")
		}
		request.NotificationID = d.session.lastNotification
	}
	if !d.submit("session-services", func(ctx context.Context) (any, error) {
		ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
		defer cancel()
		reply, err := sessionservice.Request(ctx, request)
		return reply, err
	}) {
		return errors.New("Desktop service is busy; no session request was sent")
	}
	d.session.work = &sessionWork{verb: verb, config: cfg, record: record}
	d.session.message = "Session request pending; readiness is unconfirmed."
	d.showSession()
	return nil
}
func (d *Controller) finishSessionRequest(result job) {
	work := d.session.work
	d.session.work = nil
	if work == nil {
		return
	}
	reply, ok := result.result.(sessionservice.Reply)
	valid := ok && reply.Version == sessionservice.Version && reply.Status.Validate() == nil
	if valid {
		d.adoptSession(reply.Status)
	}
	failed := result.err != nil || !valid || reply.Error != ""
	fallback := ""
	if failed {
		if !valid && sessionPowerWork(work.verb) {
			// A provider request failure does not attest notification loss or
			// session-bus death. Retain that independent inventory/ID lease.
			if s := d.session.snapshot; s != nil {
				if work.verb == "lock" {
					s.Locker.Detail = "Locker request outcome unconfirmed; refresh before retry."
				} else {
					s.Power.State = "outcome-unconfirmed"
					s.Power.Detail = "Provider request outcome unconfirmed; no fallback or replay."
				}
				d.sessionSettingsChanged(false)
			}
		} else if !valid {
			had := d.session.snapshot != nil
			d.session.snapshot = nil
			d.session.lastNotification = 0
			if had {
				d.sessionSettingsChanged(false)
			}
		}
		d.session.message = "Session request unavailable; desktop and applications remain running."
		if sessionPowerWork(work.verb) {
			d.session.message = "Provider request outcome unconfirmed; no fallback or replay. Refresh status."
		}
		if work.verb == "configure" {
			d.session.message = "Preferences saved; live application unavailable. Refresh to retry."
		}
		if work.verb == "notify" || work.verb == "notify-failure" {
			fallback = "Notification unavailable; this local notice is the fallback."
		}
	} else {
		switch work.verb {
		case "notify", "notify-failure":
			switch reply.Outcome {
			case "sent":
				d.session.lastNotification = reply.NotificationID
				d.session.message = "Notification sent to the existing server."
			case "suppressed":
				d.session.message = "Do not disturb: notification suppressed."
			default:
				d.session.message = "Notification unavailable; local fallback shown."
				fallback = "Notification server unavailable; this is a local notice."
			}
		case "close":
			d.session.lastNotification = 0
			d.session.message = "Own notification close request completed."
		case "poweroff", "reboot", "suspend", "lock", "power-probe":
			d.session.message = sessionOutcomeMessage(work.verb, reply.Outcome)
		case "configure":
			d.session.message = "Preferences saved; live owner observed. Bus policy applies at next login."
		default:
			d.session.message = "Session status observed; application launch does not imply readiness."
		}
	}
	if work.record != nil {
		work.record.result = values{"outcome": shortSessionText(reply.Outcome, 64)}
		if (sessionPowerWork(work.verb) || work.record.target == "power-status") && valid {
			power, locker := publicSessionProviders(reply.Status)
			work.record.result = values{"outcome": shortSessionText(reply.Outcome, 64), "power": power, "locker": locker}
		}
		work.record.application = "owner-observed"
		if failed {
			work.record.err = "session_unavailable"
			work.record.application = "unknown"
			d.operationChanged(work.record, "failed")
		} else {
			if sessionRefusedOutcome(reply.Outcome) {
				work.record.err = "session_request_refused"
				d.operationChanged(work.record, "failed")
			} else {
				d.operationChanged(work.record, "completed")
			}
		}
	}
	if work.verb == "configure" && work.config == d.requestedSessionConfig() {
		d.session.configure = false
	}
	// A setting may have changed while this one bounded request was running.
	// Configure only from a fresh owner CAS token, after the atomic file commit.
	if valid && d.session.configure && (work.verb != "configure" || work.config != d.requestedSessionConfig()) {
		d.session.configure = false
		if err := d.startSessionRequest("configure", nil); err != nil {
			d.session.message = "Preferences saved; live application unavailable. Refresh to retry."
		} else {
			// The pending panel is already reconciled. A NeverFocus fallback
			// must follow it so the panel cannot cover the new notice.
			if fallback != "" {
				d.notice(fallback)
			}
			return
		}
	}
	d.systemMenu()
	d.showSession()
	if fallback != "" {
		d.notice(fallback)
	}
}
func (d *Controller) startSessionPanel() error {
	if !d.ready || d.closed {
		return errors.New("Desktop is not ready")
	}
	d.session.view = &sessionView{token: newID(16), deadline: time.Now().Add(5 * time.Minute)}
	d.emit("Style SlateSessionPanelForm FPGrabFocus, FPReleaseFocus")
	if d.session.work == nil {
		if err := d.startSessionRequest("status", nil); err != nil {
			d.session.message = "Session owner unavailable; desktop remains usable."
		} else {
			return d.fatal // startSessionRequest published the pending view.
		}
	}
	return d.showSession()
}
func (d *Controller) closeSessionPanel() {
	d.session.view = nil
	d.emit("KillModule FvwmForm SlateSessionPanelForm")
}
func (d *Controller) showSession() error {
	v := d.session.view
	if v == nil {
		return nil
	}
	lines := []string{`Title "Session and services"`, "Colorset 2", "ItemColorset 12"}
	add := func(s string) {
		// Bound visible rows before native font measurement; Text then sizes
		// to the complete displayed prefix and explicit ellipsis. Longer
		// diagnostic context remains in the bounded status observation.
		lines = append(lines, "Line left", "Text "+quote(sessionPanelText(s)))
	}
	add("Owned services; independent apps are observed.")
	if s := d.session.snapshot; s != nil {
		names := []string{"Window manager", "Autostart scan", "Session bus", "Notifications", "Applications"}
		for i, item := range s.Items {
			row := names[i] + ": " + item.State + " / " + item.Ownership
			if item.Detail != "" {
				row += " - " + item.Detail
			}
			add(row)
		}
		add(fmt.Sprintf("Bus: saved %v; active %s (next login)", d.saved["session_bus"], s.ActiveBusPolicy))
		add(fmt.Sprintf("Notifications: saved %v; live %s", d.saved["session_notifications"], s.Config.Notifications))
		add(fmt.Sprintf("Do not disturb: saved %v; live %t", d.saved["notification_dnd"], s.Config.DND))
	} else {
		add("No session owner (direct/external login).")
		add("Saved bus: " + fmt.Sprint(d.saved["session_bus"]) + " (next login)")
		add(fmt.Sprintf("Notifications: saved %v; live unavailable", d.saved["session_notifications"]))
		add(fmt.Sprintf("Do not disturb: saved %v; live unavailable", d.saved["notification_dnd"]))
	}
	for _, row := range d.sessionProviderRows() {
		add(row)
	}
	if d.session.message != "" {
		add(d.session.message)
	}
	if l := d.session.logout; l != nil {
		add("Logout: " + l.message)
	}
	add("Cancel cannot restore already closed apps.")
	active, handoff := d.logoutActive(), false
	if d.session.logout != nil {
		handoff = d.session.logout.handoff
	}
	projection := strings.Join(lines, "\n") + fmt.Sprintf("\ncontrols:%t:%t:%t:%s", d.session.lastNotification != 0, active, handoff, d.sessionControlsIdentity())
	if v.projection == projection {
		return d.fatal
	}
	// Exactly one Form alias belongs to this view. Retire it on a changed
	// projection, and retain it across identical polling observations so
	// native focus and in-flight input cannot churn every500milliseconds.
	v.projection, v.frame = projection, newID(8)
	prefix := "SendToModule FvwmDesktop session-control " + d.instance + " " + v.token + " " + v.frame + " "
	lines = append(lines, "Line expand", `Button quit "Close" ^[`, "Command "+prefix+"close", `Button continue "Refresh" F5`, "Command "+prefix+"refresh", `Button continue "Settings" F2`, "Command "+prefix+"settings")
	lines = append(lines, "Line expand", `Button continue "Test notification" F6`, "Command "+prefix+"test")
	if d.session.lastNotification != 0 {
		lines = append(lines, `Button continue "Close own notice" F7`, "Command "+prefix+"close-notification")
	}
	lines = append(lines, "Line expand")
	if d.logoutActive() {
		if !d.session.logout.handoff {
			lines = append(lines, `Button continue "Cancel logout" F4`, "Command "+prefix+"cancel-logout")
		}
	} else {
		lines = append(lines, `Button continue "Log out..." F3`, "Command "+prefix+"logout")
	}
	lines = append(lines, "Line expand", `Button continue "Probe power" F8`, "Command "+prefix+"power-reprobe")
	for _, action := range []string{"lock", "poweroff", "reboot"} {
		if d.sessionAction(action).allowed {
			lines = append(lines, "Button continue "+quote(title(action)), "Command "+prefix+action)
		}
	}
	lines = append(lines, `Timeout 3000 `+quote(prefix+"close")+` ""`)
	d.form("SlateSessionPanelForm", lines, true)
	return d.fatal
}
func sessionPanelText(s string) string {
	runes := []rune(clean(s, 1024))
	if len(runes) > 46 {
		return string(runes[:43]) + "..."
	}
	return string(runes)
}

func shortSessionText(s string, n int) string {
	r := []rune(strings.ToValidUTF8(s, "�"))
	return string(r[:min(n, len(r))])
}
func (d *Controller) sessionPanelControl(argument string) error {
	words, err := wordCount(argument, 4)
	if err != nil {
		return err
	}
	v := d.session.view
	if words[0] != d.instance {
		return nil
	}
	if v == nil || words[1] != v.token || words[2] != v.frame || !time.Now().Before(v.deadline) {
		return errors.New("Session panel is stale; reopen it")
	}
	v.deadline = time.Now().Add(5 * time.Minute)
	switch words[3] {
	case "close":
		d.closeSessionPanel()
	case "settings":
		return d.editSettings("group:session", false, nil)
	case "refresh":
		d.session.configure = true
		if d.session.snapshot == nil {
			return d.startSessionRequest("status", nil)
		}
		return d.startSessionRequest("reprobe", nil)
	case "power-reprobe":
		return d.startSessionRequest("power-probe", nil)
	case "poweroff", "reboot":
		return d.confirmSessionAction(words[3])
	case "lock":
		d.lock()
	case "test":
		if d.requestedSessionConfig().DND {
			d.session.message = "Do not disturb: notification suppressed."
			return d.showSession()
		}
		if err := d.startSessionRequest("notify", nil); err != nil {
			d.session.message = "Notification unavailable; local fallback shown."
			d.showSession()
			d.notice("Notification unavailable; this local notice is the fallback.")
			return d.fatal
		}
	case "close-notification":
		return d.startSessionRequest("close", nil)
	case "logout":
		return d.handle("confirm", "logout")
	case "cancel-logout":
		return d.cancelLogout()
	default:
		return errors.New("Unknown session control")
	}
	return d.fatal
}

func hasSessionChange(changes values) bool {
	for key := range changes {
		if desktopsettings.SessionKey(key) {
			return true
		}
	}
	return false
}
func (d *Controller) changeSessionSettings(changes values) error {
	candidate, err := desktopsettings.Checked(merged(d.saved, changes))
	if err != nil {
		return err
	}
	effective, err := desktopsettings.Checked(merged(d.effective, changes))
	if err != nil {
		return err
	}
	// Prove the settings transaction cannot change a theme projection before
	// rebasing a live preview's generation/file revision across this commit.
	now := time.Now()
	minute := now.Hour()*60 + now.Minute()
	oldPlan, oldProjection, _, err := d.themePlan(d.effective, minute)
	if err != nil {
		return err
	}
	newPlan, newProjection, _, err := d.themePlan(effective, minute)
	if err != nil {
		return err
	}
	if !reflect.DeepEqual(oldPlan, newPlan) || !reflect.DeepEqual(oldProjection, newProjection) {
		return errors.New("Session settings changed appearance projection")
	}
	revision, err := saveSettings(d.statePath, candidate, d.revision)
	if err != nil {
		return err
	}
	d.retainActiveSession(effective)
	d.saved, d.effective, d.revision = candidate, effective, revision
	d.sessionSettingsChanged(true)
	d.applySessionPolicy()
	d.showSession()
	return d.fatal
}

func sessionTarget(target string) bool {
	switch target {
	case "session-status", "session-reprobe", "notification-test", "notification-close", "logout", "power-status", "power-reprobe", "poweroff", "reboot", "suspend", "lock":
		return true
	}
	return false
}
func (d *Controller) startSessionOperation(target string, request values) values {
	if request["generation"] != d.generation {
		return failure("stale_generation")
	}
	if target == "logout" && d.logoutActive() || target != "logout" && d.session.work != nil {
		return failure("operation_busy")
	}
	if len(d.operations) >= 16 {
		index := -1
		for i, r := range d.operations {
			if !r.finished.IsZero() {
				index = i
				break
			}
		}
		if index < 0 {
			return failure("operation_limit")
		}
		d.operations = append(d.operations[:index], d.operations[index+1:]...)
	}
	r := &operation{id: newID(16), generation: newID(16), target: target, state: "accepted", application: "not-submitted"}
	var err error
	if target == "logout" {
		err = d.startLogout(r)
	} else {
		verb := map[string]string{"session-status": "status", "session-reprobe": "reprobe", "notification-test": "notify", "notification-close": "close", "power-status": "status", "power-reprobe": "power-probe", "poweroff": "poweroff", "reboot": "reboot", "suspend": "suspend", "lock": "lock"}[target]
		if verb == "notify" && d.requestedSessionConfig().DND {
			r.result = values{"outcome": "suppressed"}
			r.application = "not-submitted"
			d.operationChanged(r, "completed")
		} else {
			err = d.startSessionRequest(verb, r)
		}
	}
	if err != nil {
		return failure("operation_unavailable")
	}
	d.operations = append(d.operations, r)
	return success(values{"owner": "FvwmDesktop", "provider": d.instance, "operation": d.operationSnapshot(r, false), "disconnect": "continue"})
}
func (d *Controller) sessionRequest(r values) values {
	op := stringAt(r, "operation")
	if op != "query" && op != "provider" {
		return failure("unsupported_operation")
	}
	keys := []string{"resource", "operation"}
	if _, exists := r["provider"]; !exists {
		return failure("invalid_request")
	}
	if generation, exists := r["generation"]; exists {
		keys = append(keys, "generation")
		s, ok := generation.(string)
		if !ok || !identity(s, 32) {
			return failure("invalid_request")
		}
	}
	if p, exists := r["provider"]; exists {
		keys = append(keys, "provider")
		if !identity(stringAt(r, "provider"), 32) {
			return failure("invalid_request")
		}
		if p != d.instance {
			return nil
		}
	}
	if !exact(r, keys...) {
		return failure("invalid_request")
	}
	if !d.ready {
		return failure("provider_not_ready")
	}
	// This finite aggregate includes logout state and separately sampled parent
	// inventory. A settings generation alone cannot attest it is unchanged.

	result := values{"owner": "FvwmDesktop", "provider": d.instance, "schema_version": 2, "generation": d.generation,
		"scope": "session-services", "state": "unavailable", "availability": "owner-unavailable", "bus_policy": "next-login",
		"saved": sessionConfig(d.saved), "requested": d.requestedSessionConfig(), "service_lifecycle": false,
		"notification_server": false, "disconnect": "continue", "operation_targets": sessionOperationTargets()}
	if s := d.session.snapshot; s != nil {
		result["state"], result["availability"] = "observed", "available"
		result["service_lifecycle"] = true
		result["session_generation"] = s.Generation
		result["active_bus_policy"] = s.ActiveBusPolicy
		active := s.Config
		active.Bus = s.ActiveBusPolicy
		result["active"] = active
		items := append([]sessionservice.Item(nil), s.Items...)
		// Private reasons allow512bytes; public projection reserves room for
		// worst-case JSON escaping within the unchanged8KiB MFL reply cap.
		for i := range items {
			items[i].Detail = shortSessionText(items[i].Detail, 48)
		}
		result["items"] = items
		result["power"], result["locker"] = publicSessionProviders(*s)
	}
	if l := d.session.logout; l != nil {
		result["logout"] = values{"state": l.phase, "message": shortSessionText(l.message, 128), "cancel_available": d.logoutActive() && !l.handoff, "deadline_ms": max(0, time.Until(l.deadline).Milliseconds()), "closed_applications_restorable": false}
	}
	return success(result)
}

// A real outbound consumer for the desktop's own failures. Local native notices
// remain the fallback; arbitrary service/provider text is not sent as markup.
func (d *Controller) failureNotice() {
	if d.requestedSessionConfig().DND {
		return
	}
	if d.session.snapshot != nil && d.session.work == nil && d.requestedSessionConfig().Notifications == "external" {
		if d.startSessionRequest("notify-failure", nil) == nil {
			return
		}
	}
	d.notice("Desktop service request failed; use Session and services to inspect readiness.")
}

// Service observations and commits do not invalidate an appearance editor or
// consume its saved preview draft. Only the explicit four-key editor retires
// on its own commit; all unrelated theme/cache state stays exactly in place.
func (d *Controller) sessionSettingsChanged(retireSessionEditor bool) {
	d.dismissSessionConfirmation()
	d.generation = newID(16)
	if e := d.editor; e != nil {
		own := false
		for _, key := range e.keys {
			own = own || desktopsettings.SessionKey(key)
		}
		if retireSessionEditor && own {
			d.retireEditor()
		} else {
			e.generation = d.generation
		}
	}
	if c := d.preview.control; c != nil {
		if c.saved != nil {
			c.saved.generation = d.generation
			c.saved.revision = d.revision
		}
		if c.editor != nil {
			c.editor.generation = d.generation
		}
	}
	d.staleInspection("Session settings changed; refresh for current policy.")
	_ = d.paletteProviders()
	if d.ready {
		d.systemMenu()
		d.announceSettings()
	}
}

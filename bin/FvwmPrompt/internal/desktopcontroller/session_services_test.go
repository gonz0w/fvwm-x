// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/json"
	"errors"
	"os"
	"reflect"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
)

func sessionSnapshot() sessionservice.Snapshot {
	s := sessionservice.Snapshot{Power: sessionservice.PowerObservation{Provider: "portable", Generation: strings.Repeat("c", 32), State: "ready", Capabilities: []sessionservice.PowerCapability{{Action: "poweroff", Permission: "unknown"}, {Action: "reboot", Permission: "unknown"}, {Action: "suspend", Permission: "unknown"}}}, Locker: sessionservice.LockerObservation{Generation: strings.Repeat("d", 32), State: "installed"}, Version: sessionservice.Version, Generation: strings.Repeat("a", 32), Operation: strings.Repeat("b", 32), Config: sessionservice.Defaults(), ActiveBusPolicy: "off"}
	for _, id := range []string{"core", "scan", "bus", "notifications", "applications"} {
		s.Items = append(s.Items, sessionservice.Item{ID: id, State: "ready", Ownership: "owned"})
	}
	return s
}
func TestSessionCommitPreservesThemeHeldValuesAndAppearancePreviewDraft(t *testing.T) {
	t.Setenv(sessionservice.SocketEnvironment, "")
	d, commands := colorFixture(t, "normal")
	activeColor(t, d, "palette", "ink")
	c := d.preview.control
	c.editor = &draft{scope: "palette", generation: d.generation, values: values{"palette": "grove"}}
	oldTheme := append([]any(nil), d.lastTheme...)
	oldPending := d.preview.pending
	*commands = nil
	if err := d.changeSettings(values{"session_bus": "start", "session_notifications": "external", "notification_dnd": true}); err != nil {
		t.Fatal(err)
	}
	if d.saved["session_bus"] != "start" || d.effective["session_bus"] != "off" || d.saved["session_notifications"] != "external" || d.effective["session_notifications"] != "off" {
		t.Fatal("saved claimed active", d.saved, d.effective)
	}
	if d.preview.control != c || d.preview.pending != oldPending || c.saved.generation != d.generation || !sameRevision(c.saved.revision, d.revision) || c.editor.values["palette"] != "grove" || !reflect.DeepEqual(d.lastTheme, oldTheme) {
		t.Fatal("service commit consumed appearance preview")
	}
	if err := d.checkResource(c); err != nil {
		t.Fatal("unrelated rebase made preview stale", err)
	}
	text := literalCommands(t, *commands)
	if strings.Contains(text, "Colorset ") || strings.Contains(text, "Send_Preview ") || strings.Contains(text, "wallpaper") {
		t.Fatal("service commit replayed theme", text)
	}
	if err := d.editSettings("all", false, nil); err != nil {
		t.Fatal(err)
	}
	e := d.editor
	if err := d.changeSettings(values{"notification_dnd": false}); err != nil {
		t.Fatal(err)
	}
	if d.editor != e || e.generation != d.generation {
		t.Fatal("service commit retired unrelated appearance editor")
	}
}
func TestSessionFailedAtomicCommitStartsNoHandshakeAndOwnerLossIsTruthful(t *testing.T) {
	t.Setenv(sessionservice.SocketEnvironment, "")
	d, _ := fixture(t)
	before := d.generation
	if err := os.WriteFile(d.statePath, []byte("{}\n"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := d.changeSettings(values{"session_notifications": "external"}); err == nil {
		t.Fatal("external revision overwritten")
	}
	if d.saved["session_notifications"] != "off" || d.generation != before || d.session.work != nil {
		t.Fatal("precommit failure started live mutation")
	}
	s := sessionSnapshot()
	d.adoptSession(s)
	d.session.work = &sessionWork{verb: "configure"}
	d.saved["session_notifications"] = "external"
	d.finishSessionRequest(job{err: errors.New("owner gone")})
	q, _ := d.query("session_notifications", nil, nil)
	item := q["session_notifications"].(map[string]any)
	if item["saved"] != "external" || item["effective"] != nil || item["application"] != "live-unavailable" || d.session.snapshot != nil {
		t.Fatal(q)
	}
}
func TestSessionObservationAndNotificationOwnerChanges(t *testing.T) {
	d, _ := fixture(t)
	s := sessionSnapshot()
	s.Config.Notifications = "external"
	s.Config.DND = true
	s.Config.Bus = "start"
	s.Items[3].Operation = strings.Repeat("c", 32)
	d.adoptSession(s)
	if d.effective["session_bus"] != "off" || d.effective["session_notifications"] != "external" || d.effective["notification_dnd"] != true {
		t.Fatal("wrong observed live policy")
	}
	d.session.lastNotification = 42
	s.Items[3].Operation = strings.Repeat("d", 32)
	d.adoptSession(s)
	if d.session.lastNotification != 0 {
		t.Fatal("notification ID crossed server ownership")
	}
	// This test begins after startup configuration, at the notification stage.
	// The no-endpoint fixture deliberately retains a pending configure intent.
	d.session.configure = false
	d.session.work = &sessionWork{verb: "notify"}
	d.finishSessionRequest(job{result: sessionservice.Reply{Version: sessionservice.Version, Outcome: "sent", NotificationID: 7, Status: s}})
	if d.session.lastNotification != 7 || !strings.Contains(d.session.message, "sent") {
		t.Fatal("notification success not distinguished from ready")
	}
}
func TestSessionPanelBoundsAndStaleCallback(t *testing.T) {
	d, commands := fixture(t)
	s := sessionSnapshot()
	for i := range s.Items {
		s.Items[i].Detail = strings.Repeat("x", 512)
	}
	d.adoptSession(s)
	d.session.view = &sessionView{token: newID(16), deadline: time.Now().Add(time.Minute)}
	_ = d.showSession()
	v := *d.session.view
	text := literalCommands(t, *commands)
	for _, part := range []string{"Session and services", "Colorset 2", "ItemColorset 12", "Refresh", "Test notification", "saved off; active off", "session-control " + d.instance + " " + v.token + " " + v.frame} {
		if !strings.Contains(text, part) {
			t.Fatal(part, text)
		}
	}
	// At font18 each of these at most23 rendered rows remains well below1000px.
	rendered := strings.Count(text, "Line ")
	if rendered > 24 {
		t.Fatal("panel grew beyond compact layout", rendered)
	}
	beforeCommands := len(*commands)
	_ = d.showSession()
	if len(*commands) != beforeCommands || d.session.view.frame != v.frame {
		t.Fatal("identical observation recreated the focused panel")
	}
	for _, message := range []string{"Refreshing session status", "Session status changed"} {
		beforeCommands = len(*commands)
		d.session.message = message
		_ = d.showSession()
		publication := (*commands)[beforeCommands:]
		if len(publication) == 0 || publication[0] != "KillModule FvwmForm SlateSessionPanelForm" ||
			publication[len(publication)-1] != "Module FvwmForm SlateSessionPanelForm" {
			t.Fatal("changed panel failed to retire its previous alias before replacement", publication)
		}
		if strings.Count(strings.Join(publication, "\n"), "\nModule FvwmForm SlateSessionPanelForm") != 1 {
			t.Fatal("one observation launched multiple session panels", publication)
		}
		before := len(d.queue)
		if err := d.sessionPanelControl(d.instance + " " + v.token + " " + v.frame + " test"); err == nil || len(d.queue) != before {
			t.Fatal("stale native callback acted")
		}
		v = *d.session.view
	}
	body := strings.Join(*commands, "\n")
	if !strings.Contains(body, `..."`) || strings.Contains(body, strings.Repeat("x", 512)) {
		t.Fatal("status reason lacked an explicit bounded ellipsis")
	}
	for _, text := range []string{strings.Repeat("x", 512), strings.Repeat("界", 128), "A short status"} {
		if len([]rune(sessionPanelText(text))) > 46 {
			t.Fatal("visible session row exceeded its character budget")
		}
	}
}
func TestSessionDNDAndPublicProviderAreFinite(t *testing.T) {
	d, commands := fixture(t)
	s := sessionSnapshot()
	d.adoptSession(s)
	d.saved["notification_dnd"] = true
	before := len(*commands)
	result := d.operationRequest(values{"resource": "operations", "operation": "start", "provider": d.instance, "generation": d.generation, "target": "notification-test"})
	if code(result) != "" || len(d.queue) != 0 || len(*commands) != before+1 {
		t.Fatal("DND emitted notification work", result, *commands)
	}
	if len(d.operations) != 1 || d.operations[0].state != "completed" || d.operations[0].result.(map[string]any)["outcome"] != "suppressed" {
		t.Fatal("DND sent work or lacked result")
	}
	public := d.sessionRequest(values{"resource": "session", "operation": "query", "provider": d.instance})
	body := public["result"].(map[string]any)
	if body["notification_server"] != false || body["state"] != "observed" {
		t.Fatal(body)
	}
	if code(d.sessionRequest(values{"resource": "session", "operation": "notify", "provider": d.instance})) != "unsupported_operation" {
		t.Fatal("untyped public lifecycle mutation")
	}
}
func TestAllNineteenSettingsHaveOneBoundedPublicSnapshot(t *testing.T) {
	// The initial executed 19-setting projection was 9447 bytes. Shared
	// envelope metadata must restore this existing cap without dropping rows.
	d, _ := fixture(t)
	d.runtimeHeld = map[string]string{"palette": "ink", "appearance": "light", "contrast": "high"}
	for key, value := range d.runtimeHeld {
		d.effective[key] = value
	}
	for key, item := range desktopsettings.Registry() {
		d.overrides[key] = item.Default
	}
	d.persistenceError = "Cannot read private settings; repair the file then reload"
	reply := d.settingsRequest(values{"operation": "query", "scope": "all"})
	bytes, err := json.Marshal(reply)
	if err != nil {
		t.Fatal(err)
	}
	settings := reply["result"].(map[string]any)["settings"].(map[string]any)
	if len(settings) != 19 {
		t.Fatal("missing typed service preferences")
	}
	if len(bytes) > 8192 {
		t.Fatalf("19-key bounded public snapshot needs reviewed reply cap: %d", len(bytes))
	}
}

func TestPublicSettingsInheritEnvelopeIdentityAndKeepStandaloneMetadata(t *testing.T) {
	d, _ := fixture(t)
	native, err := d.query("all", nil, nil)
	if err != nil {
		t.Fatal(err)
	}
	projected := publicSettings(native, "")
	for key, raw := range native {
		item := raw.(map[string]any)
		if item["owner"] != "FvwmDesktop" || item["generation"] != d.generation || item["preview"] != false {
			t.Fatal("standalone native/inspector identity missing", key, item)
		}
		want := values{}
		for field, value := range item {
			if field != "owner" && field != "generation" && field != "preview" && field != "persistence_error" {
				want[field] = value
			}
		}
		if !reflect.DeepEqual(projected[key], want) {
			t.Fatal("public projection changed per-setting metadata", key)
		}
	}
	for _, request := range []values{
		{"operation": "query", "scope": "all"},
		{"operation": "validate", "provider": d.instance, "generation": d.generation, "values": values{"motion": true}},
	} {
		reply := d.settingsRequest(request)
		result, ok := reply["result"].(map[string]any)
		if !ok || result["owner"] != "FvwmDesktop" || result["generation"] != d.generation || result["preview"] != false {
			t.Fatal("public consumer lost envelope identity", reply)
		}
		settings := result["settings"].(map[string]any)
		if len(settings) != 19 {
			t.Fatal("public projection lost a preference", settings)
		}
		for key, raw := range settings {
			item := raw.(map[string]any)
			if _, exists := item["owner"]; exists {
				t.Fatal("public item repeated envelope owner", key)
			}
			if _, exists := item["generation"]; exists {
				t.Fatal("public item repeated envelope generation", key)
			}
			if _, exists := item["preview"]; exists {
				t.Fatal("public item repeated envelope preview", key)
			}
			if _, exists := item["persistence_error"]; exists {
				t.Fatal("public item repeated empty envelope error", key)
			}
		}
	}
}

func TestPublicSettingsInheritCommonErrorAndRetainDistinctRuntimeErrors(t *testing.T) {
	d, _ := fixture(t)
	d.persistenceError = "Cannot read private settings; repair the file then reload"
	d.runtimeHeld = map[string]string{"palette": "ink"}
	d.effective["palette"] = "ink"
	native, err := d.query("all", nil, nil)
	if err != nil {
		t.Fatal(err)
	}
	for _, request := range []values{
		{"operation": "query", "scope": "all"},
		{"operation": "validate", "provider": d.instance, "generation": d.generation, "values": values{"motion": true}},
	} {
		reply := d.settingsRequest(request)
		result := reply["result"].(map[string]any)
		if result["persistence_error"] != d.persistenceError || result["preview"] != false {
			t.Fatal("missing common result metadata", reply)
		}
		settings := result["settings"].(map[string]any)
		for key, raw := range settings {
			item := raw.(map[string]any)
			if key == "palette" {
				if item["persistence_error"] != "Runtime kept; preference not saved. Reload or save this setting again." {
					t.Fatal("distinct per-key failure was lost", item)
				}
			} else if _, exists := item["persistence_error"]; exists {
				t.Fatal("common error repeated in a public row", key, item)
			}
		}
		data, _ := json.Marshal(reply)
		if strings.Count(string(data), d.persistenceError) != 1 {
			t.Fatal("common error was not encoded exactly once", string(data))
		}
	}
	if native["motion"].(map[string]any)["persistence_error"] == nil || native["palette"].(map[string]any)["persistence_error"] != "Runtime kept; preference not saved. Reload or save this setting again." {
		t.Fatal("standalone metadata changed during public projection", native)
	}
	unchanged := d.settingsRequest(values{"operation": "query", "scope": "all", "provider": d.instance, "generation": d.generation})["result"].(map[string]any)
	if unchanged["persistence_error"] != d.persistenceError || unchanged["preview"] != false {
		t.Fatal("conditional query lost common metadata", unchanged)
	}
	// Reload is permitted to recover from a common read failure. Its response
	// must inherit the post-operation error, rather than the old one.
	reloaded := d.settingsRequest(values{"operation": "reload", "provider": d.instance, "generation": d.generation})["result"].(map[string]any)
	if value, exists := reloaded["persistence_error"]; !exists || value != nil || reloaded["preview"] != false {
		t.Fatal("reload retained a stale common error", reloaded)
	}
	committed := d.settingsRequest(values{"operation": "set", "provider": d.instance, "generation": d.generation, "values": values{"motion": true}})["result"].(map[string]any)
	if value, exists := committed["persistence_error"]; !exists || value != nil || committed["preview"] != false {
		t.Fatal("mutation response lost common metadata", committed)
	}
}

func TestSessionPreviewConfirmationAfterServiceCommitStillSavesOneAppearanceKey(t *testing.T) {
	t.Setenv(sessionservice.SocketEnvironment, "")
	d, _ := colorFixture(t, "normal")
	activeColor(t, d, "appearance", "light")
	if err := d.changeSettings(values{"session_notifications": "external", "notification_dnd": true}); err != nil {
		t.Fatal(err)
	}
	if err := d.preview.action(d.preview.control.view, "save"); err != nil {
		t.Fatal(err)
	}
	contrastReply(&d.preview, colorResult("terminal", "confirmed", "kept"))
	stored, _, err := loadSettings(d.statePath)
	if err != nil || stored["appearance"] != "light" || stored["session_notifications"] != "external" || stored["notification_dnd"] != true || stored["palette"] != "slate" {
		t.Fatal("appearance confirmation reverted unrelated committed service policy", stored, err)
	}
	q, _ := d.query("session_notifications", nil, nil)
	if q["session_notifications"].(map[string]any)["effective"] != nil {
		t.Fatal("preview confirmation claimed live service application")
	}
}
func TestSessionPublicEscapedReasonsFitExistingReplyLimit(t *testing.T) {
	d, _ := fixture(t)
	s := sessionSnapshot()
	for i := range s.Items {
		s.Items[i].Detail = strings.Repeat("<", 512)
		s.Items[i].Attempts = ^uint64(0)
		s.Items[i].Started = 256
		s.Items[i].Exited = 256
		s.Items[i].Failed = 256
		s.Items[i].Operation = strings.Repeat("f", 32)
	}
	s.Power.Detail = strings.Repeat("<", 512)
	s.Power.BusID = strings.Repeat("<", 128)
	s.Power.ServerOwner = ":" + strings.Repeat("<", 254)
	for i := range s.Power.Capabilities {
		s.Power.Capabilities[i].Detail = strings.Repeat("<", 512)
	}
	s.Locker.Detail = strings.Repeat("<", 512)
	d.adoptSession(s)
	d.session.logout = &sessionLogout{phase: "waiting", message: strings.Repeat("<", 512), deadline: time.Now().Add(time.Minute)}
	data, err := json.Marshal(d.sessionRequest(values{"resource": "session", "operation": "query", "provider": d.instance}))
	if err != nil || len(data) > 8192 {
		t.Fatal("public projection exceeded fixed MFL budget", len(data), err)
	}
	if len(d.session.snapshot.Items[0].Detail) != 512 {
		t.Fatal("public projection mutated private observation")
	}
}

func TestSessionCommittedPolicyWaitsForFreshOwnerBeforeAutomaticConfigure(t *testing.T) {
	t.Setenv(sessionservice.SocketEnvironment, "")
	d, _ := fixture(t)
	if err := d.changeSettings(values{"session_notifications": "external", "notification_dnd": true}); err != nil {
		t.Fatal(err)
	}
	if !d.session.configure || d.session.work != nil {
		t.Fatal("unavailable owner consumed committed configuration intent")
	}
	s := sessionSnapshot()
	s.Operation = strings.Repeat("e", 32)
	d.session.work = &sessionWork{verb: "status"}
	d.finishSessionRequest(job{result: sessionservice.Reply{Version: sessionservice.Version, Outcome: "ready", Status: s}})
	if d.session.snapshot.Operation != s.Operation || d.session.work == nil || d.session.work.verb != "configure" || d.session.work.config != d.requestedSessionConfig() || d.jobs["session-services"] == nil {
		t.Fatal("fresh owner status did not admit latest committed policy")
	}
	// Finish the admitted operation without running its I/O closure. The test
	// observes event-loop policy; the lead's native gate owns actual RPC/CAS I/O.
	delete(d.jobs, "session-services")
	d.queue = nil
	s.Config = d.requestedSessionConfig()
	s.Operation = strings.Repeat("f", 32)
	d.finishSessionRequest(job{result: sessionservice.Reply{Version: sessionservice.Version, Outcome: "applied", Status: s}})
	if d.session.configure || d.session.work != nil || d.session.snapshot.Operation != s.Operation || d.effective["session_notifications"] != "external" || d.effective["notification_dnd"] != true {
		t.Fatal("live configuration acknowledgement failed to retire intent")
	}
}

func TestNotificationFallbackFollowsPanelReconciliation(t *testing.T) {
	for _, mode := range []string{"failed", "unavailable", "direct-admission"} {
		t.Run(mode, func(t *testing.T) {
			d, commands := fixture(t)
			d.session.configure = false
			s := sessionSnapshot()
			d.adoptSession(s)
			d.session.view = &sessionView{token: newID(16), deadline: time.Now().Add(time.Minute)}
			d.showSession()
			*commands = nil
			if mode == "direct-admission" {
				d.session.snapshot = nil
				v := d.session.view
				if err := d.sessionPanelControl(d.instance + " " + v.token + " " + v.frame + " test"); err != nil {
					t.Fatal(err)
				}
			} else {
				d.session.work = &sessionWork{verb: "notify"}
				result := job{result: sessionservice.Reply{Version: sessionservice.Version, Outcome: "unavailable", Status: s}}
				if mode == "failed" {
					result.err = errors.New("notification unavailable")
				}
				d.finishSessionRequest(result)
			}
			panel, notice := -1, -1
			for i, command := range *commands {
				if command == "Module FvwmForm SlateSessionPanelForm" {
					panel = i
				}
				if command == "Module FvwmForm SlateNoticeForm" {
					notice = i
				}
			}
			if panel < 0 || notice <= panel {
				t.Fatal("panel covered the newly emitted NeverFocus fallback", *commands)
			}
			if strings.Contains(strings.Join(*commands, "\n"), "Style SlateNoticeForm") {
				t.Fatal("fallback changed the shipped NeverFocus policy")
			}
		})
	}
}

func TestNativeNotificationPolicyProjectsCommittedSettingWithoutActivation(t *testing.T) {
	d, commands := colorFixture(t, "normal")
	*commands = nil
	if err := d.handle("notification-policy", ""); err != nil {
		t.Fatal(err)
	}
	text := literalCommands(t, *commands)
	if !strings.Contains(text, "SendToModule FvwmNotify DND 0") || strings.Contains("\n"+text, "\nModule FvwmNotify") {
		t.Fatal("query did not project bounded non-activating policy", text)
	}
	*commands = nil
	d.saved["notification_dnd"] = true
	d.announceSettings()
	text = literalCommands(t, *commands)
	if !strings.Contains(text, "SendToModule FvwmNotify DND 1") {
		t.Fatal("committed DND not projected", text)
	}
	*commands = nil
	for _, invalid := range []string{"FvwmOther", "0", "DND 0", "extra words"} {
		if err := d.handle("notification-policy", invalid); err == nil {
			t.Fatal("arbitrary destination or record admitted", invalid)
		}
	}
	if len(*commands) != 0 {
		t.Fatal("invalid projection emitted commands", *commands)
	}
	d.closed = true
	d.projectNotificationPolicy()
	if len(*commands) != 0 {
		t.Fatal("closed owner projected policy", *commands)
	}
}

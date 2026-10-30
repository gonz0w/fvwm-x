// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/json"
	"errors"
	"reflect"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
)

func observedPowerFixture(t *testing.T) (*Controller, *[]string) {
	t.Helper()
	d, commands := fixture(t)
	d.session.configure = false
	d.adoptSession(sessionSnapshot())
	return d, commands
}

func TestSessionPowerPreferenceCommitHasNoAppearanceReplay(t *testing.T) {
	t.Setenv(sessionservice.SocketEnvironment, "")
	d, commands := colorFixture(t, "normal")
	activeColor(t, d, "palette", "ink")
	c, theme := d.preview.control, append([]any(nil), d.lastTheme...)
	*commands = nil
	if err := d.changeSettings(values{"session_power": "login1"}); err != nil {
		t.Fatal(err)
	}
	if d.saved["session_power"] != "login1" || d.effective["session_power"] != "portable" || !reflect.DeepEqual(theme, d.lastTheme) || d.preview.control != c {
		t.Fatal("power preference claimed live application or changed appearance")
	}
	text := literalCommands(t, *commands)
	if strings.Contains(text, "Colorset ") || strings.Contains(text, "Send_Preview ") || strings.Contains(text, "wallpaper") {
		t.Fatal("power preference replayed appearance", text)
	}
}

func TestSessionPowerMenuAndPanelSharePerActionProjection(t *testing.T) {
	for _, permission := range []string{"yes", "no", "challenge", "na", "inhibited", "inhibitor-blocked", "challenge-inhibitor-blocked", "unknown"} {
		t.Run(permission, func(t *testing.T) {
			d, _ := observedPowerFixture(t)
			d.saved["session_power"] = "login1"
			s := sessionSnapshot()
			s.Config.Power, s.Power.Provider = "login1", "login1"
			s.Power.Capabilities[0].Permission = permission
			d.adoptSession(s)
			view := d.sessionAction("poweroff")
			if view.allowed != (permission == "yes") {
				t.Fatal("permission admitted wrong action", permission, view)
			}
			menu := d.sessionMenu()
			row := menu.Rows[3] // shared fixed order: lock, poweroff, reboot, suspend
			if !strings.Contains(row[0], sessionPanelText("Poweroff: "+view.detail)) || (row[1] != "Greyed") != view.allowed {
				t.Fatal("menu diverged from action projection", menu)
			}
			if rows := strings.Join(d.sessionProviderRows(), "\n"); !strings.Contains(rows, "Poweroff: "+view.detail) || !strings.Contains(rows, "secure readiness unconfirmed") {
				t.Fatal("panel diverged", rows)
			}
		})
	}
	d, _ := observedPowerFixture(t)
	if view := d.sessionAction("poweroff"); !view.allowed || !strings.Contains(view.detail, "authorization unknown") {
		t.Fatal("portable installation claimed authorization", view)
	}
}

func TestPowerConfirmationRefusesChangedPolicyProviderAndOwnerCAS(t *testing.T) {
	for _, change := range []string{"policy", "provider", "cas", "loss", "expiry"} {
		t.Run(change, func(t *testing.T) {
			d, commands := observedPowerFixture(t)
			if err := d.confirmSessionAction("poweroff"); err != nil {
				t.Fatal(err)
			}
			c := d.session.confirmation
			argument := d.instance + " " + c.token + " submit"
			switch change {
			case "policy":
				d.saved["session_power"] = "login1"
			case "provider":
				d.session.snapshot.Power.Generation = newID(16)
			case "cas":
				d.session.snapshot.Operation = newID(16)
			case "loss":
				d.session.snapshot = nil
			case "expiry":
				c.deadline = time.Now().Add(-time.Second)
			}
			if err := d.sessionConfirmationControl(argument); err == nil || d.session.confirmation != nil || len(d.queue) != 0 || d.jobs["power"] != nil {
				t.Fatal("stale confirmation submitted", change, err)
			}
			if !strings.Contains(strings.Join(*commands, "\n"), "KillModule FvwmForm SlateSessionForm") {
				t.Fatal("stale confirmation remained mapped")
			}
		})
	}
}

func TestConfirmedPowerAndLockUseParentWithoutNativeReplay(t *testing.T) {
	for _, action := range []string{"poweroff", "reboot", "lock"} {
		t.Run(action, func(t *testing.T) {
			d, _ := observedPowerFixture(t)
			if action == "lock" {
				d.lock()
			} else {
				if err := d.confirmSessionAction(action); err != nil {
					t.Fatal(err)
				}
				c := d.session.confirmation
				if err := d.sessionConfirmationControl(d.instance + " " + c.token + " submit"); err != nil {
					t.Fatal(err)
				}
			}
			if d.session.work == nil || d.session.work.verb != action || len(d.queue) != 1 || d.queue[0].key != "session-services" || d.jobs["power"] != nil || d.lockerPending {
				t.Fatal("parent action selected local fallback")
			}
			d.session.lastNotification = 37
			s := *d.session.snapshot
			d.finishSessionRequest(job{err: errors.New("ambiguous submission")})
			if d.session.lastNotification != 37 || d.session.snapshot == nil || !reflect.DeepEqual(s.Items, d.session.snapshot.Items) || d.session.work != nil || d.jobs["power"] != nil || d.lockerPending || len(d.queue) != 1 {
				t.Fatal("provider failure replayed or erased independent notifications")
			}
		})
	}
}

func TestOwnerEndpointWithoutHandshakeNeverChoosesDirectFallback(t *testing.T) {
	d, _ := fixture(t)
	t.Setenv(sessionservice.SocketEnvironment, "/not-an-owner")
	d.power("poweroff")
	d.lock()
	if len(d.queue) != 0 || d.jobs["power"] != nil || d.lockerPending {
		t.Fatal("unobserved intended owner silently chose fallback")
	}
}

func TestUnconfirmedLockerAndSuspendAreIndependentOfBackendPermission(t *testing.T) {
	for _, state := range []string{"installed", "starting", "running", "exited", "failed", "unavailable"} {
		d, _ := observedPowerFixture(t)
		d.session.snapshot.Locker.State = state
		for i := range d.session.snapshot.Power.Capabilities {
			d.session.snapshot.Power.Capabilities[i].Permission = "yes"
		}
		if d.sessionAction("suspend").allowed || !strings.Contains(d.sessionAction("lock").detail, "protection unconfirmed") || d.session.snapshot.Locker.SecureReady {
			t.Fatal("process state invented readiness", state)
		}
		allowed := state == "installed" || state == "exited"
		if d.sessionAction("lock").allowed != allowed {
			t.Fatal("locker duplicate/retry policy", state)
		}
		r := d.startSessionOperation("suspend", values{"generation": d.generation})
		if code(r) != "" || d.session.work == nil || d.session.work.verb != "suspend" || d.jobs["power"] != nil {
			t.Fatal("suspend bypassed typed parent refusal", state, r)
		}
	}
}

func TestProviderOperationResultRetainsBoundedTypedObservation(t *testing.T) {
	d, _ := observedPowerFixture(t)
	s := *d.session.snapshot
	s.Power.Detail, s.Locker.Detail = strings.Repeat("<", 512), strings.Repeat("<", 512)
	for i := range s.Power.Capabilities {
		s.Power.Capabilities[i].Detail = strings.Repeat("<", 512)
	}
	r := &operation{id: newID(16), generation: newID(16), target: "poweroff"}
	d.session.work = &sessionWork{verb: "poweroff", record: r}
	d.finishSessionRequest(job{result: sessionservice.Reply{Version: sessionservice.Version, Status: s, Outcome: "denied"}})
	data, err := json.Marshal(d.operationSnapshot(r, false))
	if err != nil || len(data) > 8192 || r.state != "failed" || d.session.snapshot.Locker.SecureReady {
		t.Fatal("typed denial exceeded reply budget or claimed secure readiness", len(data), r, err)
	}
	result := r.result.(map[string]any)
	if result["power"] == nil || result["locker"] == nil || result["outcome"] != "denied" {
		t.Fatal("provider observation missing from retained result", result)
	}
}

func TestNotificationFailureKeepsIndependentPowerProjection(t *testing.T) {
	d, _ := observedPowerFixture(t)
	before := d.session.snapshot.Power
	s := *d.session.snapshot
	s.Items = append([]sessionservice.Item(nil), s.Items...)
	s.Items[3].State = "failed"
	s.Items[3].Detail = "notification provider failed"
	d.session.work = &sessionWork{verb: "notify"}
	d.finishSessionRequest(job{result: sessionservice.Reply{Version: sessionservice.Version, Status: s, Outcome: "unavailable", Error: "notification failure"}})
	if d.session.snapshot == nil || !reflect.DeepEqual(d.session.snapshot.Power, before) || !d.sessionAction("poweroff").allowed {
		t.Fatal("notification failure disabled an independent power provider")
	}
}

func TestPublicPowerCallerGenerationRejectsChangedProvider(t *testing.T) {
	d, _ := observedPowerFixture(t)
	generation := d.generation
	s := *d.session.snapshot
	s.Power.Generation = newID(16)
	d.adoptSession(s)
	result := d.startSessionOperation("poweroff", values{"generation": generation})
	if code(result) != "stale_generation" || len(d.queue) != 0 {
		t.Fatal("old public caller silently acquired a new power provider", result)
	}
}

func TestObservedOwnerLossCannotSelectDirectNativeFallback(t *testing.T) {
	t.Setenv(sessionservice.SocketEnvironment, "")
	d, _ := observedPowerFixture(t)
	d.session.work = &sessionWork{verb: "status"}
	d.finishSessionRequest(job{err: errors.New("owner lost")})
	d.power("poweroff")
	d.lock()
	if d.session.snapshot != nil || !d.sessionOwnerExpected() || d.jobs["power"] != nil || d.lockerPending || len(d.queue) != 0 {
		t.Fatal("observed owner loss selected direct native fallback")
	}
}

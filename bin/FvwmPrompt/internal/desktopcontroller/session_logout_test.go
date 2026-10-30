// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/hex"
	"encoding/json"
	"fmt"
	"strings"
	"testing"
	"time"
)

func logoutPageFixture(offset, total int, next any, targets ...string) values {
	clients := []any{}
	for _, target := range targets {
		clients = append(clients, values{"target": target, "can_delete": true, "wm_delete": true, "title": "Application"})
	}
	return values{"state": "observed", "scope": "session-clients", "coverage": "managed-applications-excluding-active-module-classes-and-dock-desktop", "generation": strings.Repeat("a", 48), "offset": json.Number(fmt.Sprint(offset)), "next_offset": next, "total": json.Number(fmt.Sprint(total)), "truncated": false, "xsmp": false, "clients": clients}
}
func logoutResponse(t *testing.T, d *Controller, response values) {
	t.Helper()
	l := d.session.logout
	if l == nil || l.pending == nil {
		t.Fatal("missing pending core request")
	}
	serial := l.pending.serial
	data, err := json.Marshal(response)
	if err != nil {
		t.Fatal(err)
	}
	parts := 0
	for offset := 0; offset < len(data); offset += 256 {
		if !d.logoutReply(fmt.Sprintf("MFL_CORE_1 %s %s part %d %s", l.cookie, serial, parts, hex.EncodeToString(data[offset:min(offset+256, len(data))]))) {
			t.Fatal("reply not routed")
		}
		parts++
	}
	if !d.logoutReply(fmt.Sprintf("MFL_CORE_1 %s %s end %d", l.cookie, serial, parts)) {
		t.Fatal("reply completion not routed")
	}
}
func TestLogoutCollectsCompleteSnapshotBeforeAnyDeleteAndNeverRetries(t *testing.T) {
	d, commands := fixture(t)
	a, b := strings.Repeat("b", 48), strings.Repeat("c", 48)
	if err := d.startLogout(nil); err != nil {
		t.Fatal(err)
	}
	logoutResponse(t, d, success(logoutPageFixture(0, 2, json.Number("1"), a)))
	if strings.Contains(literalCommands(t, *commands), "Send_Session") {
		t.Fatal("Delete before complete collection")
	}
	logoutResponse(t, d, success(logoutPageFixture(1, 2, nil, b)))
	if !strings.Contains(literalCommands(t, *commands), " delete "+a+" -") {
		t.Fatal("initial allocation not closed")
	}
	logoutResponse(t, d, success(values{"state": "requested", "operation": "delete"}))
	logoutResponse(t, d, success(values{"state": "requested", "operation": "delete"}))
	logoutResponse(t, d, success(logoutPageFixture(0, 1, nil, b)))
	l := d.session.logout
	if l.phase != "waiting" || strings.Count(literalCommands(t, *commands), " delete ") != 2 {
		t.Fatal("not waiting or retried")
	}
	l.nextPoll = time.Now().Add(-time.Second)
	d.sessionTick()
	logoutResponse(t, d, success(logoutPageFixture(0, 0, nil)))
	if !l.handoff || !strings.Contains(literalCommands(t, *commands), " quit-if-empty - "+l.generation) {
		t.Fatal("unguarded Quit")
	}
	if err := d.cancelLogout(); err == nil {
		t.Fatal("cancellation claimed after final request")
	}
	logoutResponse(t, d, success(values{"state": "requested", "operation": "quit-if-empty"}))
	if l.phase != "requested" || strings.Contains(l.message, "completed") {
		t.Fatal("requested exit claimed complete", l)
	}
	if strings.Contains(literalCommands(t, *commands), "Kill ") || strings.Contains(literalCommands(t, *commands), "Destroy ") {
		t.Fatal("forced application closure")
	}
}
func TestLogoutRefusesUncooperativeMalformedAndNewApplications(t *testing.T) {
	a, b := strings.Repeat("b", 48), strings.Repeat("c", 48)
	for _, kind := range []string{"refusing", "truncated", "missingpage", "duplicated", "nulltotal", "nullxsmp", "overlimit", "unknownfield"} {
		t.Run(kind, func(t *testing.T) {
			d, commands := fixture(t)
			_ = d.startLogout(nil)
			raw := logoutPageFixture(0, 1, nil, a)
			switch kind {
			case "refusing":
				raw["clients"].([]any)[0].(map[string]any)["can_delete"] = false
			case "truncated":
				raw["truncated"] = true
			case "missingpage":
				raw["total"] = json.Number("2")
			case "duplicated":
				raw = logoutPageFixture(0, 2, nil, a, a)
			case "nulltotal":
				raw["total"] = nil
			case "nullxsmp":
				raw["xsmp"] = nil
			case "overlimit":
				raw["total"] = json.Number("513")
			case "unknownfield":
				raw["extra"] = true
			}
			logoutResponse(t, d, success(raw))
			if d.session.logout.phase != "failed" || strings.Contains(literalCommands(t, *commands), "Send_Session") {
				t.Fatal("refused inventory sent close", kind)
			}
		})
	}
	d, commands := fixture(t)
	_ = d.startLogout(nil)
	logoutResponse(t, d, success(logoutPageFixture(0, 1, nil, a)))
	logoutResponse(t, d, success(values{"state": "requested", "operation": "delete"}))
	logoutResponse(t, d, success(logoutPageFixture(0, 1, nil, b)))
	if d.session.logout.phase != "failed" || strings.Contains(literalCommands(t, *commands), "quit-if-empty") {
		t.Fatal("new allocation did not block logout")
	}
}
func TestLogoutCancelAndDeadlineRetainInflightBarrier(t *testing.T) {
	for _, deadline := range []bool{false, true} {
		t.Run(fmt.Sprint(deadline), func(t *testing.T) {
			d, commands := fixture(t)
			a := strings.Repeat("b", 48)
			_ = d.startLogout(nil)
			logoutResponse(t, d, success(logoutPageFixture(0, 1, nil, a)))
			l := d.session.logout
			before := len(*commands)
			if deadline {
				l.deadline = time.Now().Add(-time.Second)
				d.sessionTick()
			} else {
				if err := d.cancelLogout(); err != nil {
					t.Fatal(err)
				}
			}
			if d.logoutActive() || l.pending == nil {
				t.Fatal("in-flight close barrier lost")
			}
			if err := d.startLogout(nil); err == nil {
				t.Fatal("second logout admitted before old acknowledgement")
			}
			logoutResponse(t, d, success(values{"state": "requested", "operation": "delete"}))
			if l.pending != nil || strings.Contains(literalCommands(t, (*commands)[before:]), "Send_Session") || strings.Contains(literalCommands(t, (*commands)[before:]), "Send_Query") {
				t.Fatal("cancel/deadline sent more requests")
			}
			if err := d.startLogout(nil); err != nil {
				t.Fatal("acknowledged canceled operation retained admission", err)
			}
		})
	}
}
func TestLogoutXSMPAndStaleFinalCheckRemainTruthful(t *testing.T) {
	d, commands := fixture(t)
	_ = d.startLogout(nil)
	raw := logoutPageFixture(0, 0, nil)
	raw["xsmp"] = true
	logoutResponse(t, d, success(raw))
	if !d.session.logout.handoff || !strings.Contains(literalCommands(t, *commands), "delegate-xsmp - -") {
		t.Fatal("external XSMP not delegated")
	}
	if err := d.cancelLogout(); err == nil {
		t.Fatal("external manager cancellation promised")
	}
	logoutResponse(t, d, success(values{"state": "requested", "operation": "delegate-xsmp", "delegated": true, "cancel_supported": false}))
	if d.session.logout.phase != "delegated" {
		t.Fatal("external handoff not recorded")
	}
	d, commands = fixture(t)
	_ = d.startLogout(nil)
	logoutResponse(t, d, success(logoutPageFixture(0, 0, nil)))
	logoutResponse(t, d, success(logoutPageFixture(0, 0, nil)))
	logoutResponse(t, d, success(values{"state": "stale", "operation": "quit-if-empty", "reason": "stale_generation"}))
	if d.session.logout.phase != "failed" || strings.Contains(literalCommands(t, *commands), "Command Quit") {
		t.Fatal("stale final check claimed success")
	}
}
func TestLogoutPagedGenerationAndTypedOperationCancellation(t *testing.T) {
	d, _ := fixture(t)
	result := d.operationRequest(values{"resource": "operations", "operation": "start", "provider": d.instance, "generation": d.generation, "target": "logout"})
	if code(result) != "" || len(d.operations) != 1 {
		t.Fatal(result)
	}
	r := d.operations[0]
	canceled := d.operationRequest(values{"resource": "operations", "operation": "cancel", "provider": d.instance, "generation": r.generation, "target": r.id})
	if code(canceled) != "" || r.state != "cancelled" || !r.cancelRequested {
		t.Fatal(canceled, r)
	}
	l := &sessionLogout{}
	if complete, err := l.addPage(logoutPageFixture(0, 2, json.Number("1"), strings.Repeat("a", 48))); err != nil || complete {
		t.Fatal(complete, err)
	}
	changed := logoutPageFixture(1, 2, nil, strings.Repeat("b", 48))
	changed["generation"] = strings.Repeat("c", 48)
	if _, err := l.addPage(changed); err == nil {
		t.Fatal("mixed-generation pages admitted")
	}
}

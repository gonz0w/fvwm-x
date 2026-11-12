// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
package desktopcontroller

import (
	"encoding/base64"
	"encoding/hex"
	"fmt"
	"strings"
	"testing"
	"time"
)

func dockField(text string) string { return "x" + base64.RawURLEncoding.EncodeToString([]byte(text)) }

func dockTestAction(d *Controller, action string) error {
	return d.handle("dock-control", d.docking.token+" "+action)
}

func dockTestReply(t *testing.T, d *Controller, payload string) {
	t.Helper()
	pending := d.docking.pending
	if pending == nil {
		t.Fatal("missing docking request")
	}
	prefix := fmt.Sprintf("MFL_CORE_1 %s %016x ", d.docking.cookie, pending.serial)
	if !d.dockingReply(prefix+"part 0 "+hex.EncodeToString([]byte(payload))) ||
		!d.dockingReply(prefix+"end 1") {
		t.Fatal("directed docking reply was not consumed")
	}
}

const dockPreviewResult = `{"result":{"matched":1,"skipped":0,"fallback":true,"monitor":"DUMMY0"}}`

func TestDockingEditorAndGuardedWire(t *testing.T) {
	d, commands := fixture(t)
	d.docking.panelAdapter = true
	if err := d.handle("dock", "office"); err != nil {
		t.Fatal(err)
	}
	draftToken := d.docking.token
	if err := dockTestAction(d, "add "+strings.Join([]string{
		dockField("term"), dockField("Literal; ${class}"), dockField("-25"), dockField("17"),
	}, " ")); err != nil {
		t.Fatal(err)
	}
	if err := dockTestAction(d, "save "+dockField("DP-2")+" "+dockField("0")+" on"); err != nil {
		t.Fatal(err)
	}
	if d.docking.token == draftToken {
		t.Fatal("save reused the prior editor callback")
	}
	stored, revision, err := loadDockingProfile(d.dockingDirectory(), "office")
	if err != nil || revision == nil || !stored.Panel || stored.Desk == nil || *stored.Desk != 0 ||
		len(stored.Windows) != 1 || stored.Windows[0].Class != "Literal; ${class}" {
		t.Fatalf("wrong saved profile: %#v, %v", stored, err)
	}
	if err := d.handle("dock-control", draftToken+" remove"); err != nil {
		t.Fatal("old form callback was not ignored", err)
	}
	if len(d.docking.editor.profile.Windows) != 1 {
		t.Fatal("old form callback changed the new editor")
	}

	before := len(*commands)
	if err := dockTestAction(d, "preview"); err != nil {
		t.Fatal(err)
	}
	cookie, serial := d.docking.cookie, d.docking.pending.serial
	prefix := fmt.Sprintf("Send_Dock %s %016x ", cookie, serial)
	want := []string{
		prefix + "begin 44502d32 0 1 1",
		prefix + "record 7465726d 4c69746572616c3b20247b636c6173737d -25 17",
		prefix + "prepare",
	}
	if got := (*commands)[before:]; strings.Join(got, "\n") != strings.Join(want, "\n") {
		t.Fatalf("prepare commands = %q", got)
	}
	if d.dockingReply("MFL_CORE_1 " + strings.Repeat("f", 32) + " 0000000000000001 end 1") {
		t.Fatal("wrong cookie accepted")
	}
	if err := dockTestAction(d, "apply"); err == nil {
		t.Fatal("apply succeeded before receiving a preview")
	}
	dockTestReply(t, d, dockPreviewResult)
	if d.docking.plan == nil || d.docking.plan.monitor != "DUMMY0" || !d.docking.plan.fallback {
		t.Fatalf("wrong prepared plan: %#v", d.docking.plan)
	}
	before = len(*commands)
	if err := dockTestAction(d, "apply"); err != nil {
		t.Fatal(err)
	}
	wantApply := fmt.Sprintf("Send_Dock %s %016x apply %016x", cookie, serial+1, serial)
	if got := strings.Join((*commands)[before:], "\n"); !strings.Contains(got, wantApply) {
		t.Fatalf("missing apply command in %s", got)
	}
	dockTestReply(t, d, `{"result":{"applied":1,"skipped":0,"fallback":true,"panel_applied":true}}`)
	if d.docking.pending != nil || d.docking.plan != nil {
		t.Fatal("apply did not consume the plan")
	}
}

func TestDockingRefusesChangedRevisionsAndTimeout(t *testing.T) {
	for _, scenario := range []string{"profile", "controller", "external settings", "expired plan", "expired request"} {
		t.Run(scenario, func(t *testing.T) {
			d, commands := fixture(t)
			profile := dockingProfile{Name: "office", Connector: "DP-2",
				Windows: []dockingWindow{{Instance: "term", Class: "XTerm"}}}
			revision, err := saveDockingProfile(d.dockingDirectory(), profile, nil)
			if err != nil {
				t.Fatal(err)
			}
			if err := d.startDocking(profile.Name); err != nil {
				t.Fatal(err)
			}
			if err := dockTestAction(d, "preview"); err != nil {
				t.Fatal(err)
			}
			if scenario == "expired request" {
				d.dockingTick(d.docking.pending.deadline)
				if d.docking.pending != nil || d.docking.plan != nil {
					t.Fatal("timed-out request remained active")
				}
				return
			}
			dockTestReply(t, d, dockPreviewResult)
			switch scenario {
			case "profile":
				profile.Connector = "HDMI-1"
				if _, err := saveDockingProfile(d.dockingDirectory(), profile, revision); err != nil {
					t.Fatal(err)
				}
			case "controller":
				d.generation = newID(16)
			case "external settings":
				if _, err := saveSettings(d.statePath, d.saved, d.revision); err != nil {
					t.Fatal(err)
				}
			case "expired plan":
				d.docking.plan.deadline = time.Now().Add(-time.Second)
			}
			before := len(*commands)
			if err := dockTestAction(d, "apply"); err == nil {
				t.Fatal("changed docking preview was applied")
			}
			for _, command := range (*commands)[before:] {
				if strings.HasPrefix(command, "Send_Dock ") {
					t.Fatalf("stale plan dispatched: %s", command)
				}
			}
		})
	}
}

func TestDockingPanelRequiresActiveAdapter(t *testing.T) {
	d, commands := fixture(t)
	profile := dockingProfile{Name: "office", Connector: "DP-2", Panel: true}
	if _, err := saveDockingProfile(d.dockingDirectory(), profile, nil); err != nil {
		t.Fatal(err)
	}
	if err := d.startDocking(profile.Name); err != nil {
		t.Fatal(err)
	}
	before := len(*commands)
	if err := dockTestAction(d, "preview"); err == nil || len(*commands) != before {
		t.Fatal("unsupported panel plan was sent")
	}
}

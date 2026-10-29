// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"strings"
	"testing"
)

func TestStartupActivityRefreshCoalescesAndCloseIgnoresLateCompletion(t *testing.T) {
	d, commands := fixture(t)
	if err := d.handle("launch-activity", ""); err != nil || d.startupActivity == nil || d.jobs["startups"] == nil {
		t.Fatal("activity did not use bounded worker", err)
	}
	view := d.startupActivity
	d.queue = nil
	d.jobs["startups"].running = true
	if err := d.handle("startup-control", view.frame+" refresh"); err != nil || len(d.jobs) != 1 || !view.pending {
		t.Fatal("Refresh duplicated query workers", err)
	}
	if err := d.handle("startup-control", view.token+" close"); err != nil || d.startupActivity != nil {
		t.Fatal(err)
	}
	count := len(*commands)
	d.finishStartupActivity(job{key: "startups"})
	if len(*commands) != count || d.startupActivity != nil {
		t.Fatal("late result reopened closed activity")
	}
	for _, command := range *commands {
		if strings.Contains(command, "Periodic") || strings.Contains(command, "Schedule ") {
			t.Fatal("activity created a resident sampler", command)
		}
	}
}

func TestStartupActivityCloseOnlyRetiresAnOwnedView(t *testing.T) {
	d, commands := fixture(t)
	before := len(*commands)
	d.closeStartupActivity()
	if len(*commands) != before {
		t.Fatal("closing an absent activity view emitted commands")
	}
	if err := d.startStartupActivity(); err != nil {
		t.Fatal(err)
	}
	before = len(*commands)
	d.closeStartupActivity()
	if d.startupActivity != nil || d.jobs["startups"] != nil || d.startupWork != "" || len(*commands) != before+1 || (*commands)[before] != "KillModule FvwmForm SlateLaunchActivityForm" {
		t.Fatal("owned activity view or queued query was not retired exactly once")
	}
	before = len(*commands)
	d.closeStartupActivity()
	d.Close()
	if len(*commands) != before {
		t.Fatal("repeated close or idle controller shutdown emitted commands")
	}
}

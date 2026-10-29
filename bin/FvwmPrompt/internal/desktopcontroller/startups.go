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
	"strconv"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopworker"
)

type startupActivity struct {
	token, frame, status string
	page                 int
	result               *desktopworker.StartupSnapshot
	pending              bool
}

func (d *Controller) startStartupActivity() error {
	if _, busy := d.jobs["startups"]; !busy && len(d.jobs) >= 4 {
		return errors.New("Desktop service is busy; try Launch activity again")
	}
	if !d.startupFocusConfigured {
		d.emit("Style SlateLaunchActivityForm FPGrabFocus, FPReleaseFocus")
		d.startupFocusConfigured = true
	}
	d.startupActivity = &startupActivity{token: newID(16)}
	return d.refreshStartupActivity()
}

func (d *Controller) closeStartupActivity() {
	owned := d.startupActivity != nil
	d.startupActivity = nil
	if d.cancelQueued("startups") {
		d.startupWork = ""
	}
	if owned {
		d.emit("KillModule FvwmForm SlateLaunchActivityForm")
	}
}

func (d *Controller) refreshStartupActivity() error {
	view := d.startupActivity
	if view == nil {
		return nil
	}
	view.status = "Collecting a read-only startup activity snapshot."
	if _, busy := d.jobs["startups"]; busy {
		view.pending = true
		return d.showStartupActivity()
	}
	if !d.workerJob("startups", "startups", nil) {
		view.status = "Service busy; previous snapshot retained. Press F5 to retry."
		return d.showStartupActivity()
	}
	view.pending = false
	d.startupWork = view.token
	return d.showStartupActivity()
}

func (d *Controller) finishStartupActivity(completed job) {
	owner := d.startupWork
	d.startupWork = ""
	view := d.startupActivity
	if view == nil || owner == "" {
		return
	}
	if owner == view.token {
		err := completed.err
		var result desktopworker.StartupSnapshot
		if err == nil {
			data, marshalErr := json.Marshal(completed.result)
			err = marshalErr
			if err == nil {
				result, err = desktopworker.DecodeStartupSnapshot(data)
			}
		}
		if err != nil {
			view.status = "Refresh failed; previous snapshot is stale. Press F5 to retry."
		} else {
			view.result = &result
			view.page = min(view.page, max(0, len(result.Startups)-1))
			view.status = "Point-in-time observation; launch submitted does not mean success."
		}
	}
	if view.pending {
		_ = d.refreshStartupActivity()
	} else if owner == view.token {
		_ = d.showStartupActivity()
	}
}

func (d *Controller) startupControl(argument string) error {
	words := splitFields(argument)
	if len(words) < 2 {
		return errors.New("Invalid Launch activity callback")
	}
	view := d.startupActivity
	if words[1] == "close" {
		if view == nil || words[0] != view.token {
			return nil
		}
		if len(words) != 2 {
			return errors.New("Invalid Launch activity close")
		}
		d.closeStartupActivity()
		return d.fatal
	}
	if view == nil || words[0] != view.frame {
		return nil
	}
	switch words[1] {
	case "refresh":
		if len(words) != 2 {
			return errors.New("Invalid Launch activity refresh")
		}
		return d.refreshStartupActivity()
	case "page":
		if len(words) != 3 || view.result == nil {
			return errors.New("Invalid Launch activity page")
		}
		page, err := strconv.Atoi(words[2])
		if err != nil || page < 0 || page >= len(view.result.Startups) {
			return errors.New("Launch activity page is out of range")
		}
		view.page = page
		return d.showStartupActivity()
	}
	return errors.New("Unknown Launch activity control")
}

func startupDisplay(value string, limit int) string {
	runes := []rune(value)
	if len(runes) > limit {
		return string(runes[:limit-1]) + "…"
	}
	return value
}

func (d *Controller) showStartupActivity() error {
	view := d.startupActivity
	if view == nil {
		return nil
	}
	view.frame = newID(16)
	callback := "Command SendToModule FvwmDesktop startup-control " + view.frame + " "
	lines := []string{`Title "Launch activity"`, "Colorset 2", "ItemColorset 12",
		"CloseCommand SendToModule FvwmDesktop startup-control " + view.token + " close"}
	lines = diagnosticText(lines, "Read-only snapshot. Protocol ended is not application success.")
	lines = diagnosticText(lines, view.status)
	pages := 0
	if view.result == nil {
		lines = diagnosticText(lines, "No activity snapshot collected yet.")
	} else {
		result := view.result
		pages = len(result.Startups)
		lines = diagnosticText(lines, fmt.Sprintf("Tracking: %d/64; dropped %d; malformed %d; partial expired %d",
			pages, result.Counters.Dropped, result.Counters.Malformed, result.Counters.PartialExpired))
		if pages == 0 {
			lines = diagnosticText(lines, "No retained startup activity.")
		} else {
			record := result.Startups[view.page]
			lines = diagnosticText(lines, fmt.Sprintf("%d/%d - %s", view.page+1, pages, record.State))
			lines = diagnosticText(lines, "Application: "+startupDisplay(record.Name, 80))
			lines = diagnosticText(lines, "ID: "+startupDisplay(record.ID, 96))
			lines = diagnosticText(lines, "Desktop entry: "+startupDisplay(record.ApplicationID, 60))
			lines = diagnosticText(lines, "Window class: "+startupDisplay(record.WMClass, 60))
			lines = diagnosticText(lines, fmt.Sprintf("Observed window: 0x%x; age at snapshot: %d ms", record.Window, record.AgeMS))
		}
	}
	lines = append(lines, "Line expand", `Button quit "Close" ^[`, "Command SendToModule FvwmDesktop startup-control "+view.token+" close")
	for _, step := range []struct {
		label, key string
		delta      int
	}{{"Previous (F1)", "F1", -1}, {"Next (F2)", "F2", 1}} {
		page := view.page + step.delta
		if page >= 0 && page < pages {
			lines = append(lines, "Button continue "+quote(step.label)+" "+step.key, callback+"page "+strconv.Itoa(page))
		}
	}
	lines = append(lines, `Button continue "Refresh (F5)" F5`, callback+"refresh")
	d.form("SlateLaunchActivityForm", lines, true)
	return d.fatal
}

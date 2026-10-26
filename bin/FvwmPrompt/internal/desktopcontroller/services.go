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
	"math"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopcheck"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopworker"
)

// POSIX word quoting only. Environment/module terminal commands never perform
// shell substitution; explicit Run remains the single intentional shell path.
func shellWords(text string) ([]string, error) {
	var words []string
	var word strings.Builder
	quote := rune(0)
	escaped, started := false, false
	for _, c := range text {
		if c == 0 {
			return nil, errors.New("NUL in terminal command")
		}
		if escaped {
			if quote == '"' && c != '"' && c != '\\' {
				word.WriteRune('\\')
			}
			word.WriteRune(c)
			escaped = false
			started = true
			continue
		}
		if c == '\\' && quote != '\'' {
			escaped = true
			started = true
			continue
		}
		if quote != 0 {
			if c == quote {
				quote = 0
			} else {
				word.WriteRune(c)
			}
			continue
		}
		if c == '\'' || c == '"' {
			quote = c
			started = true
			continue
		}
		if strings.ContainsRune(" \t\r\n", c) {
			if started {
				words = append(words, word.String())
				word.Reset()
				started = false
			}
			continue
		}
		word.WriteRune(c)
		started = true
	}
	if escaped || quote != 0 {
		return nil, errors.New("Incomplete terminal command quoting")
	}
	if started {
		words = append(words, word.String())
	}
	return words, nil
}
func detectTerminal(text string) []string {
	if text != "" {
		words, err := shellWords(text)
		if err == nil && len(words) > 0 && found(words[0]) != "" {
			return words
		}
		return nil
	}
	for _, name := range []string{"xterm", "uxterm", "rxvt"} {
		if path := found(name); path != "" {
			return []string{path}
		}
	}
	return nil
}

// startDetached admits a launched application without adopting its lifetime.
// Slot release belongs to exactly one waiter; the event loop owns notices.
func (d *Controller) startDetached(argv []string, cwd string) (*exec.Cmd, error) {
	if len(argv) == 0 {
		return nil, errors.New("Missing application command")
	}
	select {
	case d.spawnSlots <- struct{}{}:
	default:
		return nil, errors.New("Too many desktop-launched applications are still running.")
	}
	command := exec.Command(argv[0], argv[1:]...)
	command.Dir = cwd
	command.SysProcAttr = &syscall.SysProcAttr{Setsid: true}
	if err := command.Start(); err != nil {
		<-d.spawnSlots
		return nil, err
	}
	return command, nil
}
func (d *Controller) spawn(argv []string, cwd string) {
	command, err := d.startDetached(argv, cwd)
	if err != nil {
		d.notice("Could not start program: " + err.Error())
		return
	}
	slots := d.spawnSlots
	go func() { _ = command.Wait(); <-slots }()
}
func (d *Controller) lock() {
	if d.lockerPending {
		d.notice("A lock request is already pending; protection is unconfirmed.")
		return
	}
	path := found("xlock")
	if path == "" {
		path = found("slock")
	}
	if path == "" {
		d.notice("No secure X11 locker is available. The screen has NOT been locked.")
		return
	}
	command, err := d.startDetached([]string{path}, "")
	if err != nil {
		d.notice("Could not start locker; the screen has NOT been locked: " + err.Error())
		return
	}
	d.lockerPending = true
	result, slots := d.lockerResult, d.spawnSlots
	// One admitted locker has one buffered completion. It survives WM/module
	// retirement like other detached applications; no fixed delay claims ready.
	go func() { err := command.Wait(); <-slots; result <- err }()
}
func (d *Controller) pollLocker() {
	select {
	case err := <-d.lockerResult:
		d.lockerPending = false
		if err != nil {
			d.notice("Locker exited unsuccessfully; protection is unconfirmed.")
		}
	default:
	}
}
func (d *Controller) power(action string) {
	if _, pending := d.jobs["power"]; pending {
		d.notice("A power request is already pending; no additional request was sent.")
		return
	}
	if !d.workerJob("power", "power", values{"system": d.system, "action": action}) {
		d.notice("Desktop service is busy; no power request was sent. Try again.")
	}
}
func (d *Controller) term(argv []string, cwd string) {
	if len(d.terminal) == 0 {
		d.notice("No terminal found. Set FVWM_TERMINAL to your terminal command.")
		return
	}
	command := append([]string{}, d.terminal...)
	if len(argv) > 0 {
		command = append(command, "-e")
		command = append(command, argv...)
	}
	d.spawn(command, cwd)
}
func (d *Controller) openDirectory(path string) {
	if !d.workerJob("files", "files", values{"path": path}) {
		return
	}
	d.pendingDirectory = path
}
func (d *Controller) startCheck() error {
	if _, ok := d.jobs["configuration"]; ok || len(d.jobs) >= 4 {
		return errors.New("Desktop service is busy; try the configuration check again")
	}
	request, err := d.literalSourceRequest()
	if err != nil {
		return err
	}
	d.workerJob("configuration", "check", values{"check": request})
	return nil
}

// Check and inspector use the same literal source root and Read search scope.
func (d *Controller) literalSourceRequest() (desktopcheck.Request, error) {
	cwd, err := os.Getwd()
	if err != nil {
		return desktopcheck.Request{}, err
	}
	return desktopcheck.Request{Path: filepath.Join(filepath.Dir(d.statePath), "local.config"), Userdir: filepath.Dir(d.statePath), Datadir: filepath.Dir(d.root), Cwd: cwd}, nil
}
func (d *Controller) screenshot() {
	if !d.png {
		d.notice("Screenshot capture is unavailable: PNG support is disabled.")
		return
	}
	helper := d.capture
	home, _ := os.UserHomeDir()
	directory := filepath.Join(home, "Pictures", "Screenshots")
	d.submit("screenshot", func(ctx context.Context) (any, error) {
		data, err := child(ctx, []string{helper, directory}, nil, 4096, 30*time.Second)
		return strings.TrimSpace(string(data)), err
	})
}
func checkedApps(value any) ([]desktopworker.Application, error) {
	records, ok := value.([]any)
	if !ok || len(records) > 512 {
		return nil, errors.New("Invalid desktop application result")
	}
	for _, raw := range records {
		r, ok := raw.(map[string]any)
		if !ok || !exact(r, "name", "argv", "cwd", "terminal") {
			return nil, errors.New("Invalid desktop application record")
		}
		if !validText(r["name"], 65536) {
			return nil, errors.New("Invalid application name")
		}
		if _, ok := r["terminal"].(bool); !ok {
			return nil, errors.New("Invalid terminal flag")
		}
		if r["cwd"] != nil && !validText(r["cwd"], 4096) {
			return nil, errors.New("Invalid application directory")
		}
		argv, ok := r["argv"].([]any)
		if !ok || len(argv) == 0 || len(argv) > 4096 {
			return nil, errors.New("Invalid application arguments")
		}
		total := 0
		for i, arg := range argv {
			if !validText(arg, 65536) || i == 0 && arg == "" {
				return nil, errors.New("Invalid application argument")
			}
			total += len(arg.(string))
		}
		if total > 65536 {
			return nil, errors.New("Application arguments exceed limit")
		}
	}
	return resultAs[[]desktopworker.Application](value)
}
func validText(value any, limit int) bool {
	text, ok := value.(string)
	return ok && len(text) <= limit && !strings.ContainsRune(text, 0)
}
func checkedStatus(value any) (desktopworker.Status, error) {
	r, ok := value.(map[string]any)
	if !ok || !exact(r, "cpu", "memory", "network", "battery") || !validText(r["network"], 4096) {
		return desktopworker.Status{}, errors.New("Invalid desktop status")
	}
	v, err := resultAs[desktopworker.Status](value)
	if err != nil {
		return v, err
	}
	if v.CPU != nil && (v.CPU[0] < 0 || v.CPU[1] < 0 || v.CPU[1] > v.CPU[0]) || v.Memory != nil && *v.Memory < 0 || v.Battery != nil && (*v.Battery < 0 || *v.Battery > 100) {
		return v, errors.New("Invalid desktop status counters")
	}
	return v, nil
}
func checkedReport(value any) (desktopcheck.Result, error) {
	v, err := resultAs[desktopcheck.Result](value)
	if err != nil {
		return v, err
	}
	r, ok := value.(map[string]any)
	_, filesArray := r["files"].([]any)
	_, diagnosticsArray := r["diagnostics"].([]any)
	if !ok || !filesArray || !diagnosticsArray || !exact(r, "schema_version", "coverage", "status", "errors", "unchecked", "checked_directives", "files", "diagnostics") || v.SchemaVersion != 1 || v.Coverage != "desktop-settings-literals-and-Read-v1" || len(v.Files) > 64 || len(v.Diagnostics) > 64 || v.CheckedDirectives < 0 || v.CheckedDirectives > 10000 {
		return v, errors.New("Invalid configuration check report")
	}
	errorsCount, unchecked, total := 0, 0, 0
	location := func(loc desktopcheck.Source) bool {
		return validText(loc.Path, 8192) && loc.Line >= 1 && loc.EndLine >= loc.Line && loc.EndLine <= 10001
	}
	for _, f := range v.Files {
		if !validText(f.Path, 8192) || f.Bytes < 0 || f.Bytes > 4194304 || !identity(f.SHA256, 64) {
			return v, errors.New("Invalid configuration source")
		}
		total += f.Bytes
	}
	if total > 4194304 {
		return v, errors.New("Invalid configuration source byte count")
	}
	for _, item := range v.Diagnostics {
		if !location(item.Source) || len(item.IncludedFrom) > 8 || !validText(item.Message, 1024) || !validText(item.Code, 64) || (item.Parser != "FvwmDesktop" && item.Parser != "configuration-source") {
			return v, errors.New("Invalid configuration diagnostic")
		}
		for _, parent := range item.IncludedFrom {
			if !location(parent) {
				return v, errors.New("Invalid configuration include location")
			}
		}
		switch item.Severity {
		case "error":
			errorsCount++
		case "unchecked":
			unchecked++
		default:
			return v, errors.New("Invalid diagnostic severity")
		}
	}
	status := "checked-subset"
	if unchecked > 0 {
		status = "partial"
	}
	if errorsCount > 0 {
		status = "errors"
	}
	if v.Errors != errorsCount || v.Unchecked != unchecked || v.Status != status {
		return v, errors.New("Invalid configuration check summary")
	}
	return v, nil
}
func (d *Controller) publishJob(result job) error {
	switch result.key {
	case "apps":
		apps, err := checkedApps(result.result)
		if err != nil {
			return err
		}
		d.apps = apps
		d.applicationMenu(nil)
	case "status":
		status, err := checkedStatus(result.result)
		if err != nil {
			return err
		}
		d.status = status
		cpu := "--"
		if status.CPU != nil && d.lastCPU != nil {
			total, idle := status.CPU[0]-d.lastCPU[0], status.CPU[1]-d.lastCPU[1]
			if total > 0 && idle >= 0 {
				cpu = fmt.Sprintf("%.0f%%", math.RoundToEven(100*float64(max(0, min(total, total-idle)))/float64(total)))
			}
		}
		d.lastCPU = status.CPU
		memory := "--"
		if status.Memory != nil {
			memory = fmt.Sprintf("%.1fG", float64(*status.Memory)/1073741824)
		}
		text := "CPU " + cpu + "  RAM " + memory
		if status.Battery != nil && d.width >= 1600 {
			text += "  BAT " + strconv.Itoa(*status.Battery) + "%"
		}
		d.button("status", text)
		d.systemMenu()
	case "files":
		records, err := resultAs[[]desktopworker.FileMenuEntry](result.result)
		if err != nil || len(records) > 100 {
			return errors.New("Invalid file menu result")
		}
		files := []fileEntry{}
		rows := []desktopsettings.Row{{"Up one folder", "SendToModule FvwmDesktop files-up"}, {"Home", "SendToModule FvwmDesktop files"}, {"Open terminal here", "SendToModule FvwmDesktop files-terminal"}, {"", "Nop"}}
		for i, r := range records {
			if !validText(r.Path, 4096) || !validText(r.Name, 4096) {
				return errors.New("Invalid file menu entry")
			}
			files = append(files, fileEntry{r.Path, r.Name, r.Directory})
			label := r.Name
			if r.Directory {
				label = "> " + label
			}
			rows = append(rows, desktopsettings.Row{label, fmt.Sprintf("SendToModule FvwmDesktop file %d", i)})
		}
		d.files, d.directory = files, d.pendingDirectory
		d.menu(desktopsettings.Menu{Name: "SlateFiles", Title: d.directory, Rows: rows})
		d.emit("Menu SlateFiles Monitor c c")
	case "configuration":
		reportValue, err := checkedReport(result.result)
		if err != nil {
			return err
		}
		d.report = &report{newID(16), reportValue}
		return d.showReport(d.report.token, 0)
	case "screenshot", "volume", "power":
		text, ok := result.result.(string)
		if !ok || !validText(text, 4096) {
			return errors.New("Invalid desktop service response")
		}
		if text == "" && result.key == "power" {
			return errors.New("Invalid power service response")
		}
		if text == "" {
			text = "Sound control is unavailable or permission was denied."
		}
		d.notice(text)
	}
	return d.fatal
}
func (d *Controller) pollJobs() {
	d.expireOperations()
	for {
		select {
		case result := <-d.completed:
			d.active--
			delete(d.jobs, result.key)
			if result.key == "inspection" {
				d.finishInspection(result)
				continue
			}
			record := d.operationJobs[result.key]
			delete(d.operationJobs, result.key)
			if record != nil && record.cancelRequested {
				d.operationChanged(record, "cancelled")
				continue
			}
			err := result.err
			publication := err == nil
			if err == nil {
				err = d.publishJob(result)
			}
			if record != nil {
				if err != nil {
					record.err = "worker_failed"
					record.application = "not-submitted"
					if publication {
						record.err = "publication_failed"
						record.application = "partial-or-unknown"
					}
					d.operationChanged(record, "failed")
				} else {
					if result.key == "apps" {
						record.result = values{"entries": len(d.apps)}
					} else {
						record.result = values{"status_collected": true}
					}
					record.application = "submitted-to-fvwm"
					d.operationChanged(record, "completed")
				}
			} else if err != nil {
				d.notice(err.Error())
			}
		default:
			for key, j := range d.jobs {
				if r := d.operationJobs[key]; r != nil && j.running && r.state != "running" {
					d.operationChanged(r, "running")
				}
			}
			for _, r := range d.operations {
				d.announceOperation(r)
			}
			d.startJobs()
			return
		}
	}
}

func CheckConfiguration(ctx context.Context, helper string, request desktopcheck.Request) (desktopcheck.Result, error) {
	value, err := worker(ctx, helper, "check", values{"check": request})
	if err != nil {
		return desktopcheck.Result{}, err
	}
	return checkedReport(value)
}

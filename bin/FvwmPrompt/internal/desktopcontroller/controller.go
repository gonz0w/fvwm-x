// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package desktopcontroller owns the resident desktop module. Only the event
// loop mutates its state or publishes commands. Workers return bounded data.
package desktopcontroller

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopcheck"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopworker"
)

type editor struct {
	token, scope, generation string
	keys                     []string
	deadline                 time.Time
}
type draft struct {
	scope, generation string
	values            values
}
type report struct {
	token  string
	result desktopcheck.Result
}
type fileEntry struct {
	path, name string
	directory  bool
}
type Controller struct {
	root, statePath, worker, capture, system string
	png                                      bool
	send                                     func(string) error
	fatal                                    error
	saved, effective, overrides              values
	revision                                 *string
	generation, instance, persistenceError   string
	ready                                    bool
	runtimeContrast                          bool
	editor                                   *editor
	preview                                  preview
	palette                                  *paletteView
	inspection                               *inspection
	inspectionJob                            *inspectionWork
	inspectionFocusConfigured                bool
	startupActivity                          *startupActivity
	startupWork                              string
	startupFocusConfigured                   bool
	lastThemeColors                          map[string]string
	lastThemeAppearance                      string
	report                                   *report
	palettes                                 map[string]map[string]map[string]string
	lastTheme                                []any
	lastWallpaper                            [2]string
	apps                                     []desktopworker.Application
	statusLabel                              string
	appGeneration                            string
	appQuery                                 *string
	status                                   desktopworker.Status
	lastCPU                                  *[2]int64
	desk, width                              int
	clock, directory, pendingDirectory       string
	terminal                                 []string
	files                                    []fileEntry
	ctx                                      context.Context
	cancel                                   context.CancelFunc
	jobs                                     map[string]*job
	queue                                    []*job
	active                                   int
	completed                                chan job
	operations                               []*operation
	operationJobs                            map[string]*operation
	spawnSlots                               chan struct{}
	browser                                  browserHandoff
	settingsBinding                          *menuBinding
	appBinding                               *menuBinding
	systemBinding                            *menuBinding
	closed                                   bool
	lockerPending                            bool
	lockerResult                             chan error
}

func New(root, statePath, system, worker, capture string, png bool, send func(string) error) (*Controller, error) {
	ctx, cancel := context.WithCancel(context.Background())
	d := &Controller{root: root, statePath: statePath, system: system, worker: worker, capture: capture, png: png, send: send,
		instance: newID(16), generation: newID(16), overrides: values{}, width: 1440,
		ctx: ctx, cancel: cancel, jobs: map[string]*job{}, completed: make(chan job, 4),
		operationJobs: map[string]*operation{}, spawnSlots: make(chan struct{}, 256), browser: newBrowserHandoff(), lockerResult: make(chan error, 1)}
	d.settingsBinding = newMenuBinding("settings")
	d.appBinding = newMenuBinding("intent")
	d.systemBinding = newMenuBinding("intent")
	d.preview.desktop = d
	file, err := os.Open(filepath.Join(root, "palettes.json"))
	if err != nil {
		cancel()
		return nil, err
	}
	data, err := io.ReadAll(io.LimitReader(file, 65537))
	file.Close()
	if err != nil || len(data) > 65536 {
		cancel()
		return nil, errors.New("Invalid palette data")
	}
	if err = json.Unmarshal(data, &d.palettes); err != nil {
		cancel()
		return nil, err
	}
	d.saved, d.revision, err = loadSettings(statePath)
	if err != nil {
		d.saved, d.revision = desktopsettings.Defaults(), nil
		d.persistenceError = "Cannot read private settings; repair the file then reload"
		fmt.Fprintln(os.Stderr, "FvwmDesktop: "+d.persistenceError)
	}
	d.effective, _ = desktopsettings.Checked(d.saved)
	d.status.Network = "Checking..."
	d.terminal = detectTerminal(os.Getenv("FVWM_TERMINAL"))
	return d, nil
}
func (d *Controller) Close() {
	d.closePalette()
	d.closeStartupActivity()
	d.closed = true
	d.browser.menuToken = ""
	d.settingsBinding.policy.Close()
	d.appBinding.policy.Close()
	d.systemBinding.policy.Close()
	d.cancel()
	deadline := time.NewTimer(2 * time.Second)
	defer deadline.Stop()
	for d.active > 0 {
		select {
		case <-d.completed:
			d.active--
		case <-deadline.C:
			return
		}
	}
}
func (d *Controller) emit(command string) {
	if d.fatal == nil {
		d.fatal = d.send(command)
	}
}
func merged(base, changes values) values {
	result := values{}
	for k, v := range base {
		result[k] = v
	}
	for k, v := range changes {
		result[k] = v
	}
	return result
}
func (d *Controller) snapshot(saved, effective values) desktopsettings.Snapshot {
	if saved == nil {
		saved = d.saved
	}
	if effective == nil {
		effective = d.effective
	}
	var persistence *string
	if d.persistenceError != "" {
		text := d.persistenceError
		persistence = &text
	}
	return desktopsettings.Snapshot{Saved: saved, Effective: effective, Overrides: d.overrides,
		Generation: d.generation, Instance: d.instance, PersistenceError: persistence, Submitted: d.lastTheme != nil,
		RuntimeContrast: d.runtimeContrast}
}
func (d *Controller) retireEditor() {
	if d.editor != nil {
		d.editor = nil
		d.emit("KillModule FvwmForm SlateSettingsForm")
	}
}
func (d *Controller) changed(generation string) {
	d.generation = generation
	d.retireEditor()
	d.staleInspection("Settings changed; refresh to collect the current generation.")
	_ = d.paletteProviders()
	if d.ready {
		d.announceSettings()
	}
}
func (d *Controller) announceSettings() {
	d.emit("SendToModule FvwmMFL MFL_PROVIDER_1 " + d.instance + " " + d.generation)
}
func (d *Controller) configure(key, text string) error {
	value, err := desktopsettings.Value(key, text, true)
	if err != nil {
		return err
	}
	d.overrides[key] = value
	return nil
}
func (d *Controller) finishConfiguration() error {
	d.runtimeContrast = false
	d.emit("KillModule FvwmForm SlateSettingsForm")
	d.emit("KillModule FvwmForm SlateConfigCheckForm")
	d.closeInspection()
	d.closeStartupActivity()
	d.closePalette()
	var err error
	d.effective, err = desktopsettings.Checked(merged(d.saved, d.overrides))
	if err != nil {
		fmt.Fprintln(os.Stderr, "FvwmDesktop: "+err.Error()+"; ignoring schedule overrides")
		delete(d.overrides, "day")
		delete(d.overrides, "night")
		d.effective, err = desktopsettings.Checked(merged(d.saved, d.overrides))
		if err != nil {
			return err
		}
	}
	d.changed(newID(16))
	d.ready = true
	d.preview.constrain()
	d.announceSettings()
	return d.fatal
}
func (d *Controller) prepare(changes values) (values, values, error) {
	now := time.Now()
	result, err := desktopsettings.Prepare(d.snapshot(nil, nil), changes, now.Hour()*60+now.Minute())
	if err != nil {
		return nil, nil, err
	}
	candidate := result["candidate"].(map[string]any)
	effective := result["effective"].(map[string]any)
	if d.runtimeContrast {
		if _, explicit := changes["contrast"]; !explicit {
			effective["contrast"] = "high"
		}
	}
	if _, _, _, err := d.themePlan(effective, now.Hour()*60+now.Minute()); err != nil {
		return nil, nil, err
	}
	return candidate, effective, nil
}
func (d *Controller) changeSettings(changes values) error {
	if d.persistenceError != "" {
		return errors.New(d.persistenceError)
	}
	candidate, effective, err := d.prepare(changes)
	if err != nil {
		return err
	}
	generation := newID(16)
	revision, err := saveSettings(d.statePath, candidate, d.revision)
	if err != nil {
		return err
	}
	d.saved, d.effective, d.revision = candidate, effective, revision
	if _, explicit := changes["contrast"]; explicit {
		d.runtimeContrast = false
	}
	d.changed(generation)
	d.preview.constrain()
	return d.theme(true)
}
func (d *Controller) reloadSettings() error {
	saved, revision, err := loadSettings(d.statePath)
	if err != nil {
		return err
	}
	effective, err := desktopsettings.Checked(merged(saved, d.overrides))
	if err != nil {
		return err
	}
	now := time.Now()
	if _, _, _, err := d.themePlan(effective, now.Hour()*60+now.Minute()); err != nil {
		return err
	}
	generation := newID(16)
	d.saved, d.effective, d.revision = saved, effective, revision
	d.runtimeContrast = false
	d.persistenceError = ""
	d.closePalette()
	d.resetWorkerMenus()
	d.changed(generation)
	d.preview.constrain()
	return d.theme(true)
}
func (d *Controller) resetSettings(scope string) error {
	keys, err := desktopsettings.Scope(scope)
	if err != nil {
		return err
	}
	changes := values{}
	defaults := desktopsettings.Defaults()
	for _, key := range keys {
		changes[key] = defaults[key]
	}
	return d.changeSettings(changes)
}
func (d *Controller) query(scope string, saved, effective values) (values, error) {
	return desktopsettings.Query(d.snapshot(saved, effective), scope, saved == nil && effective == nil)
}
func (d *Controller) editSettings(scope string, defaults bool, draft values) error {
	token := newID(16)
	plan, err := desktopsettings.Editor(d.snapshot(nil, nil), scope, defaults, draft, token)
	if err != nil {
		return err
	}
	d.retireEditor()
	d.editor = &editor{token: token, scope: scope, generation: d.generation, keys: plan.Keys, deadline: time.Now().Add(300 * time.Second)}
	d.form("SlateSettingsForm", plan.Lines, false)
	return d.fatal
}
func (d *Controller) currentEditor(token string) (*editor, error) {
	e := d.editor
	if e == nil || e.token != token || e.generation != d.generation {
		return nil, errors.New("Settings editor is stale; reopen it before applying")
	}
	if !time.Now().Before(e.deadline) {
		d.retireEditor()
		return nil, errors.New("Settings editor expired; reopen it before applying")
	}
	return e, nil
}
func (d *Controller) editorValues(argument string) (*editor, values, error) {
	words := splitFields(argument)
	if len(words) == 0 {
		return nil, nil, errors.New("Missing settings editor values")
	}
	e, err := d.currentEditor(words[0])
	if err != nil {
		return nil, nil, err
	}
	if len(words) != len(e.keys)+1 {
		return nil, nil, errors.New("Incomplete settings editor values")
	}
	changes := values{}
	for i, key := range e.keys {
		value, err := desktopsettings.Value(key, words[i+1], true)
		if err != nil {
			return nil, nil, err
		}
		changes[key] = value
	}
	return e, changes, nil
}
func (d *Controller) settingsMenu(scope string) error {
	if _, err := desktopsettings.Scope(scope); err != nil {
		return err
	}
	return d.boundSettingsMenus(scope)
}

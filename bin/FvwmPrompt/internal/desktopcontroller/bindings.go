// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"context"
	"encoding/hex"
	"encoding/json"
	"errors"
	"reflect"
	"strconv"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/viewbinding"
)

// Exactly three domains own subscriptions. Worker bindings observe committed
// request intents, not invented worker-data revisions: only a matching successful
// completion may commit checked data. Retained menus are desired publication
// deltas, not acknowledged core state or another settings store. All this state
// belongs to the resident event loop.
type menuBinding struct {
	policy *viewbinding.Binding
	serial uint64
	menus  []desktopsettings.Menu
	owned  map[string]bool
	known  bool
	lost   bool
}

func newMenuBinding(source string) *menuBinding {
	policy, err := viewbinding.New(source)
	if err != nil {
		panic(err) // Fixed internal source identifiers cannot be invalid.
	}
	return &menuBinding{policy: policy, owned: map[string]bool{}}
}

func (d *Controller) menuBinding(key string) *menuBinding {
	switch key {
	case "apps":
		return d.appBinding
	case "status":
		return d.systemBinding
	}
	return nil
}

func (d *Controller) publishMenus(binding *menuBinding, menus []desktopsettings.Menu) {
	for _, old := range binding.menus {
		present := false
		for _, menu := range menus {
			present = present || old.Name == menu.Name
		}
		if !present {
			d.emit("DestroyMenu " + old.Name)
		}
	}
	for _, menu := range menus {
		binding.owned[menu.Name] = true
		equal := false
		for _, old := range binding.menus {
			if old.Name == menu.Name {
				equal = reflect.DeepEqual(old, menu)
				break
			}
		}
		if !equal {
			d.publishBoundMenu(binding, menu)
		}
	}
	if d.fatal == nil {
		binding.menus = menus
	}
}

func (d *Controller) boundSettingsMenus(scope string) error {
	menus, err := desktopsettings.Menus(d.snapshot(nil, nil), "all")
	if err != nil {
		return err
	}
	// Scope selects the settings root's rows, never the lifetime of the
	// registry's submenus. Appearance and previously published popups can
	// continue to reference any of these fixed names.
	if scope != "all" {
		scoped, scopeErr := desktopsettings.Menus(d.snapshot(nil, nil), scope)
		if scopeErr != nil {
			return scopeErr
		}
		for i := range menus {
			if menus[i].Name == "SlateSettings" {
				menus[i] = scoped[len(scoped)-1]
				break
			}
		}
	}
	menus = append(menus, desktopsettings.AppearanceMenu(d.effective))
	b := d.settingsBinding
	if err = b.policy.Observe("settings", viewbinding.Version{Producer: d.instance, Revision: d.generation}); err != nil {
		return err
	}
	// Clock policy and retained-wallpaper resolution can change a projection
	// within the same committed settings generation.
	if !reflect.DeepEqual(b.menus, menus) {
		if err = b.policy.Invalidate(); err != nil {
			return err
		}
	}
	if ticket, ok := b.policy.Begin(); ok && b.policy.Complete(ticket, true) {
		d.publishMenus(b, menus)
		b.known = d.fatal == nil
	}
	return d.fatal
}

// Repeated explicit refreshes replace one pending intent. Existing job slots
// retain their lifetime; a newer intent invalidates publication, never starts an
// overlapping helper for that domain, and needs no per-control timer.
func (d *Controller) requestMenuJob(key string) bool {
	b := d.menuBinding(key)
	if d.closed || b == nil || b.policy.State() == viewbinding.Closed {
		return false
	}
	if b.serial == ^uint64(0) {
		b.policy.Close()
		d.menuJobFailed(key)
		d.notice("Desktop menu refresh identity exhausted; restart FvwmDesktop.")
		return false
	}
	b.serial++
	if err := b.policy.Observe("intent", viewbinding.Version{Producer: d.instance, Revision: strconv.FormatUint(b.serial, 10)}); err != nil {
		return false
	}
	if key == "apps" && len(b.menus) == 0 {
		d.applicationMenu(nil)
	}
	d.startMenuJob(key)
	return true
}

func (d *Controller) startMenuJob(key string) bool {
	b := d.menuBinding(key)
	if d.closed || b == nil || len(d.jobs) >= 4 {
		return false
	}
	if _, exists := d.jobs[key]; exists {
		return false
	}
	ticket, ok := b.policy.Begin()
	if !ok {
		return false
	}
	args := values{}
	if key == "status" {
		args["system"] = d.system
	}
	helper := d.worker
	d.submit(key, func(ctx context.Context) (any, error) { return worker(ctx, helper, key, args) })
	d.jobs[key].binding, d.jobs[key].ticket = b, ticket
	return true
}

func (d *Controller) retireMenuJob(j *job) {
	if j != nil && j.binding != nil {
		j.binding.policy.Complete(j.ticket, false)
	}
}

func (d *Controller) menuJobFailed(key string) {
	b := d.menuBinding(key)
	if b == nil || d.closed {
		return
	}
	b.lost = true
	_ = b.policy.Lost("intent")
	if key == "apps" {
		d.applicationMenu(nil)
	} else {
		d.lastCPU = nil
		d.statusButton("Status unavailable")
		d.systemMenu()
	}
}

func (d *Controller) resetWorkerMenus() {
	for _, key := range []string{"apps", "status"} {
		old := d.menuBinding(key)
		old.policy.Close()
		next := newMenuBinding("intent")
		next.menus, next.known, next.owned = old.menus, old.known, old.owned
		next.lost = true
		if key == "apps" {
			d.appBinding = next
			d.applicationMenu(nil)
		} else {
			d.systemBinding = next
			d.lastCPU = nil
			d.statusButton("Status pending")
			d.systemMenu()
		}
		d.requestMenuJob(key)
	}
}

func (d *Controller) workerAvailable() bool {
	return !d.closed && d.systemBinding.known && !d.systemBinding.lost
}

func (d *Controller) requireApplications() error {
	if d.closed || !d.appBinding.known || d.appBinding.lost {
		return errors.New("Application discovery is unavailable; discover again before launching")
	}
	return nil
}

var errMenuSuperseded = errors.New("Desktop refresh was superseded")

func (d *Controller) completeMenuJob(result job) error {
	b := result.binding
	if b == nil || b != d.menuBinding(result.key) || d.closed {
		d.retireMenuJob(result.origin)
		return errMenuSuperseded
	}
	err := result.err
	if err == nil {
		if result.key == "apps" {
			_, err = checkedApps(result.result)
		} else {
			_, err = checkedStatus(result.result)
		}
	}
	accepted := b.policy.Complete(result.ticket, err == nil)
	if !accepted && err == nil {
		return errMenuSuperseded
	}
	if err != nil {
		pending := b.policy.Pending()
		d.menuJobFailed(result.key)
		if pending {
			// Even a superseded worker failure establishes unavailable data.
			// Re-admit only the already retained latest intent; do not invent
			// another refresh or make its actions ready before completion.
			version := viewbinding.Version{Producer: d.instance,
				Revision: strconv.FormatUint(b.serial, 10)}
			if observeErr := b.policy.Observe("intent", version); observeErr != nil {
				return observeErr
			}
			return errMenuSuperseded
		}
	}
	return err
}

func (d *Controller) statusButton(text string) {
	if text != d.statusLabel {
		d.button("status", text)
		if d.fatal == nil {
			d.statusLabel = text
		}
	}
}

type bindingDomainReport struct {
	Policy viewbinding.Stats `json:"policy"`
	Known  bool              `json:"known"`
	Lost   bool              `json:"lost"`
	Menus  int               `json:"menus"`
}

// Validate every bounded native command before submitting any of the snapshot.
func bindingDiagnosticCommands(token string, report [3]bindingDomainReport) ([3]string, error) {
	if !identifier(token) {
		return [3]string{}, errors.New("Invalid binding diagnostic token")
	}
	var commands [3]string
	for i, name := range [3]string{"settings", "apps", "system"} {
		data, err := json.Marshal(report[i])
		if err != nil {
			return [3]string{}, err
		}
		command := "Echo FVWM_BINDINGS_1 " + token + " " + name +
			" " + hex.EncodeToString(data)
		if len(command) > 1000 {
			return [3]string{}, errors.New("Binding diagnostic exceeds module command limit")
		}
		commands[i] = command
	}
	return commands, nil
}

// Binding diagnostics are explicit snapshots of fixed metadata. Reading them
// neither observes a source nor starts work; normal desktop operation emits none.
func (d *Controller) bindingStats(token string) error {
	var report [3]bindingDomainReport
	for i, b := range [3]*menuBinding{d.settingsBinding, d.appBinding, d.systemBinding} {
		entry := bindingDomainReport{Policy: viewbinding.Stats{State: viewbinding.Closed}}
		if b != nil {
			entry.Known, entry.Lost, entry.Menus = b.known, b.lost, len(b.menus)
			if b.policy != nil {
				entry.Policy = b.policy.Stats()
			}
		}
		report[i] = entry
	}
	commands, err := bindingDiagnosticCommands(token, report)
	if err != nil {
		return err
	}
	for _, command := range commands {
		d.emit(command)
	}
	return d.fatal
}

func (d *Controller) menuDomain(binding *menuBinding) string {
	switch binding {
	case d.settingsBinding:
		return "settings"
	case d.appBinding:
		return "apps"
	case d.systemBinding:
		return "system"
	}
	return ""
}

func menuRevision(menu desktopsettings.Menu) string {
	data, _ := json.Marshal(menu) // The native projection contains only strings.
	return hash(data)
}

func (d *Controller) publishBoundMenu(binding *menuBinding, menu desktopsettings.Menu) {
	d.menu(menu)
	// Core menu mutation can be refused while mapped. The installed hook's
	// projection identity lets popdown repair a refused update after unmap,
	// without claiming an ACK or polling each control.
	action := "SendToModule FvwmDesktop binding-menu-popdown " +
		d.instance + " " + d.menuDomain(binding) + " " + menu.Name +
		" " + menuRevision(menu)
	d.emit("AddToMenu " + menu.Name + " DynamicPopDownAction " + action)
}

func validBoundMenu(domain, name string) bool {
	switch domain {
	case "settings":
		if name == "SlateAppearance" || name == "SlateSettings" ||
			name == "SlateOpacityPreview" {
			return true
		}
		key := strings.TrimPrefix(name, "SlateSetting_")
		_, ok := desktopsettings.Registry()[key]
		return strings.HasPrefix(name, "SlateSetting_") && ok
	case "system":
		return name == "SlateSystem" || name == "SlateSound" || name == "SlateSession"
	case "apps":
		if name == "SlateLauncherMenu" || name == "SlateSearchResults" {
			return true
		}
		for _, prefix := range []string{"SlateApps", "SlateResults"} {
			page, err := strconv.Atoi(strings.TrimPrefix(name, prefix))
			if err == nil && page >= 0 && page < 26 && name == prefix+strconv.Itoa(page) {
				return true
			}
		}
	}
	return false
}

func (d *Controller) bindingMenuPopdown(argument string) error {
	if d.closed {
		return nil
	}
	words, err := wordCount(argument, 4)
	if err != nil {
		return err
	}
	if words[0] != d.instance {
		return nil
	}
	if !validBoundMenu(words[1], words[2]) ||
		!identity(words[3], 64) {
		return errors.New("Invalid menu popdown callback")
	}
	b := d.settingsBinding
	if words[1] == "apps" {
		b = d.appBinding
	} else if words[1] == "system" {
		b = d.systemBinding
	}
	if b == nil || !b.owned[words[2]] {
		return errors.New("Menu popdown callback does not belong to this controller")
	}
	for _, menu := range b.menus {
		if menu.Name == words[2] {
			if words[3] != menuRevision(menu) {
				d.publishBoundMenu(b, menu)
			}
			return d.fatal
		}
	}
	// An obsolete page may have refused its original destruction while mapped.
	// Ever-owned names have a fixed registry/page bound and retain this hook.
	d.emit("DestroyMenu " + words[2])
	return d.fatal
}

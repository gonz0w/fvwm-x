// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"
	"unicode"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

type directoryRoleView struct {
	visible                              bool
	collectionCurrent                    bool
	lifetime, page, lastControl, request uint64
	alias, generation, status, detail    string
	snapshot                             *filemodel.Snapshot
	result                               *directoryRoleResult
	selected                             int
}
type directoryRoleCallback struct {
	parent, lifetime, page, ticket uint64
	action, packet                 string
}

func (b *browser) retireDirectoryRoles() error {
	if b.roles == nil {
		return nil
	}
	alias := b.roles.alias
	b.roles = nil
	if alias == "" {
		return nil
	}
	return b.sendRoleCommands([]string{"KillModule FvwmForm " + alias, "DestroyModuleConfig " + alias + ":*"})
}

func (b *browser) beginDirectoryRoles(visible bool) error {
	current := b.dataReady()
	retained := b.roles != nil && b.roles.snapshot == b.snapshot && b.roles.collectionCurrent
	if b.active || b.pending != nil || b.snapshot == nil || b.snapshot.Schema != 1 || (!current && !retained) {
		return errors.New("Folder applications unavailable while file work is pending")
	}
	if b.roleSerial == ^uint64(0) {
		return errors.New("Folder application chooser lifetime exhausted")
	}
	if err := b.retireDirectoryRoles(); err != nil {
		return err
	}
	b.roleSerial++
	view := &directoryRoleView{lifetime: b.roleSerial, snapshot: b.snapshot, status: "loading", visible: visible, collectionCurrent: current || retained}
	if err := b.queue(workRequest{Operation: "directory-roles", Snapshot: b.snapshot, Program: b.terminal}, "directory-roles"); err != nil {
		return err
	}
	view.request = b.serial
	b.roles = view
	return b.renderDirectoryRoles()
}

func (b *browser) acceptDirectoryRoles(event completedWork) error {
	view := b.roles
	if view == nil || view.request != event.work.serial {
		return nil
	}
	if event.result.Status != "roles" || !event.result.RoleFolderValid || !validDirectoryRoleResult(event.result.Roles) {
		view.status, view.generation, view.result = "failed", "", nil
		view.detail = event.result.Error
		if view.detail == "" {
			view.detail = "Folder applications unavailable; refresh to retry"
		}
		return b.renderDirectoryRoles()
	}
	view.result = event.result.Roles
	view.status, view.detail = "ready", ""
	view.selected = 0
	for i, choice := range view.result.Choices {
		if choice.ID == view.result.DefaultID {
			view.selected = i
			break
		}
	}
	data, _ := json.Marshal(struct {
		Owner             string
		Lifetime, Request uint64
		Result            *directoryRoleResult
	}{b.instance, view.lifetime, view.request, view.result})
	sum := sha256.Sum256(data)
	view.generation = hex.EncodeToString(sum[:16])
	if len(view.result.Choices) == 0 {
		view.detail = "No associated folder applications are available"
	}
	return b.renderDirectoryRoles()
}

func (b *browser) directoryRoleRequest(generation, id string) (workRequest, string) {
	view := b.roles
	if view == nil || view.status != "ready" || view.result == nil {
		return workRequest{}, "choices_unavailable"
	}
	if generation != view.generation {
		return workRequest{}, "stale_choices"
	}
	if b.active || b.pending != nil {
		return workRequest{}, "busy"
	}
	if !b.dataReady() || view.snapshot != b.snapshot {
		return workRequest{}, "stale_choices"
	}
	for _, choice := range view.result.Choices {
		if roleToken(choice) == id {
			return workRequest{Operation: "directory-role-launch", Snapshot: view.snapshot, Program: b.terminal, RoleID: choice.ID, RoleFingerprint: choice.Fingerprint}, ""
		}
	}
	return workRequest{}, "invalid_choice"
}

func (b *browser) startDirectoryRole(generation, id string) error {
	request, code := b.directoryRoleRequest(generation, id)
	if code != "" {
		return errors.New("Folder application refused: " + code)
	}
	if err := b.queue(request, "directory-role-launch"); err != nil {
		return err
	}
	b.selectDirectoryRole(id)
	return nil
}

func (b *browser) directoryRoleSummary() map[string]any {
	if b.roles == nil {
		return map[string]any{"status": "closed", "choice_generation": "", "default_id": "", "count": 0}
	}
	view := b.roles
	count, defaultID := 0, ""
	if view.result != nil {
		count = len(view.result.Choices)
		for _, choice := range view.result.Choices {
			if choice.ID == view.result.DefaultID {
				defaultID = roleToken(choice)
				break
			}
		}
	}
	return map[string]any{"status": view.status, "choice_generation": view.generation, "default_id": defaultID, "count": count, "detail": displayText(view.detail, 160)}
}

func (b *browser) directoryRolePage(query fileQuery, result map[string]any) string {
	for key, value := range b.directoryRoleSummary() {
		result[key] = value
	}
	choices := []directoryRoleChoice{}
	if b.roles != nil && b.roles.result != nil && b.roles.status == "ready" {
		choices = b.roles.result.Choices
	}
	if query.Offset > len(choices) {
		return "offset_out_of_range"
	}
	end := query.Offset + query.Limit
	if end > len(choices) {
		end = len(choices)
	}
	items := make([]map[string]any, 0, end-query.Offset)
	for _, choice := range choices[query.Offset:end] {
		items = append(items, map[string]any{"id": roleToken(choice), "label": displayText(choice.Name, 96), "fingerprint": choice.Fingerprint, "terminal": choice.Terminal, "default": choice.ID == b.roles.result.DefaultID})
	}
	result["items"], result["total"], result["offset"], result["next_offset"] = items, len(choices), query.Offset, nil
	if end < len(choices) {
		result["next_offset"] = end
	}
	return ""
}

func roleLabel(value string, limit int) string {
	var out strings.Builder
	for _, r := range value {
		text := string(r)
		if !unicode.IsPrint(r) || strings.ContainsRune("$%\\\"&", r) {
			text = fmt.Sprintf("[U+%04X]", r)
		}
		if len(text) > limit-out.Len() {
			out.WriteString("...")
			break
		}
		out.WriteString(text)
	}
	return out.String()
}

func (b *browser) sendRoleCommands(commands []string) error {
	if b.roleSendSerial == ^uint64(0) {
		return errors.New("Folder application send generation exhausted")
	}
	b.roleSendSerial++
	return b.sendCommandBatch(commands, "FVWM_FILES_ROLES_SYNC", b.roleSendSerial, nil, b.roleAckReplies, time.Now().Add(workerLimit))
}

func (b *browser) renderDirectoryRoles() error {
	view := b.roles
	if view == nil || !view.visible {
		return nil
	}
	if view.page == ^uint64(0) {
		return errors.New("Folder application page generation exhausted")
	}
	oldAlias := view.alias
	view.page++
	view.lastControl = 0
	view.alias = fmt.Sprintf("FvwmFiles-%s-roles-%d-%d", b.instance, view.lifetime, view.page)
	commands := []string{}
	if oldAlias != "" {
		commands = append(commands, "KillModule FvwmForm "+oldAlias, "DestroyModuleConfig "+oldAlias+":*")
	}
	line := func(value string) { commands = append(commands, "*"+view.alias+": "+value) }
	callback := fmt.Sprintf("Echo FVWM_FILES_ROLES_1 %s %d %d %d ", b.instance, b.form, view.lifetime, view.page)
	line("UIScale monitor")
	line(`Title "Open folder with"`)
	line("Line left")
	line(`Text "Current folder only; this does not change defaults."`)
	line("Line left")
	line(`Label path 42 "` + displayText(string(view.snapshot.Path), 100) + `"`)
	line("LabelViewport path")
	line("Line left")
	line(`Label status 42 "` + displayText(view.status+": "+view.detail, 120) + `"`)
	line("LabelViewport status")
	line("Line left")
	line("List applications 42 6")
	line("ListViewport 18 1")
	line("ListVersioned")
	choices := []directoryRoleChoice{}
	if view.result != nil {
		choices = view.result.Choices
	}
	for index, choice := range choices {
		selected, label := "off", roleLabel(choice.Name, 160)
		if index == view.selected {
			selected = "on"
		}
		if choice.ID == view.result.DefaultID {
			label += " (default)"
		}
		line(fmt.Sprintf(`ListItem %s "%s"`, selected, label))
	}
	if len(choices) > 0 {
		line(fmt.Sprintf("ListFocus %d", view.selected+1))
	}
	button := func(label, key, action string) {
		line(`Button continue "` + label + `" ` + key)
		line("Command " + callback + action + " x$(applications:Base64) $(FormJob:Token)")
	}
	line("Line left")
	button("Launch (Enter)", "^M", "launch")
	button("Refresh (F5)", "F5", "refresh")
	button("Close (Esc)", "^[", "close")
	line("Line left")
	line(`Text "Arrows: choose; Enter: launch focused application."`)
	line("Line left")
	line(`Text "Start is a handoff, not application completion."`)
	line("CloseCommand " + callback + "closed - -")
	line(`Timeout 3000 "KillModule FvwmForm ` + view.alias + `" ""`)
	commands = append(commands, "Module FvwmForm "+view.alias)
	return b.sendRoleCommands(commands)
}

func parseDirectoryRoleCallback(message, instance string) (directoryRoleCallback, bool) {
	p := strings.Fields(message)
	if len(message) > 800 || len(p) != 8 || p[0] != "FVWM_FILES_ROLES_1" || p[1] != instance {
		return directoryRoleCallback{}, false
	}
	parent, ok := canonicalDecimal(p[2])
	if !ok {
		return directoryRoleCallback{}, false
	}
	life, ok := canonicalDecimal(p[3])
	if !ok {
		return directoryRoleCallback{}, false
	}
	page, ok := canonicalDecimal(p[4])
	if !ok {
		return directoryRoleCallback{}, false
	}
	value := directoryRoleCallback{parent: parent, lifetime: life, page: page, action: p[5], packet: p[6]}
	if p[5] == "closed" && p[6] == "-" && p[7] == "-" {
		return value, true
	}
	if p[5] != "launch" && p[5] != "refresh" && p[5] != "close" {
		return directoryRoleCallback{}, false
	}
	ticket, ok := invocationTicket(p[7])
	if !ok || len(p[6]) > 128 || !strings.HasPrefix(p[6], "x") {
		return directoryRoleCallback{}, false
	}
	value.ticket = ticket
	return value, true
}

func (b *browser) routeDirectoryRole(message string) {
	if strings.HasPrefix(message, "FVWM_FILES_ROLES_SYNC "+b.instance+" ") && b.roleAckReplies != nil {
		b.roleAckReplies.offer(message, message)
	}
	if value, ok := parseDirectoryRoleCallback(message, b.instance); ok {
		select {
		case b.roleEvents <- value:
		default:
		}
	}
}

func roleFocus(packet string, rows int) (int, error) {
	data, err := nativePacket(packet, 1)
	if err != nil || rows < 1 || rows > directoryRoleMax || len(data) != 2+(rows+7)/8 {
		return 0, errors.New("Invalid folder application selection")
	}
	focus := int(binary.BigEndian.Uint16(data))
	if focus >= rows {
		return 0, errors.New("Invalid folder application focus")
	}
	if rows%8 != 0 && data[len(data)-1]>>uint(rows%8) != 0 {
		return 0, errors.New("Invalid folder application padding")
	}
	return focus, nil
}

func (b *browser) directoryRoleAction(callback directoryRoleCallback) (bool, error) {
	view := b.roles
	if view == nil || callback.parent != b.form || callback.lifetime != view.lifetime || callback.page != view.page {
		return false, nil
	}
	if callback.action == "closed" {
		return true, b.retireDirectoryRoles()
	}
	if callback.ticket == 0 || callback.ticket <= view.lastControl {
		return false, nil
	}
	view.lastControl = callback.ticket
	switch callback.action {
	case "close":
		return true, b.retireDirectoryRoles()
	case "refresh":
		if err := b.beginDirectoryRoles(true); err != nil {
			view.detail = err.Error()
			return true, b.renderDirectoryRoles()
		}
		return true, nil
	case "launch":
		if view.status != "ready" || view.result == nil {
			return true, nil
		}
		index, err := roleFocus(callback.packet, len(view.result.Choices))
		if err != nil {
			return false, nil
		}
		if err = b.startDirectoryRole(view.generation, roleToken(view.result.Choices[index])); err != nil {
			view.detail = err.Error()
			return true, b.renderDirectoryRoles()
		}
		view.detail = "Starting selected application; outcome not yet known"
		return true, b.renderDirectoryRoles()
	}
	return false, nil
}

func (b *browser) selectDirectoryRole(id string) {
	if b.roles != nil && b.roles.result != nil {
		for index, choice := range b.roles.result.Choices {
			if roleToken(choice) == id {
				b.roles.selected = index
				return
			}
		}
	}
}

func (b *browser) finishDirectoryRole(event completedWork) error {
	if event.work.request.Operation == "directory-roles" {
		return b.acceptDirectoryRoles(event)
	}
	if event.work.request.Operation == "directory-role-launch" && b.roles != nil && b.roles.snapshot == event.work.request.Snapshot {
		if event.result.Status == "started" {
			b.roles.detail = "Application started; completion is not tracked"
		} else {
			b.roles.detail = event.result.Error
			b.roles.status, b.roles.generation, b.roles.result = "failed", "", nil
		}
		return b.renderDirectoryRoles()
	}
	return nil
}

// The role worker cannot repair arbitrary file-data failure. Only a still-owned
// chooser's proven pre-role collection may remain current, after root validation.
// Every intervening non-role intent and native parent retirement drops this view.
func (b *browser) directoryRolePreservesCollection(event completedWork) bool {
	view := b.roles
	if view == nil || !view.collectionCurrent || view.snapshot != b.snapshot || view.snapshot != event.work.request.Snapshot || !event.result.RoleFolderValid {
		return false
	}
	switch event.work.request.Operation {
	case "directory-roles":
		return view.request == event.work.serial && (event.result.Status == "failed" || event.result.Status == "roles" && validDirectoryRoleResult(event.result.Roles))
	case "directory-role-launch":
		return event.result.Status == "started" || event.result.Status == "failed"
	}
	return false
}

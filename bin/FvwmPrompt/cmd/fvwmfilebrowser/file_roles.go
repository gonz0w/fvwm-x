// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"path/filepath"
	"strings"
	"time"
)

// The existing chooser owns native construction and choice presentation. File
// state adds only the explicit selected target and its observed MIME proof.
type fileRoleView struct {
	chooser                directoryRoleView
	id, mime, observedMIME string
	collection             uint64
	target                 []byte
}

// Metadata-only admission; filesystem identity and content are worker-owned.
func (b *browser) selectedRoleFile() (string, string, string) {
	ids := b.model.SelectedIDs()
	if b.snapshot == nil || len(ids) != 1 {
		return "", "", "selection_unavailable"
	}
	for _, item := range b.snapshot.Items {
		if item.ID == ids[0] && item.Kind == "file" && item.Identity.Mode.IsRegular() {
			name := string(item.Name)
			if name != "" && name != "." && name != ".." && !strings.ContainsAny(name, "/\x00") {
				return item.ID, filepath.Join(string(b.snapshot.Path), name), ""
			}
		}
	}
	return "", "", "selection_unavailable"
}

func (b *browser) fileRoleCaptureCurrent() bool {
	view := b.fileRoles
	if view == nil || view.chooser.snapshot != b.snapshot || view.collection != b.model.Generation {
		return false
	}
	id, path, code := b.selectedRoleFile()
	return code == "" && id == view.id && path == string(view.target)
}

func (b *browser) retireFileRoles() error {
	if b.fileRoles == nil {
		return nil
	}
	alias := b.fileRoles.chooser.alias
	b.fileRoles = nil
	if alias == "" {
		return nil
	}
	return b.sendRoleCommands([]string{"KillModule FvwmForm " + alias, "DestroyModuleConfig " + alias + ":*"})
}

func (b *browser) retireStaleFileRoles() error {
	if b.fileRoles != nil && !b.fileRoleCaptureCurrent() {
		return b.retireFileRoles()
	}
	return nil
}

func (b *browser) beginFileRoles(visible bool) error {
	id, target, code := b.selectedRoleFile()
	if code != "" {
		return errors.New("Select exactly one current regular file; links and special files are unavailable")
	}
	current := b.dataReady()
	retained := b.fileRoleCaptureCurrent() && b.fileRoles.chooser.collectionCurrent
	if b.active || b.pending != nil || !current && !retained {
		return errors.New("File applications unavailable; refresh the browser folder and reselect the file")
	}
	if b.roleSerial == ^uint64(0) {
		return errors.New("File application chooser lifetime exhausted")
	}
	if err := b.retireFileRoles(); err != nil {
		return err
	}
	b.roleSerial++
	view := &fileRoleView{chooser: directoryRoleView{lifetime: b.roleSerial, snapshot: b.snapshot, status: "loading", visible: visible, collectionCurrent: current || retained}, id: id, collection: b.model.Generation, target: []byte(target)}
	if err := b.queue(workRequest{Operation: "file-roles", Snapshot: b.snapshot, ID: id, Program: b.terminal}, "file-roles"); err != nil {
		return err
	}
	view.chooser.request = b.serial
	b.fileRoles = view
	return b.renderFileRoles()
}

func (b *browser) renderFileRoles() error {
	if b.fileRoles == nil {
		return nil
	}
	view := b.fileRoles
	return b.renderRoleChooser(&view.chooser, true, view.target, view.observedMIME)
}

func (b *browser) fileRoleRequest(generation, id string) (workRequest, string) {
	view := b.fileRoles
	if view == nil || view.chooser.status != "ready" || view.chooser.result == nil {
		return workRequest{}, "choices_unavailable"
	}
	if generation != view.chooser.generation || !b.fileRoleCaptureCurrent() {
		return workRequest{}, "stale_choices"
	}
	if b.active || b.pending != nil {
		return workRequest{}, "busy"
	}
	if !b.dataReady() {
		return workRequest{}, "stale_choices"
	}
	for _, choice := range view.chooser.result.Choices {
		if roleToken(choice) == id {
			return workRequest{Operation: "file-role-launch", Snapshot: view.chooser.snapshot, ID: view.id, Program: b.terminal, RoleID: choice.ID, RoleFingerprint: choice.Fingerprint, RoleMIME: view.mime, ObservedMIME: view.observedMIME}, ""
		}
	}
	return workRequest{}, "invalid_choice"
}

func (b *browser) selectFileRole(id string) {
	if b.fileRoles == nil || b.fileRoles.chooser.result == nil {
		return
	}
	for index, choice := range b.fileRoles.chooser.result.Choices {
		if roleToken(choice) == id {
			b.fileRoles.chooser.selected = index
			return
		}
	}
}

func (b *browser) startFileRole(generation, id string) error {
	request, code := b.fileRoleRequest(generation, id)
	if code != "" {
		return errors.New("File application refused: " + code)
	}
	if err := b.queue(request, "file-role-launch"); err != nil {
		return err
	}
	b.selectFileRole(id)
	return nil
}

func (b *browser) fileRoleSummary() map[string]any {
	var chooser *directoryRoleView
	if b.fileRoleCaptureCurrent() {
		chooser = &b.fileRoles.chooser
	}
	result := roleViewSummary(chooser)
	result["mime"], result["canonical_mime"], result["target_id"], result["target_label"] = "", "", "", ""
	if chooser != nil {
		view := b.fileRoles
		result["mime"], result["canonical_mime"], result["target_id"], result["target_label"] = view.observedMIME, view.mime, view.id, displayText(string(view.target), 100)
	}
	return result
}

func (b *browser) fileRolePage(query fileQuery, result map[string]any) string {
	for key, value := range b.fileRoleSummary() {
		result[key] = value
	}
	var chooser *directoryRoleView
	if b.fileRoleCaptureCurrent() {
		chooser = &b.fileRoles.chooser
	}
	return roleChoicePage(chooser, query, result)
}

func (b *browser) finishFileRole(event completedWork) error {
	view := b.fileRoles
	if view == nil || !b.fileRoleCaptureCurrent() || view.id != event.work.request.ID || view.chooser.snapshot != event.work.request.Snapshot {
		return nil
	}
	chooser := &view.chooser
	if event.work.request.Operation == "file-roles" {
		if chooser.request != event.work.serial {
			return nil
		}
		result := event.result.FileRoles
		if event.result.Status != "roles" || !event.result.RoleFileValid || !validFileRoleResult(result) {
			chooser.status, chooser.generation, chooser.result = "failed", "", nil
			chooser.detail = event.result.Error
			if chooser.detail == "" {
				chooser.detail = "File applications unavailable; refresh choices to retry"
			}
		} else {
			chooser.status, chooser.detail, chooser.result = "ready", "", &result.directoryRoleResult
			view.mime, view.observedMIME = result.MIME, result.ObservedMIME
			chooser.selected = 0
			for index, choice := range chooser.result.Choices {
				if choice.ID == chooser.result.DefaultID {
					chooser.selected = index
					break
				}
			}
			data, _ := json.Marshal(struct {
				Owner             string
				Lifetime, Request uint64
				ID                string
				Result            *fileRoleResult
			}{b.instance, chooser.lifetime, chooser.request, view.id, result})
			sum := sha256.Sum256(data)
			chooser.generation = hex.EncodeToString(sum[:16])
			if len(chooser.result.Choices) == 0 {
				chooser.detail = "No eligible file applications are available"
			}
		}
	} else if event.work.request.Operation == "file-role-launch" {
		if event.result.Status == "started" {
			chooser.detail = "Application launch submitted; completion is not tracked"
		} else {
			chooser.status, chooser.generation, chooser.result = "failed", "", nil
			chooser.detail = event.result.Error
		}
	} else {
		return nil
	}
	if !event.result.RoleFileValid {
		chooser.collectionCurrent = false
	}
	return b.renderFileRoles()
}

func (b *browser) fileRolePreservesCollection(event completedWork) bool {
	view := b.fileRoles
	if view == nil || !view.chooser.collectionCurrent || !b.fileRoleCaptureCurrent() || view.chooser.snapshot != event.work.request.Snapshot || view.id != event.work.request.ID || !event.result.RoleFileValid {
		return false
	}
	switch event.work.request.Operation {
	case "file-roles":
		return view.chooser.request == event.work.serial && (event.result.Status == "failed" || event.result.Status == "roles" && validFileRoleResult(event.result.FileRoles))
	case "file-role-launch":
		return event.result.Status == "started" || event.result.Status == "failed"
	}
	return false
}

func (b *browser) fileRoleAction(callback directoryRoleCallback) (bool, error) {
	view := b.fileRoles
	if view == nil {
		return false, nil
	}
	chooser := &view.chooser
	if callback.parent != b.form || callback.lifetime != chooser.lifetime || callback.page != chooser.page {
		return false, nil
	}
	if callback.action == "closed" {
		return true, b.retireFileRoles()
	}
	if callback.ticket == 0 || callback.ticket <= chooser.lastControl {
		return false, nil
	}
	chooser.lastControl = callback.ticket
	if callback.action == "close" {
		return true, b.retireFileRoles()
	}
	if b.alias != "" {
		// Native selection notifications and chooser callbacks have separate
		// bounded mailboxes. Observe the browser's current selection here too,
		// so a queued older notification cannot authorize the previous target.
		b.renderDeadline = time.Now().Add(workerLimit)
		token, err := b.nextFeedbackToken()
		var packet []byte
		if err == nil {
			packet, err = b.snapshotNativeFeedback(token)
		}
		if err == nil {
			err = b.model.SelectNativeView(b.model.Generation, entryIDs(b.viewEntries), packet)
		}
		if err != nil {
			chooser.detail = "Current file selection unavailable; retry the action"
			return true, b.renderFileRoles()
		}
	}
	if !b.fileRoleCaptureCurrent() {
		return true, b.retireFileRoles()
	}
	switch callback.action {
	case "refresh":
		if err := b.beginFileRoles(true); err != nil {
			chooser.detail = err.Error()
			return true, b.renderFileRoles()
		}
		return true, nil
	case "launch":
		if chooser.status != "ready" || chooser.result == nil {
			return true, nil
		}
		index, err := roleFocus(callback.packet, len(chooser.result.Choices))
		if err != nil {
			return false, nil
		}
		if err := b.startFileRole(chooser.generation, roleToken(chooser.result.Choices[index])); err != nil {
			chooser.detail = err.Error()
		} else {
			chooser.detail = "Starting selected application; outcome not yet known"
		}
		return true, b.renderFileRoles()
	}
	return false, nil
}

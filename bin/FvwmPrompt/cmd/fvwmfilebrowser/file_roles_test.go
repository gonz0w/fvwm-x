// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"runtime"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

func TestFileRolesRequireExactlyOneSelectedRegularFile(t *testing.T) {
	b, _ := providerFixture(t)
	b.model.Focus = b.snapshot.Items[0].ID
	if _, _, code := b.selectedRoleFile(); code != "selection_unavailable" {
		t.Fatal("focused-item fallback", code)
	}
	for _, ids := range [][]string{{b.snapshot.Items[0].ID, b.snapshot.Items[1].ID}, {b.snapshot.Items[0].ID}} {
		b.model.SelectIDs(b.model.Generation, ids)
		if len(ids) > 1 {
			if _, _, code := b.selectedRoleFile(); code != "selection_unavailable" {
				t.Fatal("multiple selection", code)
			}
			continue
		}
		for _, kind := range []string{"directory", "symlink", "fifo", "socket", "other"} {
			b.snapshot.Items[0].Kind = kind
			if _, _, code := b.selectedRoleFile(); code != "selection_unavailable" {
				t.Fatal(kind, code)
			}
		}
		b.snapshot.Items[0].Kind = "file"
		b.snapshot.Items[0].Identity.Mode = os.ModeSymlink
		if _, _, code := b.selectedRoleFile(); code != "selection_unavailable" {
			t.Fatal("contradictory regular-file metadata admitted", code)
		}
		b.snapshot.Items[0].Identity.Mode = 0600
		id, path, code := b.selectedRoleFile()
		if code != "" || id != ids[0] || path != filepath.Join(string(b.snapshot.Path), string(b.snapshot.Items[0].Name)) {
			t.Fatal(id, path, code)
		}
	}
}

func fileRoleTestView(b *browser) *fileRoleView {
	id := b.snapshot.Items[0].ID
	b.model.SelectIDs(b.model.Generation, []string{id})
	return &fileRoleView{chooser: directoryRoleView{snapshot: b.snapshot, result: roleTestChoices(), status: "ready", generation: strings.Repeat("a", 32), collectionCurrent: true}, id: id, collection: b.model.Generation, mime: "text/plain", observedMIME: "text/plain", target: []byte(filepath.Join(string(b.snapshot.Path), string(b.snapshot.Items[0].Name)))}
}

func TestFileRolesProviderIdentityBoundsAndSelectionRetirement(t *testing.T) {
	b, query := providerFixture(t)
	b.fileRoles = fileRoleTestView(&b)
	for _, values := range []string{`{}`, `{"generation":"bad","id":"0123456789abcdef0123456789abcdef"}`, `{"generation":"0123456789abcdef0123456789abcdef","id":[]}`, `{"generation":"0123456789abcdef0123456789abcdef","id":"0123456789abcdef0123456789abcdef","mime":"text/plain"}`} {
		if _, err := decodeProviderAction(actionQuery(&b, "file-role-launch", values)); err == nil {
			t.Fatal("file launch admitted invalid fields or a MIME override", values)
		}
	}
	query.Scope = "file_roles"
	page, code := b.providerResult(query)
	if code != "" || page["mime"] != "text/plain" || page["canonical_mime"] != "text/plain" || page["target_id"] != b.fileRoles.id {
		t.Fatal(page, code)
	}
	data, _ := json.Marshal(page)
	if len(data) > providerResultMax || strings.Contains(string(data), "default.desktop") {
		t.Fatal("private IDs or oversized page", string(data))
	}
	id := roleToken(b.fileRoles.chooser.result.Choices[0])
	generation := b.fileRoles.chooser.generation
	request, code := b.fileRoleRequest(generation, id)
	if code != "" || request.ID != b.fileRoles.id || request.RoleMIME != "text/plain" || request.ObservedMIME != "text/plain" {
		t.Fatal(request, code)
	}
	if _, code := b.fileRoleRequest(strings.Repeat("b", 32), id); code != "stale_choices" {
		t.Fatal(code)
	}
	b.model.SelectIDs(b.model.Generation, []string{b.snapshot.Items[1].ID})
	if _, code := b.fileRoleRequest(generation, id); code != "stale_choices" {
		t.Fatal("changed selection retained captured target", code)
	}
	if err := b.retireStaleFileRoles(); err != nil || b.fileRoles != nil {
		t.Fatal("selection did not retire capture", err)
	}
	b.model.SelectIDs(b.model.Generation, []string{b.snapshot.Items[0].ID})
	if _, code := b.fileRoleRequest(generation, id); code != "choices_unavailable" {
		t.Fatal("selection round trip resurrected capture", code)
	}
}

func TestFileRolesNativeDefaultMIMEAndEmptyResult(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.snapshot.Items = []filemodel.Item{{ID: "a", Name: []byte("selected literal"), Kind: "file"}}
	// The native renderer uses already captured bytes and choices, never reads
	// selected-file content or launches a classifier on the UI loop.
	view := &fileRoleView{chooser: directoryRoleView{snapshot: b.snapshot, lifetime: 9, request: 23, visible: true, collectionCurrent: true}, id: "a", collection: b.model.Generation, target: []byte("/fixture/selected literal"), mime: "text/plain", observedMIME: "text/plain"}
	b.model.SelectIDs(b.model.Generation, []string{"a"})
	b.fileRoles = view
	event := completedWork{work: pendingWork{serial: 23, request: workRequest{Operation: "file-roles", Snapshot: b.snapshot, ID: "a"}}, result: workResult{Status: "roles", RoleFileValid: true, FileRoles: &fileRoleResult{directoryRoleResult: *roleTestChoices(), MIME: "text/plain", ObservedMIME: "text/plain"}}}
	commands, err := layoutScript(t, b, peer, func() error { return b.finishFileRole(event) }, func(string) string { return "" })
	if err != nil {
		t.Fatal(err)
	}
	text := strings.Join(commands, "\n")
	for _, wanted := range []string{`Title "Open file with"`, `Label mime 42 "Observed MIME: text/plain"`, `Label path 42 "/fixture/selected literal"`, `ListItem on "Default (default)"`, `ListFocus 2`, `Button continue "Launch (Enter)" ^M`, `Button continue "Refresh (F5)" F5`, `Button continue "Close (Esc)" ^[`} {
		if !strings.Contains(text, wanted) {
			t.Fatal("missing file chooser contract", wanted, text)
		}
	}
	view.chooser.visible = false
	event.result.FileRoles = &fileRoleResult{MIME: "text/plain", ObservedMIME: "text/plain"}
	if err := b.finishFileRole(event); err != nil || !strings.Contains(view.chooser.detail, "No eligible") {
		t.Fatal("empty association was not explicit", view.chooser.detail, err)
	}
	if _, code := b.fileRoleRequest(view.chooser.generation, strings.Repeat("b", 32)); code != "invalid_choice" {
		t.Fatal("empty result admitted launch", code)
	}
}

func TestFileRoleNativeLaunchObservesPendingBrowserSelection(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.snapshot.Items = []filemodel.Item{{ID: "a", Name: []byte("a"), Kind: "file"}, {ID: "b", Name: []byte("b"), Kind: "file"}}
	b.fileRoles = fileRoleTestView(b)
	b.fileRoles.chooser.lifetime, b.fileRoles.chooser.page = 7, 2
	callback := directoryRoleCallback{parent: b.form, lifetime: 7, page: 2, ticket: 1, action: "launch", packet: rolePacket(1, 0, 1)}
	_, err := layoutScript(t, b, peer, func() error {
		_, err := b.fileRoleAction(callback)
		return err
	}, func(command string) string {
		if strings.Contains(command, " ListWatchStatus ") {
			return feedbackReply(command, b.nativeGeneration, []byte{0, 1, 2})
		}
		return ""
	})
	if err != nil || b.fileRoles != nil || b.active {
		t.Fatal("chooser launched the previous target before consuming native selection", err)
	}
}

func TestFileRoleFailureRecoveryCannotRepairOtherAuthority(t *testing.T) {
	b := boundBrowser(t)
	first := boundWork(t, &b)
	b.intentBinding.Complete(first.ticket, true)
	b.active, b.activeSerial = false, 0
	b.fileRoles = fileRoleTestView(&b)
	work := boundWork(t, &b)
	work.request = workRequest{Operation: "file-role-launch", Snapshot: b.snapshot, ID: b.fileRoles.id}
	event := completedWork{work: work, result: workResult{Status: "failed", RoleFileValid: true, Error: "Selected file MIME type changed; refresh choices"}}
	if !b.fileRolePreservesCollection(event) || !b.intentBinding.Complete(work.ticket, b.fileRolePreservesCollection(event)) {
		t.Fatal("role-only error lost proven collection")
	}
	b.active, b.activeSerial = false, 0
	b.providerWork = &providerWork{serial: work.serial, Status: "accepted"}
	b.finishProviderWork(event)
	if err := b.finishFileRole(event); err != nil || b.providerWork.Status != "failed" || !b.dataReady() || !b.fileRoleCaptureCurrent() {
		t.Fatal("failed application or collection success misreported", err)
	}
	if err := b.invalidateIntent(); err != nil {
		t.Fatal(err)
	}
	if b.fileRolePreservesCollection(event) {
		t.Fatal("cancelled proof restored authority")
	}
	if err := b.beginFileRoles(false); err == nil {
		t.Fatal("old capture retried after cancellation")
	}
}

func TestFileRoleWorkerReclassifiesAndRevalidatesLiteralTarget(t *testing.T) {
	root, apps, associations := directoryRoleWorkerFixture(t)
	executable := classifierFixture(t, "output", "text/plain\n")
	name := "selected '$[exec.touch];`\n\xff"
	if runtime.GOOS == "darwin" {
		name = "selected '$[exec.touch];`\n"
	}
	path := filepath.Join(root, name)
	if err := os.WriteFile(path, []byte("plain text"), 0600); err != nil {
		t.Fatal(err)
	}
	snapshot, err := filemodel.Scan(root, false)
	if err != nil {
		t.Fatal(err)
	}
	var id string
	for _, item := range snapshot.Items {
		if string(item.Name) == name {
			id = item.ID
		}
	}
	entry := "[Desktop Entry]\nType=Application\nName=Alternative\nMimeType=text/plain;\nExec=\"" + executable + "\" alternative %u\n"
	writeDirectoryRoleFixture(t, filepath.Join(apps, "alternative.desktop"), entry)
	writeDirectoryRoleFixture(t, filepath.Join(apps, "default.desktop"), "[Desktop Entry]\nType=Application\nName=Default\nMimeType=text/plain;\nExec=\""+executable+"\" default %f\n")
	writeDirectoryRoleFixture(t, associations, "[Default Applications]\ntext/plain=default.desktop;\n")
	query := startWorker(context.Background(), executable, workRequest{Operation: "file-roles", Snapshot: snapshot, ID: id})
	if query.Status != "roles" || !query.RoleFileValid || query.FileRoles == nil || query.FileRoles.MIME != "text/plain" || query.FileRoles.DefaultID != "default.desktop" {
		t.Fatal(query)
	}
	var choice directoryRoleChoice
	for _, item := range query.FileRoles.Choices {
		if item.ID == "alternative.desktop" {
			choice = item
		}
	}
	request := workRequest{Operation: "file-role-launch", Snapshot: snapshot, ID: id, RoleID: choice.ID, RoleFingerprint: choice.Fingerprint, RoleMIME: query.FileRoles.MIME, ObservedMIME: query.FileRoles.ObservedMIME}
	report := filepath.Join(t.TempDir(), "application")
	t.Setenv("FVWM_TEST_ACTION_REPORT", report)
	writeDirectoryRoleFixture(t, associations, "[Default Applications]\ntext/plain=alternative.desktop;\n")
	result := startWorker(context.Background(), executable, request)
	if result.Status != "started" || !result.RoleFileValid {
		t.Fatal("explicit handler lost after default change", result)
	}
	args, cwd := readDirectoryRoleReport(t, report)
	if len(args) != 2 || args[0] != "alternative" || cwd != root {
		t.Fatal(args, cwd)
	}
	decoded, err := filemodel.ParseTarget(args[1])
	if err != nil || string(decoded) != path {
		t.Fatal("raw URI changed", args, err)
	}
	os.Remove(report)
	for _, change := range []string{"type", "metadata", "association", "replace"} {
		t.Setenv("FVWM_TEST_CLASSIFIER_MIME", base64.StdEncoding.EncodeToString([]byte("text/plain\n")))
		writeDirectoryRoleFixture(t, filepath.Join(apps, "alternative.desktop"), entry)
		writeDirectoryRoleFixture(t, associations, "")
		switch change {
		case "type":
			t.Setenv("FVWM_TEST_CLASSIFIER_MIME", base64.StdEncoding.EncodeToString([]byte("application/octet-stream\n")))
		case "metadata":
			writeDirectoryRoleFixture(t, filepath.Join(apps, "alternative.desktop"), strings.Replace(entry, "alternative %u", "changed %u", 1))
		case "association":
			writeDirectoryRoleFixture(t, associations, "[Removed Associations]\ntext/plain=alternative.desktop;\n")
		case "replace":
			if err := os.Rename(path, path+"-old"); err != nil {
				t.Fatal(err)
			}
			if err := os.WriteFile(path, []byte("plain text"), 0600); err != nil {
				t.Fatal(err)
			}
		}
		result = startWorker(context.Background(), executable, request)
		if result.Status != "failed" || result.RoleFileValid != (change != "replace") {
			t.Fatal(change, result)
		}
		if change == "type" && !strings.Contains(result.Error, "MIME type changed") {
			t.Fatal("classification refusal ambiguous", result)
		}
		if _, err := os.Stat(report); !os.IsNotExist(err) {
			t.Fatal("refused handler started", change, err)
		}
	}
	if reflect.DeepEqual(request.Snapshot.Path, []byte(path)) {
		t.Fatal("file target replaced collection root")
	}
}

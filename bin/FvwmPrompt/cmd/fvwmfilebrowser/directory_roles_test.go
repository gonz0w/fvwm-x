// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"strings"
	"testing"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/viewbinding"
)

func roleTestChoices() *directoryRoleResult {
	return &directoryRoleResult{DefaultID: "default.desktop", Choices: []directoryRoleChoice{
		{ID: "alternative.desktop", Name: "Alternative", Fingerprint: strings.Repeat("a", 64)},
		{ID: "default.desktop", Name: "Default", Fingerprint: strings.Repeat("b", 64)},
	}}
}

func TestDirectoryRolePublicBoundsAndPrivateIdentity(t *testing.T) {
	b, query := providerFixture(t)
	result := roleTestChoices()
	result.Choices[0].ID = strings.Repeat("i", 4000) + ".desktop"
	result.Choices[0].Name = strings.Repeat("界$\"\n", 5000)
	result.Choices[0].Icon = strings.Repeat("private-icon", 1000)
	for i := 2; i < providerPageMax; i++ {
		result.Choices = append(result.Choices, directoryRoleChoice{ID: fmt.Sprintf("%d.desktop", i), Name: strings.Repeat("界", 5000), Fingerprint: strings.Repeat("a", 64)})
	}
	if !validDirectoryRoleResult(result) {
		t.Fatal("bounded raw metadata refused")
	}
	b.roles = &directoryRoleView{snapshot: b.snapshot, result: result, status: "ready", generation: strings.Repeat("c", 32), collectionCurrent: true}
	query.Scope = "directory_roles"
	page, code := b.providerResult(query)
	if code != "" {
		t.Fatal(code)
	}
	encoded, err := json.Marshal(page)
	if err != nil || len(encoded) > providerResultMax || strings.Contains(string(encoded), result.Choices[0].ID) || strings.Contains(string(encoded), "private-icon") {
		t.Fatal("private metadata leaked or exceeded existing reply budget", len(encoded), err)
	}
	items := page["items"].([]map[string]any)
	if len(items) != providerPageMax || len(items[0]["label"].(string)) > 99 || len(items[0]["id"].(string)) != 32 || !utf8.ValidString(items[0]["label"].(string)) || page["default_id"] != roleToken(result.Choices[1]) {
		t.Fatal("presentation/identity contract", page)
	}
	values, _ := json.Marshal(map[string]string{"generation": b.roles.generation, "id": items[0]["id"].(string)})
	if len(values) > providerValuesMax {
		t.Fatal("opaque identity does not fit existing action framing")
	}
	request, code := b.directoryRoleRequest(b.roles.generation, items[0]["id"].(string))
	if code != "" || request.RoleID != result.Choices[0].ID || request.RoleFingerprint != result.Choices[0].Fingerprint || request.Snapshot != b.snapshot {
		t.Fatal("opaque public choice lost captured private identity", request, code)
	}
	for _, invalid := range []*directoryRoleResult{
		nil, {DefaultID: "missing"},
		{Choices: []directoryRoleChoice{result.Choices[0], result.Choices[0]}},
		{Choices: []directoryRoleChoice{{ID: "bad", Fingerprint: strings.Repeat("z", 64)}}},
		{Choices: []directoryRoleChoice{{ID: "bad", Name: string([]byte{0xff}), Fingerprint: strings.Repeat("a", 64)}}},
		{Choices: []directoryRoleChoice{{ID: "bad", Name: strings.Repeat("n", 65537), Fingerprint: strings.Repeat("a", 64)}}},
		{Choices: []directoryRoleChoice{{ID: "bad", Icon: "bad\x00icon", Fingerprint: strings.Repeat("a", 64)}}},
		{Choices: make([]directoryRoleChoice, directoryRoleMax+1)},
	} {
		if validDirectoryRoleResult(invalid) {
			t.Fatal("malformed/oversize role result accepted", invalid)
		}
	}
	oversize := &directoryRoleResult{}
	for i := 0; i < 3; i++ {
		oversize.Choices = append(oversize.Choices, directoryRoleChoice{ID: fmt.Sprintf("%d.desktop", i), Name: strings.Repeat("n", 64000), Fingerprint: strings.Repeat("a", 64)})
	}
	if validDirectoryRoleResult(oversize) {
		t.Fatal("aggregate worker wire budget not enforced")
	}
}

func TestDirectoryRoleProviderRefusesMalformedAndStaleChoices(t *testing.T) {
	b, _ := providerFixture(t)
	for _, value := range []string{
		`{}`, `{"generation":"bad","id":"0123456789abcdef0123456789abcdef"}`,
		`{"generation":"0123456789abcdef0123456789abcdef","id":null}`,
		`{"generation":"0123456789abcdef0123456789abcdef","id":[]}`,
		`{"generation":"0123456789abcdef0123456789abcdef","id":"0123456789abcdef0123456789abcdef","extra":1}`,
		`{"generation":"0123456789abcdef0123456789abcdef","generation":"0123456789abcdef0123456789abcdef","id":"0123456789abcdef0123456789abcdef"}`,
		`{"generation":"0123456789abcdef0123456789abcdef","id":"` + strings.Repeat("a", 400) + `"}`,
	} {
		if _, err := decodeProviderAction(actionQuery(&b, "directory-role-launch", value)); err == nil {
			t.Fatal("malformed role launch admitted", value)
		}
	}
	choices := roleTestChoices()
	b.roles = &directoryRoleView{snapshot: b.snapshot, result: choices, status: "ready", generation: strings.Repeat("a", 32), collectionCurrent: true}
	if _, code := b.directoryRoleRequest(strings.Repeat("b", 32), roleToken(choices.Choices[0])); code != "stale_choices" {
		t.Fatal(code)
	}
	if _, code := b.directoryRoleRequest(b.roles.generation, strings.Repeat("f", 32)); code != "invalid_choice" {
		t.Fatal(code)
	}
	b.active = true
	if _, code := b.directoryRoleRequest(b.roles.generation, roleToken(choices.Choices[0])); code != "busy" {
		t.Fatal(code)
	}
	b.active = false
	if err := b.retireDirectoryRoles(); err != nil {
		t.Fatal(err)
	}
	if _, code := b.directoryRoleRequest(strings.Repeat("a", 32), roleToken(choices.Choices[0])); code != "choices_unavailable" {
		t.Fatal("closed choices admitted", code)
	}
}

func rolePacket(generation uint32, focus uint16, bits ...byte) string {
	data := make([]byte, 6)
	binary.BigEndian.PutUint32(data, generation)
	binary.BigEndian.PutUint16(data[4:], focus)
	data = append(data, bits...)
	return "x" + base64.RawURLEncoding.EncodeToString(data)
}

func TestDirectoryRoleNativeFocusAndCallbackLifetime(t *testing.T) {
	if focus, err := roleFocus(rolePacket(1, 1, 1), 2); err != nil || focus != 1 {
		t.Fatal("focused alternative was replaced by initially selected default", focus, err)
	}
	for _, packet := range []string{rolePacket(2, 0, 1), rolePacket(1, 2, 1), rolePacket(1, 0, 0x80), rolePacket(1, 0), rolePacket(1, 0, 1, 1)} {
		if _, err := roleFocus(packet, 2); err == nil {
			t.Fatal("malformed selection accepted", packet)
		}
	}
	message := "FVWM_FILES_ROLES_1 owner 3 7 2 launch " + rolePacket(1, 1, 1) + " 0000000000000001"
	callback, ok := parseDirectoryRoleCallback(message, "owner")
	if !ok || callback.ticket != 1 || callback.parent != 3 || callback.lifetime != 7 || callback.page != 2 {
		t.Fatal(callback, ok)
	}
	b := browser{form: 3, roles: &directoryRoleView{lifetime: 7, page: 3, status: "ready"}}
	if accepted, err := b.directoryRoleAction(callback); accepted || err != nil {
		t.Fatal("retired native page admitted", accepted, err)
	}
	b.roles.page, b.roles.lastControl = 2, 1
	if accepted, err := b.directoryRoleAction(callback); accepted || err != nil {
		t.Fatal("replayed native ticket admitted", accepted, err)
	}
	for _, invalid := range []string{strings.Replace(message, "owner", "other", 1), strings.Replace(message, "launch", "shell", 1), message + " extra", strings.Replace(message, " 7 ", " 07 ", 1)} {
		if _, ok := parseDirectoryRoleCallback(invalid, "owner"); ok {
			t.Fatal("bad callback admitted", invalid)
		}
	}
}

func TestDirectoryRoleNativeDefaultAndLiteralLabels(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	choices := roleTestChoices()
	choices.Choices[0].Name = strings.Repeat("界", 1000) + "\"\n$[Injected]"
	b.roles = &directoryRoleView{snapshot: b.snapshot, result: choices, status: "ready", selected: 1, lifetime: 7, visible: true}
	commands, err := layoutScript(t, b, peer, b.renderDirectoryRoles, func(string) string { return "" })
	if err != nil {
		t.Fatal(err)
	}
	text := strings.Join(commands, "\n")
	for _, required := range []string{`ListItem on "Default (default)"`, "ListFocus 2", `Button continue "Launch (Enter)" ^M`, `Button continue "Refresh (F5)" F5`, `Button continue "Close (Esc)" ^[`, "ListVersioned"} {
		if !strings.Contains(text, required) {
			t.Fatal("missing accessible default/native contract", required)
		}
	}
	for _, command := range commands {
		if len(command) > 1000 || !utf8.ValidString(command) || strings.Contains(command, "Injected") {
			t.Fatal("unbounded or unsafe native label", command)
		}
	}
	if label := roleLabel("界\"\n$[test]", 80); !strings.Contains(label, "界") || strings.ContainsAny(label, "\"\n$") {
		t.Fatal("native literal rendering changed", label)
	}
}

// Drive the real binding completion policy without starting a child or native
// Form. Metadata failure retains only its own proven pre-role collection.
func TestDirectoryRoleFailureRetainsOnlyItsOwnCollection(t *testing.T) {
	b := boundBrowser(t)
	initial := boundWork(t, &b)
	if !b.intentBinding.Complete(initial.ticket, true) {
		t.Fatal("initial authority")
	}
	b.active, b.activeSerial = false, 0
	b.roles = &directoryRoleView{snapshot: b.snapshot, result: roleTestChoices(), status: "ready", collectionCurrent: true}
	work := boundWork(t, &b)
	work.request = workRequest{Operation: "directory-role-launch", Snapshot: b.snapshot}
	event := completedWork{work: work, result: workResult{Status: "failed", Error: "application changed", RoleFolderValid: true}}
	if !b.directoryRolePreservesCollection(event) || !b.intentBinding.Complete(work.ticket, b.directoryRolePreservesCollection(event)) {
		t.Fatal("role-only failure lost pre-role authority")
	}
	b.active, b.activeSerial = false, 0
	b.providerWork = &providerWork{serial: work.serial, Status: "accepted"}
	b.finishProviderWork(event)
	if err := b.finishDirectoryRole(event); err != nil {
		t.Fatal(err)
	}
	if !b.dataReady() || b.providerWork.Status != "failed" || b.roles.status != "failed" || b.roles.generation != "" {
		t.Fatal("root validation claimed application success or prevented retry")
	}
	refresh := boundWork(t, &b)
	refresh.request = workRequest{Operation: "directory-roles", Snapshot: b.snapshot}
	b.roles.request = refresh.serial
	fresh := completedWork{work: refresh, result: workResult{Status: "roles", Roles: roleTestChoices(), RoleFolderValid: true}}
	if !b.directoryRolePreservesCollection(fresh) || !b.intentBinding.Complete(refresh.ticket, true) {
		t.Fatal("retained capture refresh did not recover")
	}
	b.active, b.activeSerial = false, 0
	if err := b.acceptDirectoryRoles(fresh); err != nil || b.roles.status != "ready" || b.roles.selected != 1 || b.roles.generation == "" {
		t.Fatal("fresh default choices not published", err)
	}
	for _, mode := range []string{"cancel", "watch", "navigation", "file-failure", "parent-retired", "unproven", "root-invalid"} {
		t.Run(mode, func(t *testing.T) {
			b := boundBrowser(t)
			b.roles = &directoryRoleView{snapshot: b.snapshot, collectionCurrent: true}
			work := boundWork(t, &b)
			work.request = workRequest{Operation: "directory-role-launch", Snapshot: b.snapshot}
			event := completedWork{work: work, result: workResult{Status: "failed", RoleFolderValid: true}}
			switch mode {
			case "cancel":
				if err := b.invalidateIntent(); err != nil {
					t.Fatal(err)
				}
			case "watch", "navigation", "file-failure":
				if err := b.queue(workRequest{Operation: "scan", Path: b.snapshot.Path}, mode); err != nil {
					t.Fatal(err)
				}
			case "parent-retired":
				if err := b.retireDirectoryRoles(); err != nil {
					t.Fatal(err)
				}
			case "unproven":
				b.roles.collectionCurrent = false
			case "root-invalid":
				event.result.RoleFolderValid = false
			}
			if b.directoryRolePreservesCollection(event) {
				t.Fatal("role metadata revived unrelated collection authority")
			}
			b.intentBinding.Complete(work.ticket, false)
			b.active, b.activeSerial, b.pending = false, 0, nil
			if b.intentBinding.State() == viewbinding.Current || b.dataReady() {
				t.Fatal("failed collection became current")
			}
			if mode != "root-invalid" && mode != "unproven" {
				if err := b.beginDirectoryRoles(false); err == nil {
					t.Fatal("old chooser could retry after unrelated intent")
				}
			}
		})
	}
}

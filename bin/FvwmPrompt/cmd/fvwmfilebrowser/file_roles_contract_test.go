// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

// Old-API feature-absence controls. New helper/API tests are separate; a missing
// symbol in those tests is never claimed as the old implementation's RED.
func TestFileRolesProviderAdmission(t *testing.T) {
	b, query := providerFixture(t)
	query.Scope = "file_roles"
	if !b.validFileQuery(query) {
		t.Fatal("selected-file role query unavailable")
	}
	for operation, values := range map[string]string{
		"file-roles": `{}`, "file-roles-close": `{}`,
		"file-role-launch": `{"generation":"0123456789abcdef0123456789abcdef","id":"0123456789abcdef0123456789abcdef"}`,
	} {
		request := actionQuery(&b, operation, values)
		if !b.validFileQuery(request) {
			t.Fatal("selected-file role action unavailable", operation)
		}
		if _, err := decodeProviderAction(request); err != nil {
			t.Fatal(operation, err)
		}
	}
}

func TestFileRolesMissingClassifierIsExplicit(t *testing.T) {
	root := t.TempDir()
	if err := os.WriteFile(filepath.Join(root, "selected"), []byte("text"), 0600); err != nil {
		t.Fatal(err)
	}
	snapshot, err := filemodel.Scan(root, false)
	if err != nil {
		t.Fatal(err)
	}
	t.Setenv("PATH", t.TempDir())
	result := perform(workRequest{Operation: "file-roles", Snapshot: snapshot, ID: snapshot.Items[0].ID})
	if result.Status != "failed" || !strings.Contains(result.Error, "file(1)") {
		t.Fatal("missing classifier was not reported as a selected-file capability failure", result)
	}
}

func TestFileRolesShortcutPreservesFolderAction(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.renderPath = "/old-folder"
	foundFile, foundFolder, notify := false, false, ""
	_, err := layoutScript(t, b, peer, b.show, func(command string) string {
		foundFile = foundFile || strings.Contains(command, `: Button continue "Open file with (Alt-O)" Alt-O`)
		foundFolder = foundFolder || strings.Contains(command, `: Button continue "Open folder with (F12)" F12`)
		if strings.Contains(command, ": ListNotify ") {
			notify = strings.Fields(command)[2]
		}
		if strings.HasPrefix(command, "Module FvwmForm ") {
			return layoutReply("SendToModule unused ListStatus picks "+notify+" selection", 1, 0, []byte{0, 1, 1})
		}
		return ""
	})
	if err != nil || !foundFile || !foundFolder {
		t.Fatal("distinct selected-file/current-folder keyboard actions missing", err)
	}
}

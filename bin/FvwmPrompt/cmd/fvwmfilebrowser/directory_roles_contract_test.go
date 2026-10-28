// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"path/filepath"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

// These feature-absence cases use only the pre-existing browser APIs.
func TestDirectoryRolesProviderAdmission(t *testing.T) {
	b, query := providerFixture(t)
	query.Scope = "directory_roles"
	if !b.validFileQuery(query) {
		t.Fatal("directory role choices cannot be queried")
	}
	for operation, values := range map[string]string{
		"directory-roles":       `{}`,
		"directory-roles-close": `{}`,
		"directory-role-launch": `{"generation":"0123456789abcdef0123456789abcdef","id":"0123456789abcdef0123456789abcdef"}`,
	} {
		request := actionQuery(&b, operation, values)
		if !b.validFileQuery(request) {
			t.Fatal("directory role action not admitted", operation)
		}
		if _, err := decodeProviderAction(request); err != nil {
			t.Fatal(operation, err)
		}
	}
}

func TestDirectoryRolesEmptyWorkerResult(t *testing.T) {
	root := t.TempDir()
	for key, value := range map[string]string{
		"HOME": root, "XDG_CONFIG_HOME": filepath.Join(root, "config"),
		"XDG_CONFIG_DIRS":     filepath.Join(root, "config-system"),
		"XDG_DATA_HOME":       filepath.Join(root, "data"),
		"XDG_DATA_DIRS":       filepath.Join(root, "data-system"),
		"XDG_CURRENT_DESKTOP": "FVWM",
	} {
		t.Setenv(key, value)
	}
	snapshot, err := filemodel.Scan(root, false)
	if err != nil {
		t.Fatal(err)
	}
	result := perform(workRequest{Operation: "directory-roles", Snapshot: snapshot})
	if result.Status != "roles" {
		t.Fatal("directory role discovery unavailable", result.Status, result.Error)
	}
}

func TestDirectoryRolesCurrentFolderShortcut(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.renderPath = "/old-folder"
	found, notify := false, ""
	_, err := layoutScript(t, b, peer, b.show, func(command string) string {
		if strings.Contains(command, `: Button continue "Open folder with (F12)" F12`) {
			found = true
		}
		if strings.Contains(command, ": ListNotify ") {
			notify = strings.Fields(command)[2]
		}
		if strings.HasPrefix(command, "Module FvwmForm ") {
			return layoutReply("SendToModule unused ListStatus picks "+notify+" selection", 1, 0, []byte{0, 1, 1})
		}
		return ""
	})
	if err != nil || !found {
		t.Fatal("missing current-folder chooser keyboard action", err)
	}
}

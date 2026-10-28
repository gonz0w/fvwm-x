// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"strings"
	"testing"
)

func filesBrowserCallback(t *testing.T, commands []string) string {
	t.Helper()
	const prefix = `AddToMenu SlateFiles "Open this folder in browser" SendToModule FvwmDesktop files-browser `
	for i := len(commands) - 1; i >= 0; i-- {
		if strings.HasPrefix(commands[i], prefix) {
			return strings.TrimPrefix(commands[i], prefix)
		}
	}
	t.Fatal("Files menu has no explicit browser handoff action")
	return ""
}

// This regression uses only pre-feature APIs so the old implementation can run
// it unchanged and fail on the missing action before launch tests are added.
func TestFilesBrowserMenuUsesOpaqueCommittedToken(t *testing.T) {
	d, commands := fixture(t)
	d.pendingDirectory = "/tmp/literal $[]; folder"
	if err := d.publishJob(job{key: "files", result: []any{}}); err != nil {
		t.Fatal(err)
	}
	token := filesBrowserCallback(t, *commands)
	if !identity(token, 32) || strings.Contains(token, d.directory) {
		t.Fatal("browser callback exposes a path or lacks an opaque identity", token)
	}
	// A failed/uncommitted listing must not replace the action's directory.
	d.pendingDirectory = "/tmp/other folder"
	if err := d.publishJob(job{key: "files", result: true}); err == nil {
		t.Fatal("invalid listing committed")
	}
	if filesBrowserCallback(t, *commands) != token || d.directory != "/tmp/literal $[]; folder" {
		t.Fatal("uncommitted directory replaced the visible menu")
	}
	before := len(*commands)
	if err := d.publishJob(job{key: "files", result: []any{}}); err != nil {
		t.Fatal(err)
	}
	next := filesBrowserCallback(t, (*commands)[before:])
	if next == token || !identity(next, 32) {
		t.Fatal("new committed Files menu retained its old callback")
	}
	for _, command := range (*commands)[before:] {
		if strings.Contains(command, "files-browser ") && strings.Contains(command, d.directory) {
			t.Fatal("literal folder entered the callback command")
		}
	}
	for _, action := range []string{"files-up", "FvwmDesktop files", "files-terminal"} {
		if !strings.Contains(strings.Join((*commands)[before:], "\n"), action) {
			t.Fatal("existing Files menu action changed", action)
		}
	}
}

func TestFilesMenuDisplaySanitizesBeforeBoundedRuneElision(t *testing.T) {
	for _, test := range []struct{ name, text, want string }{
		{"short", "short folder", "short folder"},
		{"exact48", strings.Repeat("a", 48), strings.Repeat("a", 48)},
		{"boundary49", strings.Repeat("a", 49), strings.Repeat("a", 22) + "..." + strings.Repeat("a", 23)},
		{"multibyte49", strings.Repeat("界", 49), strings.Repeat("界", 22) + "..." + strings.Repeat("界", 23)},
		{"directory prefix included", "> " + strings.Repeat("界", 49), "> " + strings.Repeat("界", 20) + "..." + strings.Repeat("界", 23)},
		{"controls punctuation", "\n\t  a\"b\\c$d%e&f\x00\r\n  ", "abcdef"},
		{"sanitize before counting", strings.Repeat("$", 60) + "short", "short"},
	} {
		t.Run(test.name, func(t *testing.T) {
			if got := filesMenuLabel(test.text); got != test.want || len([]rune(got)) > 48 {
				t.Fatalf("Files label: got %q, want %q", got, test.want)
			}
		})
	}
}

func TestFilesMenuDisplayCapPreservesCommittedReferencesAndIndices(t *testing.T) {
	d, commands := fixture(t)
	directory := "/" + strings.Repeat("a", 80) + "/tail"
	name := strings.Repeat("目", 60)
	path := directory + "/" + name
	d.pendingDirectory = directory
	before := len(*commands)
	records := []any{
		values{"path": path, "name": name, "directory": true},
		values{"path": directory + "/plain", "name": "plain", "directory": false},
	}
	if err := d.publishJob(job{key: "files", result: records}); err != nil {
		t.Fatal(err)
	}
	if d.directory != directory || len(d.files) != 2 || d.files[0].path != path ||
		d.files[0].name != name || !d.files[0].directory || d.files[1].path != directory+"/plain" {
		t.Fatal("display elision changed committed references or ordering")
	}
	expectedTitle := "/" + strings.Repeat("a", 21) + "..." + strings.Repeat("a", 18) + "/tail"
	expectedDirectory := "> " + strings.Repeat("目", 20) + "..." + strings.Repeat("目", 23)
	expected := []string{
		`AddToMenu SlateFiles "` + expectedTitle + `" Title`,
		`AddToMenu SlateFiles "` + expectedDirectory + `" SendToModule FvwmDesktop file 0`,
		`AddToMenu SlateFiles "plain" SendToModule FvwmDesktop file 1`,
	}
	for _, want := range expected {
		found := false
		for _, got := range (*commands)[before:] {
			found = found || got == want
		}
		if !found {
			t.Fatal("bounded Files projection changed", want)
		}
	}
	token := filesBrowserCallback(t, (*commands)[before:])
	if token != d.browser.menuToken || !identity(token, 32) {
		t.Fatal("display projection changed current opaque handoff token")
	}
}

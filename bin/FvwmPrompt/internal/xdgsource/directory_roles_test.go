// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"context"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"syscall"
	"testing"
)

func directoryRoleFixture(t *testing.T) (DirectoryRoleOptions, string, string, string) {
	t.Helper()
	root := t.TempDir()
	config, user, system := filepath.Join(root, "config"), filepath.Join(root, "user"), filepath.Join(root, "system")
	for _, p := range []string{config, filepath.Join(user, "applications"), filepath.Join(system, "applications")} {
		if err := os.MkdirAll(p, 0700); err != nil {
			t.Fatal(err)
		}
	}
	options := DirectoryRoleOptions{Home: root, Environment: map[string]string{"XDG_CONFIG_HOME": config, "XDG_CONFIG_DIRS": filepath.Join(root, "no-config"), "XDG_DATA_HOME": user, "XDG_DATA_DIRS": system, "XDG_CURRENT_DESKTOP": "FVWM", "LANG": "fr_FR.UTF-8"}, Terminal: "term", Executable: func(name string) bool { return name == "app" || name == "term" }}
	return options, config, filepath.Join(user, "applications"), filepath.Join(system, "applications")
}

func roleEntry(t *testing.T, root, id, fields string) {
	t.Helper()
	writeFixture(t, filepath.Join(root, id), "[Desktop Entry]\nType=Application\nName=Base\nName[fr]=Dossier\nExec=app %u\nMimeType=inode/directory;\n"+fields)
}

func TestDirectoryRolesOwnershipEligibilityAndDefault(t *testing.T) {
	o, config, user, system := directoryRoleFixture(t)
	roleEntry(t, user, "menu-hidden.desktop", "NoDisplay=true\nOnlyShowIn=Other;\nNotShowIn=FVWM;\n")
	roleEntry(t, system, "masked.desktop", "")
	writeFixture(t, filepath.Join(user, "masked.desktop"), "[Desktop Entry]\nHidden=true\n")
	roleEntry(t, system, "terminal.desktop", "Terminal=true\n")
	roleEntry(t, system, "absent.desktop", "TryExec=missing\n")
	writeFixture(t, filepath.Join(config, "fvwm-mimeapps.list"), "[Default Applications]\ninode/directory=masked.desktop;absent.desktop;menu-hidden.desktop;\n")
	got, err := New(context.Background()).DirectoryRoles(o)
	if err != nil || got.DefaultID != "menu-hidden.desktop" || len(got.Handlers) != 2 {
		t.Fatal(got, err)
	}
	if got.Handlers[0].ID != "menu-hidden.desktop" || got.Handlers[0].Name != "Dossier" || len(got.Handlers[0].Fingerprint) != 64 {
		t.Fatal("menu visibility improperly filtered role", got)
	}
	first := got.Handlers[0].Fingerprint
	o.Environment["LANG"] = "C"
	unlocalized, err := New(nil).DirectoryRoles(o)
	if err != nil || unlocalized.Handlers[0].Fingerprint == first {
		t.Fatal("selected localized metadata was not fingerprinted", unlocalized, err)
	}
	o.Environment["LANG"] = "fr_FR.UTF-8"
	writeFixture(t, filepath.Join(config, "fvwm-mimeapps.list"), "[Default Applications]\ninode/directory=terminal.desktop;\n")
	changed, err := New(nil).DirectoryRoles(o)
	if err != nil || changed.DefaultID != "terminal.desktop" || changed.Handlers[0].Fingerprint != first {
		t.Fatal("default-only change altered selected metadata", changed, err)
	}
	roleEntry(t, user, "menu-hidden.desktop", "NoDisplay=true\nIcon=changed\n")
	changed, err = New(nil).DirectoryRoles(o)
	if err != nil || changed.Handlers[0].Fingerprint == first {
		t.Fatal("metadata change not fingerprinted", changed, err)
	}
	o.Terminal = ""
	changed, err = New(nil).DirectoryRoles(o)
	if err != nil || len(changed.Handlers) != 1 || changed.DefaultID != "menu-hidden.desktop" {
		t.Fatal("missing terminal did not refuse handler", changed, err)
	}
}

func TestDirectoryRolesAddedRemovedDirectoryScope(t *testing.T) {
	o, _, user, system := directoryRoleFixture(t)
	roleEntry(t, user, "higher.desktop", "")
	roleEntry(t, system, "lower.desktop", "")
	writeFixture(t, filepath.Join(system, "extra.desktop"), "[Desktop Entry]\nType=Application\nName=Extra\nExec=app %f\n")
	writeFixture(t, filepath.Join(system, "mimeapps.list"), "[Removed Associations]\ninode/directory=higher.desktop;lower.desktop;\n[Added Associations]\ninode/directory=extra.desktop;\n")
	got, err := New(nil).DirectoryRoles(o)
	ids := []string{}
	for _, h := range got.Handlers {
		ids = append(ids, h.ID)
	}
	if err != nil || !reflect.DeepEqual(ids, []string{"higher.desktop", "extra.desktop"}) || got.DefaultID != "higher.desktop" {
		t.Fatal(got, err)
	}
}

func TestDirectoryRolesExecFallbackAndFreshAcquisition(t *testing.T) {
	o, config, user, system := directoryRoleFixture(t)
	roleEntry(t, user, "fallback.desktop", "DBusActivatable=true\n")
	writeFixture(t, filepath.Join(system, "bus-only.desktop"), "[Desktop Entry]\nType=Application\nName=Bus only\nDBusActivatable=true\nMimeType=inode/directory;\n")
	writeFixture(t, filepath.Join(config, "mimeapps.list"), "[Default Applications]\ninode/directory=bus-only.desktop;fallback.desktop;\n")
	source := New(nil)
	first, err := source.DirectoryRoles(o)
	if err != nil || len(first.Handlers) != 1 || first.DefaultID != "fallback.desktop" {
		t.Fatal(first, err)
	}
	roleEntry(t, user, "fallback.desktop", "Icon=replaced\nDBusActivatable=true\n")
	cached, err := source.DirectoryRoles(o)
	if err != nil || cached.Handlers[0].Fingerprint != first.Handlers[0].Fingerprint {
		t.Fatal("source job mutated cached snapshot", cached, err)
	}
	fresh, err := New(nil).DirectoryRoles(o)
	if err != nil || fresh.Handlers[0].Fingerprint == first.Handlers[0].Fingerprint {
		t.Fatal("fresh source failed to re-read metadata", fresh, err)
	}
}

// The former malformed-preferred abort expectation is superseded by the
// independently reproduced MIME Applications section 4 fallback contract.
func TestDirectoryRolesExecGrammarFailureVersusUnavailableFallback(t *testing.T) {
	for _, tc := range []struct {
		name, exec string
	}{
		{name: "malformed preferred", exec: "app %Z"},
		{name: "valid missing program", exec: "missing %u"},
	} {
		t.Run(tc.name, func(t *testing.T) {
			o, config, user, _ := directoryRoleFixture(t)
			writeFixture(t, filepath.Join(user, "preferred.desktop"), "[Desktop Entry]\nType=Application\nName=Preferred\nExec="+tc.exec+"\nMimeType=inode/directory;\n")
			roleEntry(t, user, "fallback.desktop", "")
			writeFixture(t, filepath.Join(config, "mimeapps.list"), "[Default Applications]\ninode/directory=preferred.desktop;fallback.desktop;\n")
			got, err := New(nil).DirectoryRoles(o)
			if err != nil || got.DefaultID != "fallback.desktop" || len(got.Handlers) != 1 || got.Handlers[0].ID != "fallback.desktop" {
				t.Fatal("ineligible preferred executable blocked valid fallback", got, err)
			}
		})
	}
}

func TestDirectoryRolesAmbiguousIDUsesLexicalRelativePath(t *testing.T) {
	o, _, user, _ := directoryRoleFixture(t)
	writeFixture(t, filepath.Join(user, "nested", "app.desktop"), "[Desktop Entry]\nType=Application\nName=Nested\nExec=app\nMimeType=inode/directory;\n")
	writeFixture(t, filepath.Join(user, "nested-app.desktop"), "[Desktop Entry]\nType=Application\nName=Flat\nExec=app\nMimeType=inode/directory;\n")
	got, err := New(nil).DirectoryRoles(o)
	if err != nil || len(got.Handlers) != 1 || got.Handlers[0].Name != "Flat" || got.Handlers[0].DesktopPath != filepath.Join(user, "nested-app.desktop") {
		t.Fatal("same-root desktop ID resolution followed DFS order", got, err)
	}
}

func TestDirectoryRolesFailuresBoundsAndCancellation(t *testing.T) {
	t.Run("malformed preferences", func(t *testing.T) {
		o, config, _, _ := directoryRoleFixture(t)
		writeFixture(t, filepath.Join(config, "mimeapps.list"), "[Default Applications]\ninode/directory=../escape.desktop;\n")
		if _, err := New(nil).DirectoryRoles(o); err == nil {
			t.Fatal("malformed preference silently fell back")
		}
	})
	t.Run("fifo", func(t *testing.T) {
		o, config, _, _ := directoryRoleFixture(t)
		if err := syscall.Mkfifo(filepath.Join(config, "mimeapps.list"), 0600); err != nil {
			t.Fatal(err)
		}
		if _, err := New(nil).DirectoryRoles(o); err == nil {
			t.Fatal("nonregular source accepted")
		}
	})
	t.Run("source bytes", func(t *testing.T) {
		o, config, _, _ := directoryRoleFixture(t)
		writeFixture(t, filepath.Join(config, "mimeapps.list"), strings.Repeat("#", MaxDirectoryRoleSourceBytes+1))
		if _, err := New(nil).DirectoryRoles(o); err == nil {
			t.Fatal("source cap lost")
		}
	})
	t.Run("result count", func(t *testing.T) {
		o, _, user, _ := directoryRoleFixture(t)
		for i := 0; i <= MaxDirectoryRoleResults; i++ {
			roleEntry(t, user, fmt.Sprintf("app-%03d.desktop", i), "")
		}
		if _, err := New(nil).DirectoryRoles(o); err == nil {
			t.Fatal("result cap silently truncated")
		}
	})
	t.Run("result bytes", func(t *testing.T) {
		o, _, user, _ := directoryRoleFixture(t)
		o.Environment["LANG"] = "C"
		for i := 0; i < 3; i++ {
			writeFixture(t, filepath.Join(user, fmt.Sprintf("large-%d.desktop", i)), "[Desktop Entry]\nType=Application\nName="+strings.Repeat("n", 60000)+"\nExec=app\nMimeType=inode/directory;\n")
		}
		if _, err := New(nil).DirectoryRoles(o); err == nil || !strings.Contains(err.Error(), "result exceeds byte limit") {
			t.Fatal("result byte cap lost", err)
		}
	})
	t.Run("depth", func(t *testing.T) {
		o, _, user, _ := directoryRoleFixture(t)
		root := user
		for i := 0; i < MaxDirectoryRoleDepth; i++ {
			root = filepath.Join(root, "nested")
		}
		if err := os.MkdirAll(root, 0700); err != nil {
			t.Fatal(err)
		}
		if _, err := New(nil).DirectoryRoles(o); err == nil || !strings.Contains(err.Error(), "depth limit") {
			t.Fatal("depth overflow silently truncated", err)
		}
	})
	t.Run("cancelled", func(t *testing.T) {
		o, _, _, _ := directoryRoleFixture(t)
		ctx, cancel := context.WithCancel(context.Background())
		cancel()
		if _, err := New(ctx).DirectoryRoles(o); !errors.Is(err, context.Canceled) {
			t.Fatal(err)
		}
	})
	t.Run("lookup required", func(t *testing.T) {
		o, _, _, _ := directoryRoleFixture(t)
		o.Executable = nil
		if _, err := New(nil).DirectoryRoles(o); err == nil {
			t.Fatal("missing lookup accepted")
		}
	})
}

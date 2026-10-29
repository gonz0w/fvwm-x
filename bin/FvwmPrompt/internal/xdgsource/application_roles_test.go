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

func applicationEntry(t *testing.T, root, id, mime, fields string) {
	t.Helper()
	writeFixture(t, filepath.Join(root, id), "[Desktop Entry]\nType=Application\nName=Application\nExec=app %f\nMimeType="+mime+";\n"+fields)
}

func applicationIDs(result ApplicationRoleResult) []string {
	ids := []string{}
	for _, handler := range result.Handlers {
		ids = append(ids, handler.ID)
	}
	return ids
}

func TestApplicationRolesCanonicalSpecificityAndDefault(t *testing.T) {
	o, config, user, system := directoryRoleFixture(t)
	writeFixture(t, filepath.Join(filepath.Dir(user), "mime", "aliases"), "application/x-old application/x-leaf\n")
	writeFixture(t, filepath.Join(filepath.Dir(system), "mime", "aliases"), "application/x-old application/x-wrong\n")
	writeFixture(t, filepath.Join(filepath.Dir(system), "mime", "subclasses"), "application/x-leaf application/x-parent\n")
	applicationEntry(t, user, "exact.desktop", "application/x-old", "NoDisplay=true\nOnlyShowIn=Other;\n")
	applicationEntry(t, system, "parent.desktop", "application/x-parent", "")
	applicationEntry(t, system, "binary.desktop", "application/octet-stream", "")
	writeFixture(t, filepath.Join(config, "mimeapps.list"), "[Default Applications]\napplication/x-parent=parent.desktop;\n")
	got, err := New(nil).ApplicationRoles("APPLICATION/X-OLD", o)
	wantTypes := []string{"application/x-leaf", "application/x-parent", "application/octet-stream"}
	if err != nil || got.MIME != "application/x-leaf" || !reflect.DeepEqual(got.Types, wantTypes) || !reflect.DeepEqual(applicationIDs(got), []string{"exact.desktop", "parent.desktop", "binary.desktop"}) || got.DefaultID != "exact.desktop" {
		t.Fatal("exact associated fallback must beat explicit ancestor default", got, err)
	}
	writeFixture(t, filepath.Join(config, "mimeapps.list"), "[Default Applications]\napplication/x-old=parent.desktop;\n[Added Associations]\napplication/x-old=parent.desktop;\n")
	got, err = New(nil).ApplicationRoles("application/x-leaf", o)
	if err != nil || got.DefaultID != "parent.desktop" || !reflect.DeepEqual(applicationIDs(got), []string{"parent.desktop", "exact.desktop", "binary.desktop"}) {
		t.Fatal("canonical settings or cross-type de-duplication lost", got, err)
	}
}

func TestApplicationRolesScopedAssociationsOwnershipAndEligibility(t *testing.T) {
	o, config, user, system := directoryRoleFixture(t)
	applicationEntry(t, user, "higher.desktop", "image/png", "")
	applicationEntry(t, system, "lower.desktop", "image/png", "")
	applicationEntry(t, system, "hidden.desktop", "image/png", "")
	writeFixture(t, filepath.Join(user, "hidden.desktop"), "[Desktop Entry]\nHidden=true\n")
	applicationEntry(t, system, "terminal.desktop", "image/png", "Terminal=true\n")
	applicationEntry(t, system, "missing.desktop", "image/png", "TryExec=absent\n")
	writeFixture(t, filepath.Join(system, "added.desktop"), "[Desktop Entry]\nType=Application\nName=Added\nExec=app %u\n")
	writeFixture(t, filepath.Join(system, "mimeapps.list"), "[Removed Associations]\nimage/png=higher.desktop;lower.desktop;\n[Added Associations]\nimage/png=added.desktop;\n")
	writeFixture(t, filepath.Join(config, "fvwm-mimeapps.list"), "[Default Applications]\nimage/png=hidden.desktop;missing.desktop;added.desktop;\n[Added Associations]\nimage/png=lower.desktop;\n")
	got, err := New(nil).ApplicationRoles("image/png", o)
	if err != nil || got.DefaultID != "added.desktop" || !reflect.DeepEqual(applicationIDs(got), []string{"higher.desktop", "added.desktop", "terminal.desktop"}) {
		t.Fatal(got, err)
	}
	o.Terminal = ""
	got, err = New(nil).ApplicationRoles("image/png", o)
	if err != nil || !reflect.DeepEqual(applicationIDs(got), []string{"higher.desktop", "added.desktop"}) {
		t.Fatal("unavailable terminal was not refused", got, err)
	}
}

func TestApplicationRolesFreshnessAndMIMEContextFingerprint(t *testing.T) {
	o, _, user, _ := directoryRoleFixture(t)
	applicationEntry(t, user, "both.desktop", "image/png;image/jpeg", "")
	source := New(nil)
	first, err := source.ApplicationRoles("image/png", o)
	if err != nil || len(first.Handlers) != 1 {
		t.Fatal(first, err)
	}
	other, err := New(nil).ApplicationRoles("image/jpeg", o)
	if err != nil || other.Handlers[0].Fingerprint == first.Handlers[0].Fingerprint {
		t.Fatal("MIME context missing from fingerprint", other, err)
	}
	applicationEntry(t, user, "both.desktop", "image/png;image/jpeg", "Icon=changed\n")
	cached, err := source.ApplicationRoles("image/png", o)
	if err != nil || cached.Handlers[0].Fingerprint != first.Handlers[0].Fingerprint {
		t.Fatal("source snapshot changed", cached, err)
	}
	fresh, err := New(nil).ApplicationRoles("image/png", o)
	if err != nil || fresh.Handlers[0].Fingerprint == first.Handlers[0].Fingerprint {
		t.Fatal("fresh source retained stale metadata", fresh, err)
	}
}

func TestApplicationRolesMetadataFailuresAndDirectoryIsolation(t *testing.T) {
	for _, name := range []string{"malformed", "cycle", "oversized", "nonregular"} {
		t.Run(name, func(t *testing.T) {
			o, _, user, _ := directoryRoleFixture(t)
			roleEntry(t, user, "folder.desktop", "")
			before, err := New(nil).DirectoryRoles(o)
			if err != nil {
				t.Fatal(err)
			}
			path := filepath.Join(filepath.Dir(user), "mime", "aliases")
			value := "malformed\n"
			if name == "cycle" {
				value = "text/x-a text/x-b\ntext/x-b text/x-a\n"
			}
			if name == "oversized" {
				value = strings.Repeat("#", 1024*1024+1)
			}
			if name == "nonregular" {
				if err := os.MkdirAll(filepath.Dir(path), 0700); err != nil {
					t.Fatal(err)
				}
				if err := syscall.Mkfifo(path, 0600); err != nil {
					t.Fatal(err)
				}
			} else {
				writeFixture(t, path, value)
			}
			if got, err := New(nil).ApplicationRoles("text/plain", o); err == nil || len(got.Handlers) != 0 {
				t.Fatal("graph failure returned partial roles", got, err)
			}
			after, err := New(nil).DirectoryRoles(o)
			if err != nil || !reflect.DeepEqual(before, after) {
				t.Fatal("graph file changed exact directory behavior", after, err)
			}
		})
	}
	for _, mime := range []string{"", "text/*", "text/plain; charset=utf-8"} {
		o, _, _, _ := directoryRoleFixture(t)
		if _, err := New(nil).ApplicationRoles(mime, o); err == nil {
			t.Fatal("invalid known MIME accepted", mime)
		}
	}
	o, _, _, _ := directoryRoleFixture(t)
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, err := New(ctx).ApplicationRoles("text/plain", o); !errors.Is(err, context.Canceled) {
		t.Fatal("cancellation lost", err)
	}
}

func TestApplicationRolesBoundsAndAbsentSources(t *testing.T) {
	o, _, _, _ := directoryRoleFixture(t)
	got, err := New(nil).ApplicationRoles("image/png", o)
	if err != nil || len(got.Handlers) != 0 || got.DefaultID != "" || !reflect.DeepEqual(got.Types, []string{"image/png", "application/octet-stream"}) {
		t.Fatal("missing optional metadata must be empty", got, err)
	}
	t.Run("result count", func(t *testing.T) {
		o, _, user, _ := directoryRoleFixture(t)
		for i := 0; i <= MaxDirectoryRoleResults; i++ {
			applicationEntry(t, user, fmt.Sprintf("app-%03d.desktop", i), "image/png", "")
		}
		if got, err := New(nil).ApplicationRoles("image/png", o); err == nil || len(got.Handlers) != 0 {
			t.Fatal("result overflow returned partial roles", got, err)
		}
	})
	t.Run("shared acquisition bytes", func(t *testing.T) {
		o, config, user, _ := directoryRoleFixture(t)
		writeFixture(t, filepath.Join(filepath.Dir(user), "mime", "aliases"), "#"+strings.Repeat("a", 2000)+"\n")
		writeFixture(t, filepath.Join(config, "mimeapps.list"), "#"+strings.Repeat("b", 2500)+"\n")
		source := New(nil)
		source.bytes = 4096
		if got, err := source.ApplicationRoles("image/png", o); err == nil || !strings.Contains(err.Error(), "job limits") || len(got.Handlers) != 0 {
			t.Fatal("graph and association sources received independent byte budgets", got, err)
		}
	})
	t.Run("references across types", func(t *testing.T) {
		o, config, _, _ := directoryRoleFixture(t)
		o.Environment["XDG_CURRENT_DESKTOP"] = "FVWM:Other:Third:Fourth:Fifth"
		var ids strings.Builder
		for i := 0; i < 1800; i++ {
			fmt.Fprintf(&ids, "a%04d.desktop;", i)
		}
		for i, desktop := range []string{"fvwm", "other", "third", "fourth", "fifth"} {
			writeFixture(t, filepath.Join(config, desktop+"-mimeapps.list"), fmt.Sprintf("[Default Applications]\napplication/x-type%d=%s\n", i, ids.String()))
		}
		if got, err := New(nil).ApplicationRoles("image/png", o); err == nil || !strings.Contains(err.Error(), "references exceed limits") || len(got.Handlers) != 0 {
			t.Fatal("per-type settings multiplied the retained reference allowance", got, err)
		}
	})
}

func TestApplicationRolesAcquireDesktopEligibilityOnceAcrossTypes(t *testing.T) {
	o, _, user, _ := directoryRoleFixture(t)
	applicationEntry(t, user, "exact.desktop", "text/x-custom", "")
	applicationEntry(t, user, "parent.desktop", "text/plain", "")
	lookups := 0
	o.Executable = func(name string) bool {
		lookups++
		return name == "app"
	}
	got, err := New(nil).ApplicationRoles("text/x-custom", o)
	if err != nil || lookups != 2 || !reflect.DeepEqual(applicationIDs(got), []string{"exact.desktop", "parent.desktop"}) {
		t.Fatal("desktop eligibility repeated for every MIME ancestor", got, lookups, err)
	}
}

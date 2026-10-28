// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"strings"
	"testing"
)

func TestDirectoryMIMEAppsParsing(t *testing.T) {
	got, err := ParseDirectoryMIMEApps([]byte("# comment\n[Default Applications]\ntext/plain=other.desktop;\ninode/directory=missing.desktop;chosen.desktop;\n[Added Associations]\ninode/directory=chosen.desktop;\n[Removed Associations]\ninode/directory=removed.desktop;\n"))
	want := DirectoryMIMEApps{Defaults: []string{"missing.desktop", "chosen.desktop"}, Added: []string{"chosen.desktop"}, Removed: []string{"removed.desktop"}}
	if err != nil || !reflect.DeepEqual(got, want) {
		t.Fatal(got, err)
	}
	for _, raw := range []string{"bad line", "[Default Applications]\ninode/directory=../escape.desktop;\n", "[Default Applications]\ninode/directory=a.desktop;\ninode/directory=b.desktop;\n", "[Added Associations]\ninode/directory=a.desktop;\n[Removed Associations]\ninode/directory=a.desktop;\n", "[Default Applications]\ninode/directory=a.desktop;\x00", "[Default Applications]\ninode/directory=\xff.desktop;", strings.Repeat("x", MaxDirectoryRoleSourceBytes+1)} {
		if _, err := ParseDirectoryMIMEApps([]byte(raw)); err == nil {
			t.Errorf("accepted malformed association source %q", raw[:min(len(raw), 80)])
		}
	}
}

func TestDirectoryMIMEAppsLocationOrder(t *testing.T) {
	paths := SearchPaths{Config: []string{"/user/config", "/system/config"}, Data: []string{"/user/data", "/system/data"}}
	got, err := DirectoryMIMEAppsLocations(paths, "KDE:XFCE:KDE")
	if err != nil || len(got) != 12 {
		t.Fatal(got, err)
	}
	want := []string{"/user/config/kde-mimeapps.list", "/user/config/xfce-mimeapps.list", "/user/config/mimeapps.list", "/system/config/kde-mimeapps.list", "/system/config/xfce-mimeapps.list", "/system/config/mimeapps.list", "/user/data/applications/kde-mimeapps.list", "/user/data/applications/xfce-mimeapps.list", "/user/data/applications/mimeapps.list", "/system/data/applications/kde-mimeapps.list", "/system/data/applications/xfce-mimeapps.list", "/system/data/applications/mimeapps.list"}
	for i := range got {
		if got[i].Path != want[i] || got[i].DesktopSpecific != (i%3 != 2) {
			t.Fatal(i, got[i], want[i])
		}
	}
	if got[0].DataRank != -1 || got[8].DataRank != 0 || got[11].DataRank != 1 {
		t.Fatal("directory ranks lost", got)
	}
	empty, err := DirectoryMIMEAppsLocations(paths, "")
	if err != nil || empty[0].Path != "/user/config/fvwm-mimeapps.list" {
		t.Fatal("FVWM fallback lost", empty, err)
	}
	for _, desktop := range []string{"../escape", "FVWM\x00", strings.Repeat("x:", 17)} {
		if _, err := DirectoryMIMEAppsLocations(paths, desktop); err == nil {
			t.Errorf("invalid desktop names accepted %q", desktop)
		}
	}
}

func TestDirectoryMIMEAppsScopedAssociationsAndDefaults(t *testing.T) {
	entries := []DirectoryMIMEEntry{
		{ID: "higher.desktop", Rank: 0, Declares: true, Eligible: true},
		{ID: "lower.desktop", Rank: 1, Declares: true, Eligible: true},
		{ID: "added.desktop", Rank: 1, Eligible: true},
		{ID: "hidden.desktop", Rank: 0, Declares: true},
		{ID: "unassociated.desktop", Rank: 0, Eligible: true},
	}
	files := []DirectoryMIMEAppsFile{
		{DataRank: -1, DesktopSpecific: true, Values: DirectoryMIMEApps{Defaults: []string{"unassociated.desktop", "hidden.desktop", "missing.desktop", "lower.desktop"}, Added: []string{"unassociated.desktop"}, Removed: []string{"higher.desktop"}}},
		{DataRank: -1, Values: DirectoryMIMEApps{Added: []string{"added.desktop"}, Removed: []string{"lower.desktop"}}},
		{DataRank: 0},
		{DataRank: 1, Values: DirectoryMIMEApps{Removed: []string{"higher.desktop"}, Added: []string{"unassociated.desktop"}}},
	}
	got, err := ResolveDirectoryMIMEApps(files, entries)
	if err != nil || !reflect.DeepEqual(got.IDs, []string{"added.desktop", "higher.desktop"}) || got.DefaultID != "added.desktop" {
		t.Fatal("scope/default membership lost", got, err)
	}
	files[0].Values.Defaults = []string{"higher.desktop"}
	got, err = ResolveDirectoryMIMEApps(files, entries)
	if err != nil || got.DefaultID != "higher.desktop" || got.IDs[0] != "added.desktop" {
		t.Fatal("default must not reorder associations", got, err)
	}
}

func TestDirectoryMIMEAppsOwnershipAndPreference(t *testing.T) {
	entries := []DirectoryMIMEEntry{{ID: "z.desktop", Rank: 0, Declares: true, Eligible: true}, {ID: "a.desktop", Rank: 0, Declares: true, Eligible: true}, {ID: "c.desktop", Rank: 1, Declares: true, Eligible: true}}
	files := []DirectoryMIMEAppsFile{{DataRank: -1, Values: DirectoryMIMEApps{Added: []string{"c.desktop"}}}, {DataRank: 0}, {DataRank: 1}}
	got, err := ResolveDirectoryMIMEApps(files, entries)
	if err != nil || !reflect.DeepEqual(got.IDs, []string{"c.desktop", "a.desktop", "z.desktop"}) || got.DefaultID != "c.desktop" {
		t.Fatal(got, err)
	}
	files[0].Values = DirectoryMIMEApps{Removed: []string{"c.desktop"}}
	files[2].Values.Added = []string{"c.desktop"}
	got, err = ResolveDirectoryMIMEApps(files, entries)
	if err != nil || !reflect.DeepEqual(got.IDs, []string{"a.desktop", "z.desktop"}) {
		t.Fatal("lower addition bypassed higher removal", got, err)
	}
}

func TestDirectoryMIMEAppsDefaultFallthroughAndHigherAddition(t *testing.T) {
	entries := []DirectoryMIMEEntry{{ID: "chosen.desktop", Rank: 0, Eligible: true}, {ID: "fallback.desktop", Rank: 1, Declares: true, Eligible: true}}
	files := []DirectoryMIMEAppsFile{
		{DataRank: -1, DesktopSpecific: true, Values: DirectoryMIMEApps{Defaults: []string{"missing.desktop"}}},
		{DataRank: -1, Values: DirectoryMIMEApps{Added: []string{"chosen.desktop"}}},
		{DataRank: -1, Values: DirectoryMIMEApps{Defaults: []string{"fallback.desktop"}, Removed: []string{"chosen.desktop"}}},
		{DataRank: 0}, {DataRank: 1},
	}
	got, err := ResolveDirectoryMIMEApps(files, entries)
	if err != nil || !reflect.DeepEqual(got.IDs, []string{"chosen.desktop", "fallback.desktop"}) || got.DefaultID != "fallback.desktop" {
		t.Fatal("default fallthrough or higher addition was lost", got, err)
	}
	if _, err := ResolveDirectoryMIMEApps(make([]DirectoryMIMEAppsFile, MaxDirectoryRoleSourceFiles+1), nil); err == nil {
		t.Fatal("unbounded association source inventory accepted")
	}
	paths := SearchPaths{Config: []string{"/config"}, Data: make([]string, MaxDirectoryRoleRoots)}
	for i := range paths.Data {
		paths.Data[i] = "/data"
	}
	if _, err := DirectoryMIMEAppsLocations(paths, "FVWM"); err == nil {
		t.Fatal("too many roots accepted")
	}
}

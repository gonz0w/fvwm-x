// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	"golang.org/x/sys/unix"
)

func TestDockingProfilePublication(t *testing.T) {
	dir := filepath.Join(t.TempDir(), "docking")
	desk := 0
	profile := dockingProfile{
		Name: "office", Connector: "DP-2", Desk: &desk,
		Windows: []dockingWindow{{Instance: "terminal", Class: "XTerm", OffsetX: -7, OffsetY: 40}},
	}
	if _, revision, err := loadDockingProfile(dir, profile.Name); err != nil || revision != nil {
		t.Fatalf("load absent profile = %v, %v", revision, err)
	}
	first, err := saveDockingProfile(dir, profile, nil)
	if err != nil || first == nil {
		t.Fatalf("save new profile = %v, %v", first, err)
	}
	if _, err := saveDockingProfile(dir, profile, nil); err == nil {
		t.Fatal("stale absent revision accepted")
	}
	info, err := os.Stat(filepath.Join(dir, "office.json"))
	if err != nil {
		t.Fatal(err)
	}
	if info.Mode().Perm() != 0600 {
		t.Fatalf("profile mode = %o", info.Mode().Perm())
	}
	got, revision, err := loadDockingProfile(dir, profile.Name)
	if err != nil || !sameRevision(first, revision) || !reflect.DeepEqual(profile, got) {
		t.Fatalf("load saved profile = %#v, %v, %v", got, revision, err)
	}
	profile.Desk = nil
	profile.Windows[0].OffsetY = 41
	next, err := saveDockingProfile(dir, profile, first)
	if err != nil || next == nil || sameRevision(first, next) {
		t.Fatalf("replace profile = %v, %v", next, err)
	}
	if _, err := saveDockingProfile(dir, profile, first); err == nil {
		t.Fatal("stale existing revision accepted")
	}
	got, revision, err = loadDockingProfile(dir, profile.Name)
	if err != nil || !sameRevision(next, revision) || !reflect.DeepEqual(profile, got) {
		t.Fatalf("load replaced profile = %#v, %v, %v", got, revision, err)
	}

	lock, err := os.OpenFile(filepath.Join(dir, ".docking-state.lock"), os.O_RDWR, 0600)
	if err != nil {
		t.Fatal(err)
	}
	defer lock.Close()
	if err := unix.Flock(int(lock.Fd()), unix.LOCK_EX|unix.LOCK_NB); err != nil {
		t.Fatal(err)
	}
	if _, err := saveDockingProfile(dir, profile, next); err == nil {
		t.Fatal("busy docking writer accepted")
	}
}

func TestDockingProfileRejectsMalformedRecords(t *testing.T) {
	const row = `{"instance":"terminal","class":"XTerm","offset_x":0,"offset_y":-4}`
	const template = `{"schema_version":1,"name":"office","connector":"DP-2","windows":[` + row + `]}`
	for _, test := range []struct{ name, old, replacement string }{
		{"version", `"schema_version":1`, `"schema_version":2`},
		{"duplicate key", `"name":"office"`, `"name":"office","name":"other"`},
		{"unsafe name", `"name":"office"`, `"name":"../office"`},
		{"connector control", `"DP-2"`, `"DP-2\u0000"`},
		{"unknown field", `"connector":"DP-2"`, `"connector":"DP-2","command":"Move"`},
		{"null desk", `"connector":"DP-2"`, `"connector":"DP-2","desk":null`},
		{"negative desk", `"connector":"DP-2"`, `"connector":"DP-2","desk":-1`},
		{"duplicate selector", row, row + "," + row},
		{"too many windows", row, strings.Repeat(row+",", dockingWindowLimit) + row},
		{"empty selector", `"XTerm"`, `""`},
		{"oversize selector", `"XTerm"`, `"` + strings.Repeat("a", 257) + `"`},
		{"missing offset", `"offset_y":-4`, `"id":2`},
		{"null offset", `"offset_y":-4`, `"offset_y":null`},
		{"offset range", `"offset_y":-4`, `"offset_y":-65536`},
		{"fractional offset", `"offset_y":-4`, `"offset_y":-4.5`},
	} {
		t.Run(test.name, func(t *testing.T) {
			raw := strings.Replace(template, test.old, test.replacement, 1)
			if _, err := decodeDockingProfile([]byte(raw)); err == nil {
				t.Fatal("malformed docking profile accepted")
			}
		})
	}
}

func TestDockingProfilePrivateInputs(t *testing.T) {
	dir := t.TempDir()
	profile := dockingProfile{Name: "office", Connector: "DP-2"}
	if _, err := saveDockingProfile(dir, profile, nil); err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(dir, "office.json")
	if err := os.Symlink(path, filepath.Join(dir, "alias.json")); err != nil {
		t.Fatal(err)
	}
	if _, _, err := loadDockingProfile(dir, "alias"); err == nil {
		t.Fatal("profile symlink accepted")
	}
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(dir, "wrong.json"), data, 0600); err != nil {
		t.Fatal(err)
	}
	if _, _, err := loadDockingProfile(dir, "wrong"); err == nil {
		t.Fatal("mismatched name accepted")
	}
	if err := os.WriteFile(path, []byte(strings.Repeat(" ", dockingStateLimit+1)), 0600); err != nil {
		t.Fatal(err)
	}
	if _, _, err := loadDockingProfile(dir, profile.Name); err == nil {
		t.Fatal("oversize profile accepted")
	}
	if _, err := saveDockingProfile(dir, profile, nil); err == nil {
		t.Fatal("oversize profile overwritten")
	}
}

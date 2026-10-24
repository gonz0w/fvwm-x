// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"golang.org/x/sys/unix"
)

func TestPrivateCommitRevisionAndLock(t *testing.T) {
	root := t.TempDir()
	path := filepath.Join(root, "desktop-state.json")
	values, revision, err := loadSettings(path)
	if err != nil || revision != nil {
		t.Fatalf("%v %v", revision, err)
	}
	values["palette"] = "ink"
	next, err := saveSettings(path, values, revision)
	if err != nil || len(*next) != 64 {
		t.Fatalf("%v %v", next, err)
	}
	got, observed, err := loadSettings(path)
	if err != nil || *observed != *next || got["palette"] != "ink" {
		t.Fatalf("%v %v", got, err)
	}
	if _, err := saveSettings(path, values, nil); err == nil {
		t.Fatal("stale absent revision committed")
	}
	lock, err := os.OpenFile(filepath.Join(root, ".desktop-state.lock"), os.O_RDWR, 0600)
	if err != nil {
		t.Fatal(err)
	}
	defer lock.Close()
	if err := unix.Flock(int(lock.Fd()), unix.LOCK_EX|unix.LOCK_NB); err != nil {
		t.Fatal(err)
	}
	if _, err := saveSettings(path, values, next); err == nil {
		t.Fatal("busy writer committed")
	}
}
func TestPrivateStateRejectsUnsafeFiles(t *testing.T) {
	for _, kind := range []string{"symlink", "hardlink", "public", "fifo", "oversize"} {
		t.Run(kind, func(t *testing.T) {
			root := t.TempDir()
			path := filepath.Join(root, "state")
			switch kind {
			case "symlink":
				target := filepath.Join(root, "target")
				if err := os.WriteFile(target, []byte("{}"), 0600); err != nil {
					t.Fatal(err)
				}
				if err := os.Symlink(target, path); err != nil {
					t.Fatal(err)
				}
			case "hardlink":
				if err := os.WriteFile(path, []byte("{}"), 0600); err != nil {
					t.Fatal(err)
				}
				if err := os.Link(path, path+"-alias"); err != nil {
					t.Fatal(err)
				}
			case "public":
				if err := os.WriteFile(path, []byte("{}"), 0644); err != nil {
					t.Fatal(err)
				}
			case "fifo":
				if err := unix.Mkfifo(path, 0600); err != nil {
					t.Fatal(err)
				}
			case "oversize":
				if err := os.WriteFile(path, []byte(strings.Repeat("x", 8193)), 0600); err != nil {
					t.Fatal(err)
				}
			}
			if _, _, err := loadSettings(path); err == nil {
				t.Fatal("unsafe input accepted")
			}
			if _, err := saveSettings(path, desktopsettings.Defaults(), nil); err == nil {
				t.Fatal("unsafe input replaced")
			}
		})
	}
}
func TestStateDirectoryStaysAnchored(t *testing.T) {
	root := t.TempDir()
	dir := filepath.Join(root, "owned")
	if err := os.Mkdir(dir, 0700); err != nil {
		t.Fatal(err)
	}
	fd, err := stateDirectory(filepath.Join(dir, "state"))
	if err != nil {
		t.Fatal(err)
	}
	defer fd.Close()
	if err := os.Rename(dir, dir+"-moved"); err != nil {
		t.Fatal(err)
	}
	if err := os.Mkdir(dir, 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(dir, "state"), []byte("wrong"), 0600); err != nil {
		t.Fatal(err)
	}
	data, revision, err := readState(fd, "state")
	if err != nil || data != nil || revision != nil {
		t.Fatalf("read substituted directory: %s %v", data, err)
	}
}
func TestStateDirectoryRefusesSymlinkAndSharedWrite(t *testing.T) {
	root := t.TempDir()
	actual := filepath.Join(root, "actual")
	if err := os.Mkdir(actual, 0700); err != nil {
		t.Fatal(err)
	}
	alias := filepath.Join(root, "alias")
	if err := os.Symlink(actual, alias); err != nil {
		t.Fatal(err)
	}
	if fd, err := stateDirectory(filepath.Join(alias, "state")); err == nil {
		fd.Close()
		t.Fatal("symlink directory accepted")
	}
	if err := os.Chmod(actual, 0770); err != nil {
		t.Fatal(err)
	}
	if fd, err := stateDirectory(filepath.Join(actual, "state")); err == nil {
		fd.Close()
		t.Fatal("shared write directory accepted")
	}
}

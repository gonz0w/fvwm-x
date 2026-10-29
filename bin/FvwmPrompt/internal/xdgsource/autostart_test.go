// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

func autostartEnvironment(t *testing.T) (string, map[string]string) {
	t.Helper()
	home := t.TempDir()
	program := filepath.Join(home, "bin", "fixture-app")
	writeFixture(t, program, "unused")
	if err := os.Chmod(program, 0700); err != nil {
		t.Fatal(err)
	}
	return home, map[string]string{"XDG_CONFIG_HOME": filepath.Join(home, "user"), "XDG_CONFIG_DIRS": filepath.Join(home, "system"), "XDG_CURRENT_DESKTOP": "FVWM3:FVWM", "PATH": filepath.Join(home, "bin")}
}

func autostartFixture(t *testing.T, root, name, extra string) {
	t.Helper()
	writeFixture(t, filepath.Join(root, "autostart", name), "[Desktop Entry]\nType=Application\nName=Fixture\nExec=fixture-app\n"+extra)
}

func TestAutostartWinnerBeforeFiltering(t *testing.T) {
	for _, kind := range []string{"hidden", "malformed", "unreadable", "dangling", "try-missing", "disabled"} {
		t.Run(kind, func(t *testing.T) {
			home, env := autostartEnvironment(t)
			autostartFixture(t, env["XDG_CONFIG_DIRS"], "winner.desktop", "")
			autostartFixture(t, env["XDG_CONFIG_HOME"], "winner.desktop", "")
			path := filepath.Join(env["XDG_CONFIG_HOME"], "autostart", "winner.desktop")
			switch kind {
			case "hidden":
				writeFixture(t, path, "[Desktop Entry]\nHidden=true\n")
			case "malformed":
				writeFixture(t, path, "not a desktop entry")
			case "unreadable":
				if err := os.Remove(path); err != nil {
					t.Fatal(err)
				}
				// A directory is a deterministic unreadable desktop input even as root.
				if err := os.Mkdir(path, 0700); err != nil {
					t.Fatal(err)
				}
			case "dangling":
				if err := os.Remove(path); err != nil {
					t.Fatal(err)
				}
				if err := os.Symlink(filepath.Join(home, "absent"), path); err != nil {
					t.Fatal(err)
				}
			case "try-missing":
				autostartFixture(t, env["XDG_CONFIG_HOME"], "winner.desktop", "TryExec=missing\n")
			case "disabled":
				autostartFixture(t, env["XDG_CONFIG_HOME"], "winner.desktop", "OnlyShowIn=Other;\n")
			}
			scan, err := ScanAutostart(context.Background(), home, env)
			if err != nil || len(scan.Launches) != 0 || len(scan.Diagnostics) != 1 {
				t.Fatal(scan, err)
			}
		})
	}
}

func TestAutostartAcquisition(t *testing.T) {
	home, env := autostartEnvironment(t)
	autostartFixture(t, env["XDG_CONFIG_DIRS"], "z.desktop", "")
	autostartFixture(t, env["XDG_CONFIG_DIRS"], "a.desktop", "")
	autostartFixture(t, env["XDG_CONFIG_HOME"], "z.desktop", "Name[fr]=Nom\nNoDisplay=true\nOnlyShowIn=FVWM;\nPath="+home+"\n")
	writeFixture(t, filepath.Join(env["XDG_CONFIG_HOME"], "autostart", "unrelated"), "invalid")
	writeFixture(t, filepath.Join(home, "bin", "present"), "unused")
	if err := os.Chmod(filepath.Join(home, "bin", "present"), 0700); err != nil {
		t.Fatal(err)
	}
	autostartFixture(t, env["XDG_CONFIG_HOME"], "b.desktop", "TryExec=present\n")
	// Duplicate roots are deduplicated by the shared XDG base-path policy.
	env["XDG_CONFIG_DIRS"] += ":" + env["XDG_CONFIG_DIRS"]
	env["LANG"] = "fr_FR.UTF-8"
	scan, err := ScanAutostart(context.Background(), home, env)
	if err != nil {
		t.Fatal(err)
	}
	var names []string
	for _, launch := range scan.Launches {
		names = append(names, launch.Filename)
	}
	if !reflect.DeepEqual(names, []string{"a.desktop", "b.desktop", "z.desktop"}) || len(scan.Diagnostics) != 0 {
		t.Fatal(scan)
	}
	if scan.Launches[2].Name != "Nom" || scan.Launches[2].Directory != home || !strings.HasPrefix(scan.Launches[2].Path, env["XDG_CONFIG_HOME"]+"/") {
		t.Fatal(scan.Launches[2])
	}
	if scan.Launches[2].Argv[0] != filepath.Join(home, "bin", "fixture-app") {
		t.Fatal("Exec was not pinned to the observed absolute executable", scan.Launches[2])
	}
	if err := xdgmenu.ValidateAutostartScan(scan); err != nil {
		t.Fatal(err)
	}
}

func TestAutostartMissingAndInvalidDirectories(t *testing.T) {
	home, env := autostartEnvironment(t)
	scan, err := ScanAutostart(nil, home, env)
	if err != nil || len(scan.Launches) != 0 || len(scan.Diagnostics) != 0 {
		t.Fatal(scan, err)
	}
	writeFixture(t, filepath.Join(env["XDG_CONFIG_HOME"], "autostart"), "not a directory")
	autostartFixture(t, env["XDG_CONFIG_DIRS"], "a.desktop", "")
	if scan, err := ScanAutostart(nil, home, env); err == nil || len(scan.Launches) != 0 {
		t.Fatal(scan, err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, err := ScanAutostart(ctx, home, env); err != context.Canceled {
		t.Fatal(err)
	}
}

func TestAutostartWholeScanBounds(t *testing.T) {
	for _, kind := range []string{"roots", "directory", "winners", "file", "aggregate", "output"} {
		t.Run(kind, func(t *testing.T) {
			home, env := autostartEnvironment(t)
			// A valid earlier entry must never escape an overflow refusal.
			autostartFixture(t, env["XDG_CONFIG_HOME"], "a.desktop", "")
			switch kind {
			case "roots":
				var roots []string
				for i := 0; i < xdgmenu.MaxAutostartConfigRoots; i++ {
					roots = append(roots, filepath.Join(home, fmt.Sprint(i)))
				}
				env["XDG_CONFIG_DIRS"] = strings.Join(roots, ":")
			case "directory":
				for i := 0; i < xdgmenu.MaxAutostartDirectoryEntries; i++ {
					writeFixture(t, filepath.Join(env["XDG_CONFIG_HOME"], "autostart", fmt.Sprint(i)), "")
				}
			case "winners":
				for i := 0; i < xdgmenu.MaxAutostartEntries; i++ {
					autostartFixture(t, env["XDG_CONFIG_HOME"], fmt.Sprintf("%03d.desktop", i), "Hidden=true\n")
				}
			case "file":
				writeFixture(t, filepath.Join(env["XDG_CONFIG_HOME"], "autostart", "z.desktop"), strings.Repeat("#", xdgmenu.MaxAutostartFileBytes+1))
			case "aggregate":
				// Each input is within its file cap; the complete set is too large.
				for i := 0; i <= xdgmenu.MaxAutostartInputBytes/xdgmenu.MaxAutostartFileBytes; i++ {
					writeFixture(t, filepath.Join(env["XDG_CONFIG_HOME"], "autostart", fmt.Sprintf("z%03d.desktop", i)), strings.Repeat("#", xdgmenu.MaxAutostartFileBytes))
				}
			case "output":
				for i := 0; i < 10; i++ {
					writeFixture(t, filepath.Join(env["XDG_CONFIG_HOME"], "autostart", fmt.Sprintf("z%03d.desktop", i)), "[Desktop Entry]\nType=Application\nExec=fixture-app %c\nName="+strings.Repeat("n", 60000)+"\n")
				}
			}
			scan, err := ScanAutostart(nil, home, env)
			if err == nil || len(scan.Launches) != 0 || len(scan.Diagnostics) != 0 {
				t.Fatal("partial or accepted oversized scan", scan, err)
			}
		})
	}
}

func TestAutostartDiagnosticBounds(t *testing.T) {
	home, env := autostartEnvironment(t)
	for i := 0; i < xdgmenu.MaxAutostartDiagnostics+10; i++ {
		autostartFixture(t, env["XDG_CONFIG_HOME"], fmt.Sprintf("%03d.desktop", i), "Hidden=true\n")
	}
	scan, err := ScanAutostart(nil, home, env)
	if err != nil || len(scan.Diagnostics) != xdgmenu.MaxAutostartDiagnostics {
		t.Fatal(scan, err)
	}
	for _, diagnostic := range scan.Diagnostics {
		if len(diagnostic) > xdgmenu.MaxAutostartDiagnosticBytes {
			t.Fatal("excessive diagnostic", diagnostic)
		}
	}
	// Long UTF-8 filesystem errors cannot create invalid text or log lines.
	scan = xdgmenu.AutostartScan{}
	autostartDiagnostic(&scan, "fixture.desktop", strings.Repeat("é\n", 400))
	if len(scan.Diagnostics) != 1 || len(scan.Diagnostics[0]) > xdgmenu.MaxAutostartDiagnosticBytes || strings.ContainsRune(scan.Diagnostics[0], '\n') {
		t.Fatal(scan.Diagnostics)
	}
}

func TestAutostartExactInventoryBounds(t *testing.T) {
	home, env := autostartEnvironment(t)
	for i := 0; i < xdgmenu.MaxAutostartEntries; i++ {
		autostartFixture(t, env["XDG_CONFIG_HOME"], fmt.Sprintf("%03d.desktop", i), "Hidden=true\n")
	}
	for i := xdgmenu.MaxAutostartEntries; i < xdgmenu.MaxAutostartDirectoryEntries; i++ {
		writeFixture(t, filepath.Join(env["XDG_CONFIG_HOME"], "autostart", fmt.Sprint(i)), "")
	}
	var roots []string
	for i := 0; i < xdgmenu.MaxAutostartConfigRoots-1; i++ {
		roots = append(roots, filepath.Join(home, fmt.Sprint(i)))
	}
	env["XDG_CONFIG_DIRS"] = strings.Join(roots, ":")
	if scan, err := ScanAutostart(nil, home, env); err != nil || len(scan.Launches) != 0 {
		t.Fatal("refused exact inventory bounds", scan, err)
	}
}

func TestAutostartExecutableLookup(t *testing.T) {
	for _, kind := range []string{"bare", "absolute", "relative", "nonexecutable", "relative-path", "empty-path"} {
		t.Run(kind, func(t *testing.T) {
			home, env := autostartEnvironment(t)
			program := filepath.Join(home, "bin", "fixture-app")
			execValue := "fixture-app"
			switch kind {
			case "absolute":
				execValue = program
			case "relative":
				execValue = "./fixture-app"
			case "nonexecutable":
				if err := os.Chmod(program, 0600); err != nil {
					t.Fatal(err)
				}
			case "relative-path":
				env["PATH"] = "bin"
			case "empty-path":
				env["PATH"] = ":"
			}
			writeFixture(t, filepath.Join(env["XDG_CONFIG_HOME"], "autostart", "a.desktop"), "[Desktop Entry]\nType=Application\nName=Fixture\nExec="+execValue+"\n")
			scan, err := ScanAutostart(nil, home, env)
			if err != nil {
				t.Fatal(err)
			}
			want := kind == "bare" || kind == "absolute"
			if (len(scan.Launches) == 1) != want {
				t.Fatal(kind, scan)
			}
			if want && scan.Launches[0].Argv[0] != program {
				t.Fatal("unpinned executable", scan)
			}
			if !want && len(scan.Diagnostics) != 1 {
				t.Fatal("missing unavailable diagnostic", scan)
			}
		})
	}
}

func TestAutostartOwnerLockAt(t *testing.T) {
	path := t.TempDir()
	directory, err := os.Open(path)
	if err != nil {
		t.Fatal(err)
	}
	defer directory.Close()
	for _, name := range []string{"", ".", "..", "../other", "/absolute"} {
		if file, err := LockRecordAt(directory, name); err == nil {
			file.Close()
			t.Fatal("accepted invalid lock name", name)
		}
	}
	if _, err := LockRecordAt(nil, "owner.lock"); err == nil {
		t.Fatal("accepted nil directory")
	}
	lock, err := LockRecordAt(directory, "owner.lock")
	if err != nil {
		t.Fatal(err)
	}
	if other, err := LockRecordAt(directory, "owner.lock"); err == nil {
		other.Close()
		t.Fatal("accepted competing owner")
	}
	before, err := lock.Stat()
	if err != nil {
		t.Fatal(err)
	}
	if err := lock.Close(); err != nil {
		t.Fatal(err)
	}
	lock, err = LockRecordAt(directory, "owner.lock")
	if err != nil {
		t.Fatal(err)
	}
	after, err := lock.Stat()
	lock.Close()
	if err != nil || !os.SameFile(before, after) {
		t.Fatal("lock inode replaced", err)
	}
	if err := os.Symlink("owner.lock", filepath.Join(path, "symlink.lock")); err != nil {
		t.Fatal(err)
	}
	if file, err := LockRecordAt(directory, "symlink.lock"); err == nil {
		file.Close()
		t.Fatal("accepted symlink lock")
	}
	if err := os.Link(filepath.Join(path, "owner.lock"), filepath.Join(path, "hardlink.lock")); err != nil {
		t.Fatal(err)
	}
	if file, err := LockRecordAt(directory, "hardlink.lock"); err == nil {
		file.Close()
		t.Fatal("accepted hard-linked lock")
	}
	writeFixture(t, filepath.Join(path, "public.lock"), "")
	if err := os.Chmod(filepath.Join(path, "public.lock"), 0644); err != nil {
		t.Fatal(err)
	}
	if file, err := LockRecordAt(directory, "public.lock"); err == nil {
		file.Close()
		t.Fatal("accepted nonprivate lock")
	}
	// Replacing the pathname of the parent cannot redirect the anchored open.
	parent := t.TempDir()
	original := filepath.Join(parent, "original")
	moved := filepath.Join(parent, "moved")
	if err := os.Mkdir(original, 0700); err != nil {
		t.Fatal(err)
	}
	anchor, err := os.Open(original)
	if err != nil {
		t.Fatal(err)
	}
	defer anchor.Close()
	if err := os.Rename(original, moved); err != nil {
		t.Fatal(err)
	}
	if err := os.Mkdir(original, 0700); err != nil {
		t.Fatal(err)
	}
	lock, err = LockRecordAt(anchor, "anchored.lock")
	if err != nil {
		t.Fatal(err)
	}
	lock.Close()
	if _, err := os.Stat(filepath.Join(moved, "anchored.lock")); err != nil {
		t.Fatal(err)
	}
	if _, err := os.Stat(filepath.Join(original, "anchored.lock")); !os.IsNotExist(err) {
		t.Fatal("lock escaped directory anchor", err)
	}
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"runtime"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

func TestDirectoryRoleLiteralArgumentsAndWorkingDirectory(t *testing.T) {
	path := "/literal '$[value];`\n/raw-\xff"
	uri := "file:///literal%20%27$[value]%3B%60%0A/raw-%FF"
	// URI punctuation is deliberately compared by decoding as well as by its
	// escaped non-UTF-8 bytes: URL.String preserves some legal URI characters.
	for _, code := range []string{"%f", "%F", "%u", "%U", ""} {
		handler := xdgsource.DirectoryHandler{Exec: "/literal-program " + code, Name: "Application"}
		argv, cwd, err := directoryLaunchArguments(handler, path, "")
		if err != nil || cwd != path || argv[0] != "/literal-program" {
			t.Fatal(code, argv, cwd, err)
		}
		if code == "" {
			if len(argv) != 1 {
				t.Fatal("target appended without a field code", argv)
			}
			continue
		}
		if len(argv) != 2 {
			t.Fatal("target split or expanded", argv)
		}
		if code == "%f" || code == "%F" {
			if argv[1] != path {
				t.Fatalf("raw bytes changed: %q", argv)
			}
		} else if decoded, err := filemodel.ParseTarget(argv[1]); err != nil || string(decoded) != path || !strings.Contains(argv[1], "%FF") || !strings.Contains(argv[1], "%0A") {
			t.Fatal("local URI did not preserve bytes", argv[1], uri, err)
		}
	}
	handler := xdgsource.DirectoryHandler{Exec: "/literal-program %c %k %i", Name: "Name $ ;\"", Icon: "icon $", DesktopPath: "/entry literal.desktop", WorkingDir: "/explicit-cwd"}
	argv, cwd, err := directoryLaunchArguments(handler, path, "")
	if err != nil || cwd != "/explicit-cwd" || !reflect.DeepEqual(argv, []string{"/literal-program", handler.Name, handler.DesktopPath, "--icon", handler.Icon}) {
		t.Fatal("entry metadata became command syntax", argv, cwd, err)
	}
	handler.Terminal = true
	if _, _, err := directoryLaunchArguments(handler, path, ""); err == nil {
		t.Fatal("terminal entry silently used a nonterminal launch")
	}
	if _, _, err := directoryLaunchArguments(handler, path, "/missing-terminal"); err == nil {
		t.Fatal("unavailable terminal accepted")
	}
	executable, _ := os.Executable()
	argv, _, err = directoryLaunchArguments(handler, path, executable)
	if err != nil || len(argv) < 3 || argv[0] != executable || argv[1] != "-e" || argv[2] != "/literal-program" {
		t.Fatal("terminal did not wrap literal argv", argv, err)
	}
}

func directoryRoleWorkerFixture(t *testing.T) (string, string, string) {
	t.Helper()
	root := t.TempDir()
	for key, value := range map[string]string{
		"HOME": root, "XDG_CONFIG_HOME": filepath.Join(root, "config"),
		"XDG_CONFIG_DIRS": filepath.Join(root, "config-system"),
		"XDG_DATA_HOME":   filepath.Join(root, "data"), "XDG_DATA_DIRS": filepath.Join(root, "data-system"),
		"XDG_CURRENT_DESKTOP": "FVWM", "LANG": "C", "LC_ALL": "C",
	} {
		t.Setenv(key, value)
	}
	apps, config := filepath.Join(root, "data", "applications"), filepath.Join(root, "config")
	for _, directory := range []string{apps, config} {
		if err := os.MkdirAll(directory, 0700); err != nil {
			t.Fatal(err)
		}
	}
	return root, apps, filepath.Join(config, "mimeapps.list")
}

func writeDirectoryRoleFixture(t *testing.T, path, value string) {
	t.Helper()
	if err := os.WriteFile(path, []byte(value), 0600); err != nil {
		t.Fatal(err)
	}
}

func readDirectoryRoleReport(t *testing.T, path string) ([]string, string) {
	t.Helper()
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) {
		if data, err := os.ReadFile(path); err == nil {
			var report struct {
				Args [][]byte
				Cwd  []byte
			}
			if err := json.Unmarshal(data, &report); err != nil {
				t.Fatal(err)
			}
			args := make([]string, len(report.Args))
			for i := range report.Args {
				args[i] = string(report.Args[i])
			}
			return args, string(report.Cwd)
		}
		time.Sleep(time.Millisecond)
	}
	t.Fatal("started application did not produce its independent report")
	return nil, ""
}

func TestDirectoryRoleWorkerFreshSelectionAndRawTarget(t *testing.T) {
	root, apps, associations := directoryRoleWorkerFixture(t)
	executable, _ := os.Executable()
	name := "folder '$[value];`\n\xff"
	if runtime.GOOS == "darwin" {
		name = "folder '$[value];`\n"
	}
	folder := filepath.Join(root, name)
	if err := os.Mkdir(folder, 0700); err != nil {
		t.Fatal(err)
	}
	snapshot, err := filemodel.Scan(folder, false)
	if err != nil {
		t.Fatal(err)
	}
	entry := "[Desktop Entry]\nType=Application\nName=Literal alternative\nMimeType=inode/directory;\nExec=\"" + executable + "\" alternative %f\n"
	writeDirectoryRoleFixture(t, filepath.Join(apps, "alternative.desktop"), entry)
	writeDirectoryRoleFixture(t, filepath.Join(apps, "default.desktop"), "[Desktop Entry]\nType=Application\nName=Default\nMimeType=inode/directory;\nExec=\""+executable+"\" default %u\n")
	writeDirectoryRoleFixture(t, associations, "[Default Applications]\ninode/directory=default.desktop;\n")
	query := startWorker(context.Background(), executable, workRequest{Operation: "directory-roles", Snapshot: snapshot})
	if query.Status != "roles" || !query.RoleFolderValid || query.Roles.DefaultID != "default.desktop" || len(query.Roles.Choices) != 2 {
		t.Fatal("role discovery", query)
	}
	var alternative directoryRoleChoice
	for _, choice := range query.Roles.Choices {
		if choice.ID == "alternative.desktop" {
			alternative = choice
		}
	}
	launch := workRequest{Operation: "directory-role-launch", Snapshot: snapshot, RoleID: alternative.ID, RoleFingerprint: alternative.Fingerprint}
	report := filepath.Join(root, "report")
	t.Setenv("FVWM_TEST_ACTION_REPORT", report)
	// Merely changing the default neither substitutes nor invalidates the
	// explicitly captured, still-associated alternative.
	writeDirectoryRoleFixture(t, associations, "[Default Applications]\ninode/directory=alternative.desktop;\n")
	result := startWorker(context.Background(), executable, launch)
	if result.Status != "started" || result.ActionCount != 1 || !result.RoleFolderValid {
		t.Fatal("default change refused explicit selection", result)
	}
	args, cwd := readDirectoryRoleReport(t, report)
	if !reflect.DeepEqual(args, []string{"alternative", folder}) || cwd != folder {
		t.Fatalf("literal raw target/cwd changed: %q %q", args, cwd)
	}
	if err := os.Remove(report); err != nil {
		t.Fatal(err)
	}
	for _, change := range []string{"metadata", "association", "root"} {
		switch change {
		case "metadata":
			writeDirectoryRoleFixture(t, filepath.Join(apps, "alternative.desktop"), strings.Replace(entry, "alternative %f", "changed %f", 1))
		case "association":
			writeDirectoryRoleFixture(t, filepath.Join(apps, "alternative.desktop"), entry)
			writeDirectoryRoleFixture(t, associations, "[Removed Associations]\ninode/directory=alternative.desktop;\n")
		case "root":
			writeDirectoryRoleFixture(t, associations, "")
			if err := os.Rename(folder, folder+"-old"); err != nil {
				t.Fatal(err)
			}
			if err := os.Mkdir(folder, 0700); err != nil {
				t.Fatal(err)
			}
		}
		result = startWorker(context.Background(), executable, launch)
		if result.Status != "failed" || result.ActionCount != 0 || result.RoleFolderValid != (change != "root") {
			t.Fatal("stale launch was not refused with correct root status", change, result)
		}
		if _, err := os.Stat(report); !os.IsNotExist(err) {
			t.Fatal("refused application was started", change, err)
		}
	}
}

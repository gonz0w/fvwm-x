// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package startuplaunch

import (
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

func TestLaunchPlanPreservesLiteralArgvAndClearsInheritedID(t *testing.T) {
	argv := []string{"/app", "two words", "raw-\xff", `quote'"$[]`, "line\nnext"}
	env := []string{"A=one", "DESKTOP_STARTUP_ID=old", "B=two", "DESKTOP_STARTUP_ID=older"}
	enabled, disabled := true, false
	metadata := &xdgmenu.StartupMetadata{Name: "Localized App", Icon: "icon", ApplicationID: "app.desktop", WMClass: "AppClass", Notify: &enabled}
	for _, helper := range []string{"", "/private/fvwm-startup-launch"} {
		plan, err := Prepare(argv, metadata, helper, env)
		want := argv
		if helper != "" {
			want = append([]string{helper, "--name", metadata.Name, "--icon", "icon", "--application-id", "app.desktop", "--wmclass", "AppClass", "--"}, argv...)
		}
		if err != nil || !reflect.DeepEqual(plan.Argv, want) || !reflect.DeepEqual(plan.Environment, []string{"A=one", "B=two"}) {
			t.Fatal(plan, err)
		}
	}
	metadata.Notify = &disabled
	plan, err := Prepare(argv, metadata, "/private/helper", env)
	if err != nil || !reflect.DeepEqual(plan.Argv, argv) || strings.Contains(strings.Join(plan.Environment, "\n"), "DESKTOP_STARTUP_ID=") {
		t.Fatal("explicit false failed to override class", plan, err)
	}
	if !reflect.DeepEqual(env, []string{"A=one", "DESKTOP_STARTUP_ID=old", "B=two", "DESKTOP_STARTUP_ID=older"}) {
		t.Fatal("caller environment mutated", env)
	}
}

func TestPrivateHelperUsesResolvedPeersThenConfiguredModuleDirectory(t *testing.T) {
	root := t.TempDir()
	self := filepath.Join(root, "bin", "FvwmPrompt", "fvwm-menu-desktop")
	module := filepath.Join(root, "modules")
	paths := []string{filepath.Join(filepath.Dir(self), "fvwm-startup-launch"), filepath.Join(root, "bin", "fvwm-startup-launch"), filepath.Join(module, "fvwm-startup-launch")}
	for i := len(paths) - 1; i >= 0; i-- {
		path := paths[i]
		if err := os.MkdirAll(filepath.Dir(path), 0700); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(path, []byte("fixture; never executed"), 0700); err != nil {
			t.Fatal(err)
		}
		if got := findHelper(self, module); got != path {
			t.Fatal("private helper precedence lost", got, path)
		}
	}
	for _, path := range paths {
		if err := os.Remove(path); err != nil {
			t.Fatal(err)
		}
	}
	if got := findHelper(self, module); got != "" {
		t.Fatal("invented an absent helper", got)
	}
}

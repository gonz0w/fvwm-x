// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package startuplaunch adapts literal launch metadata to the optional private
// X11 announcement/exec helper. It owns no application lifetime or X state.
package startuplaunch

import (
	"errors"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/runtimepath"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

// ModuleDirectory is linked from Meson's actual private module installation
// directory for bindir consumers; module peers and build siblings win first.
var ModuleDirectory string

type Plan struct {
	Argv        []string
	Environment []string
}

func Prepare(argv []string, metadata *xdgmenu.StartupMetadata, helper string, environment []string) (Plan, error) {
	if len(argv) == 0 || argv[0] == "" {
		return Plan{}, errors.New("missing application command")
	}
	for _, value := range argv {
		if strings.ContainsRune(value, 0) {
			return Plan{}, errors.New("NUL in application argument")
		}
	}
	plan := Plan{Argv: append([]string(nil), argv...), Environment: make([]string, 0, len(environment))}
	for _, value := range environment {
		if !strings.HasPrefix(value, "DESKTOP_STARTUP_ID=") {
			plan.Environment = append(plan.Environment, value)
		}
	}
	if helper == "" || !xdgmenu.ValidStartupMetadata(metadata) || !xdgmenu.StartupEnabled(metadata) {
		return plan, nil
	}
	wrapped := []string{helper, "--name", metadata.Name}
	for _, pair := range [][2]string{{"--icon", metadata.Icon}, {"--application-id", metadata.ApplicationID}, {"--wmclass", metadata.WMClass}} {
		if pair[1] != "" {
			wrapped = append(wrapped, pair[0], pair[1])
		}
	}
	wrapped = append(wrapped, "--")
	plan.Argv = append(wrapped, argv...)
	return plan, nil
}

func findHelper(self, moduleDirectory string) string {
	var paths []string
	if filepath.IsAbs(self) {
		directory := filepath.Dir(self)
		paths = append(paths, filepath.Join(directory, "fvwm-startup-launch"),
			filepath.Join(directory, "..", "fvwm-startup-launch"))
	}
	if filepath.IsAbs(moduleDirectory) {
		paths = append(paths, filepath.Join(moduleDirectory, "fvwm-startup-launch"))
	}
	for _, path := range paths {
		if info, err := os.Stat(path); err == nil && info.Mode().IsRegular() && info.Mode().Perm()&0111 != 0 {
			return filepath.Clean(path)
		}
	}
	return ""
}

func Current(argv []string, metadata *xdgmenu.StartupMetadata) (Plan, error) {
	helper := ""
	if xdgmenu.StartupEnabled(metadata) && xdgmenu.ValidStartupMetadata(metadata) {
		self, _ := runtimepath.Executable(os.Args[0])
		helper = findHelper(self, ModuleDirectory)
	}
	return Prepare(argv, metadata, helper, os.Environ())
}

// Command does not start or wait. The existing caller retains its detached
// process/group policy, cwd and exactly-once submission. Helper failure after
// Start has its own diagnostic/nonzero exit and never triggers an app retry.
func Command(argv []string, metadata *xdgmenu.StartupMetadata) (*exec.Cmd, error) {
	plan, err := Current(argv, metadata)
	if err != nil {
		return nil, err
	}
	command := exec.Command(plan.Argv[0], plan.Argv[1:]...)
	command.Env, command.Stderr = plan.Environment, os.Stderr
	return command, nil
}

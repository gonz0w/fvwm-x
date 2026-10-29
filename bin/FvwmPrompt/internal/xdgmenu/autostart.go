// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"encoding/json"
	"errors"
	"path/filepath"
	"strings"
)

const (
	MaxAutostartConfigRoots      = 16
	MaxAutostartDirectoryEntries = 1024
	MaxAutostartEntries          = 256
	MaxAutostartFileBytes        = 256 * 1024
	MaxAutostartInputBytes       = 8 * 1024 * 1024
	MaxAutostartOutputBytes      = 512 * 1024
	MaxAutostartDiagnostics      = 64
	MaxAutostartDiagnosticBytes  = 512
)

// AutostartLaunch describes one literal Exec fallback, never shell source.
// Starting it establishes neither application nor service readiness.
type AutostartLaunch struct {
	Filename  string
	Path      string
	Name      string
	Argv      []string
	Directory string
}

// AutostartScan is an all-or-nothing worker result. Diagnostics cover skipped,
// invalid and unavailable filename winners, capped independently of launches.
type AutostartScan struct {
	Launches    []AutostartLaunch
	Diagnostics []string
}

type AutostartOptions struct {
	Desktops   []string
	Locales    []string
	Executable func(string) bool
	Directory  func(string) bool
}

func autostartPath(path string) bool {
	return pathValue(path) && filepath.IsAbs(path) && filepath.Clean(path) == path
}

func autostartFilename(name string) bool {
	return pathValue(name) && name != ".desktop" && filepath.Base(name) == name &&
		strings.HasSuffix(name, ".desktop")
}

// PlanAutostart implements Autostart 0.5 sections 2.1-2.2.3, after acquisition
// has selected the highest-priority filename. In particular Visible is menu
// policy: its NoDisplay and empty-list behavior must not be reused here.
// A nonempty reason with a launch records the portable D-Bus Exec fallback.
func PlanAutostart(d *Desktop, filename, path string, options AutostartOptions) (AutostartLaunch, string, error) {
	var none AutostartLaunch
	if d == nil || d.Groups["Desktop Entry"] == nil || !autostartFilename(filename) || !autostartPath(path) || filepath.Base(path) != filename {
		return none, "", errors.New("invalid autostart entry or location")
	}
	const group = "Desktop Entry"
	hidden, err := d.Boolean(group, "Hidden")
	if err != nil {
		return none, "", err
	}
	if hidden {
		return none, "skipped: Hidden=true", nil
	}
	entryType, err := d.String(group, "Type")
	if err != nil {
		return none, "", err
	}
	if entryType != "Application" {
		return none, "skipped: Type is not Application", nil
	}
	_, onlyPresent := d.Groups[group]["OnlyShowIn"]
	_, notPresent := d.Groups[group]["NotShowIn"]
	if onlyPresent && notPresent {
		return none, "", errors.New("both OnlyShowIn and NotShowIn keys are present")
	}
	only, err := d.List(group, "OnlyShowIn")
	if err != nil {
		return none, "", err
	}
	not, err := d.List(group, "NotShowIn")
	if err != nil {
		return none, "", err
	}
	match := func(values []string) bool {
		for _, desktop := range options.Desktops {
			for _, value := range values {
				if value != "" && value == desktop {
					return true
				}
			}
		}
		return false
	}
	if onlyPresent && !match(only) || match(not) {
		return none, "skipped: desktop filter", nil
	}
	tryExec, err := d.String(group, "TryExec")
	if err != nil {
		return none, "", err
	}
	if tryExec != "" {
		if !filepath.IsAbs(tryExec) && strings.ContainsRune(tryExec, '/') {
			return none, "", errors.New("TryExec must be absolute or a bare executable name")
		}
		if options.Executable == nil || !options.Executable(tryExec) {
			return none, "unavailable: TryExec executable not found", nil
		}
	}
	terminal, err := d.Boolean(group, "Terminal")
	if err != nil {
		return none, "", err
	}
	if terminal {
		return none, "unsupported: Terminal=true requires a terminal provider", nil
	}
	bus, err := d.Boolean(group, "DBusActivatable")
	if err != nil {
		return none, "", err
	}
	if d.Groups[group]["Exec"] == "" {
		if bus {
			return none, "unsupported: D-Bus activation has no Exec fallback", nil
		}
		return none, "unavailable: missing Exec", nil
	}
	name, err := d.Localized(group, "Name", options.Locales)
	if err != nil {
		return none, "", err
	}
	if name == "" {
		return none, "", errors.New("autostart Name is empty")
	}
	icon, err := d.String(group, "Icon")
	if err != nil {
		return none, "", err
	}
	directory, err := d.String(group, "Path")
	if err != nil {
		return none, "", err
	}
	if directory != "" {
		if !pathValue(directory) || !filepath.IsAbs(directory) {
			return none, "", errors.New("autostart Path must be an absolute directory")
		}
		if options.Directory == nil || !options.Directory(directory) {
			return none, "unavailable: Path is not an existing directory", nil
		}
	}
	argv, err := ExpandExec(d.Groups[group]["Exec"], ExecContext{Name: name, Icon: icon, Location: path})
	if err != nil {
		return none, "", err
	}
	launch := AutostartLaunch{Filename: filename, Path: path, Name: name, Argv: argv, Directory: directory}
	reason := ""
	if bus {
		reason = "portable Exec fallback: D-Bus activation is unsupported"
	}
	return launch, reason, nil
}

// ValidateAutostartScan checks the complete bounded worker result before the
// session owner starts any entry. The wire reader must also cap input before
// decoding; validation does not make an unbounded JSON decoder safe.
func ValidateAutostartScan(scan AutostartScan) error {
	if len(scan.Launches) > MaxAutostartEntries || len(scan.Diagnostics) > MaxAutostartDiagnostics {
		return errors.New("autostart output exceeds entry or diagnostic limit")
	}
	remaining := MaxAutostartOutputBytes
	text := func(value string) bool {
		if !pathValue(value) || len(value) >= remaining {
			return false
		}
		// Charge even empty strings so aggregate argument traversal is bounded.
		remaining -= len(value) + 1
		return true
	}
	last := ""
	for _, entry := range scan.Launches {
		if !autostartFilename(entry.Filename) || entry.Filename <= last ||
			!autostartPath(entry.Path) || filepath.Base(entry.Path) != entry.Filename ||
			entry.Directory != "" && (!pathValue(entry.Directory) || !filepath.IsAbs(entry.Directory)) ||
			entry.Name == "" || len(entry.Argv) == 0 || len(entry.Argv) > MaxItems || !autostartPath(entry.Argv[0]) {
			return errors.New("invalid or unordered autostart launch")
		}
		last = entry.Filename
		for _, value := range []string{entry.Filename, entry.Path, entry.Name, entry.Directory} {
			if !text(value) {
				return errors.New("autostart output exceeds string limits")
			}
		}
		for _, arg := range entry.Argv {
			if !text(arg) {
				return errors.New("autostart output exceeds argument limits")
			}
		}
	}
	for _, diagnostic := range scan.Diagnostics {
		if len(diagnostic) > MaxAutostartDiagnosticBytes || !text(diagnostic) {
			return errors.New("autostart output exceeds diagnostic limits")
		}
	}
	data, err := json.Marshal(scan)
	if err != nil || len(data) > MaxAutostartOutputBytes {
		return errors.New("autostart encoded output exceeds limit")
	}
	return nil
}

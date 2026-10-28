// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import "strings"

// CurrentDesktops decodes the desktop environment, independently of menu-file
// selection. Preserve the resident worker's empty-value FVWM fallback.
func CurrentDesktops(value string) []string {
	if value == "" {
		return []string{"FVWM"}
	}
	return strings.Split(value, ":")
}

// LocalizedIcon decodes an optional icon with the same locale order as Name.
// Localized-only values without a required base value do not supply an icon.
func LocalizedIcon(entry *Desktop, group string, locales []string) (string, error) {
	if _, present := entry.Groups[group]["Icon"]; !present {
		return "", nil
	}
	return entry.Localized(group, "Icon", locales)
}

// DeclaredAction is bounded presentation/Exec source metadata, not a launch.
// Callers expand Exec using the base application's Name and source Location,
// enforce their own argv/output bounds and inherit parent cwd/Terminal.
// Keeping argv expansion caller-owned charges one action before expanding the
// next, rather than retaining eight provider-sized argv buffers in a plan.
type DeclaredAction struct {
	Label, Icon, Exec string
}

// DeclaredActions considers only the first eight declared IDs, once each.
// Malformed metadata and unlisted groups never replace a valid primary entry.
// Exec grammar/executable checks remain with each consumer's expansion policy.
func DeclaredActions(entry *Desktop, locales []string, parentName, parentIcon string) []DeclaredAction {
	if entry == nil {
		return nil
	}
	actions, err := entry.List("Desktop Entry", "Actions")
	if err != nil {
		return nil
	}
	var result []DeclaredAction
	seen := make(map[string]bool)
	for _, id := range actions[:min(8, len(actions))] {
		if id == "" || seen[id] {
			continue
		}
		seen[id] = true
		group := "Desktop Action " + id
		name, err := entry.Localized(group, "Name", locales)
		if err != nil || name == "" || len(parentName)+len(" — ")+len(name) > MaxValueBytes {
			continue
		}
		icon, err := LocalizedIcon(entry, group, locales)
		if err != nil {
			continue
		}
		if icon == "" {
			icon = parentIcon
		}
		result = append(result, DeclaredAction{Label: parentName + " — " + name,
			Icon: icon, Exec: entry.Groups[group]["Exec"]})
	}
	return result
}

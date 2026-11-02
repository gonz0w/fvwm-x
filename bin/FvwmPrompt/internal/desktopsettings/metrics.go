// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import "strings"

// DesktopMetrics contains profile values, not observed resources or pixels.
// Native consumers apply their existing scale convention exactly once.
type DesktopMetrics struct {
	Version                            int
	Font, Density, Border              string
	MenuItemPad, MenuTitlePad, FormPad int
	MenuBorder                         int
}

func DesktopKey(key string) bool {
	return key == "desktop_font" || key == "desktop_density" || key == "desktop_border"
}

// FontCommandSafe guarantees that native command serialization preserves every
// accepted byte. Application-font data has a different, broader transport.
func FontCommandSafe(value string) bool {
	if len(value) > 128 || strings.TrimSpace(value) != value {
		return false
	}
	for _, r := range value {
		if r < 32 || r == 127 || strings.ContainsRune("\\\"$%&", r) {
			return false
		}
	}
	return true
}

func ResolveDesktopMetrics(settings Values) (DesktopMetrics, error) {
	var result DesktopMetrics
	for _, key := range []string{"desktop_font", "desktop_density", "desktop_border"} {
		if _, err := Value(key, settings[key], false); err != nil {
			return result, err
		}
	}
	result = DesktopMetrics{Version: 1, Font: settings["desktop_font"].(string),
		Density: settings["desktop_density"].(string), Border: settings["desktop_border"].(string),
		MenuItemPad: 8, MenuTitlePad: 10, FormPad: 12, MenuBorder: 1}
	switch result.Density {
	case "compact":
		result.MenuItemPad, result.MenuTitlePad, result.FormPad = 4, 6, 6
	case "spacious":
		result.MenuItemPad, result.MenuTitlePad, result.FormPad = 12, 14, 16
	}
	if result.Border == "strong" {
		result.MenuBorder = 3
	}
	return result, nil
}

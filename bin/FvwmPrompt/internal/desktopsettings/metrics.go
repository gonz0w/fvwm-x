// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import (
	"fmt"
	"strings"
)

// DesktopMetrics contains profile values, not observed resources or pixels.
// Native consumers apply their existing scale convention exactly once.
type DesktopMetrics struct {
	Version                                 int
	Font, Density, Border                   string
	MenuItemPad, MenuTitlePad, FormPad      int
	MenuBorder                              int
	Style                                   string
	LauncherFont, LauncherTitleFont         string
	FormFont, FormInputFont, FormButtonFont string
	LauncherDensity, FormDensity            string
	MenuRelief, CornerRadius                int
	LongSeparators, ReliefTriangles         bool
}

func DesktopKey(key string) bool {
	item, ok := find(key)
	return ok && item.Group == "desktop"
}

// desktopFallback names only data-policy defaults. It does not infer module
// ownership or observe per-widget fonts from existing native consumers.
func desktopFallback(key string) string {
	switch key {
	case "launcher_font", "launcher_title_font", "form_font", "form_input_font", "form_button_font":
		return "desktop_font"
	case "launcher_density", "form_density":
		return "desktop_density"
	}
	return ""
}

// DesktopResolution reports fallback independently of the saved/module owner of
// the requested key. A configured-profile result is deliberately not a font
// name: consumers restore their recorded native profile baseline.
func DesktopResolution(settings Values, key string) (string, string, error) {
	if !DesktopKey(key) || key == "desktop_icon_theme" {
		return "", "", fmt.Errorf("Not a desktop metric: %s", key)
	}
	value, err := Value(key, settings[key], false)
	if err != nil {
		return "", "", err
	}
	if value != "" && value != "inherit" {
		return value.(string), "explicit", nil
	}
	if fallback := desktopFallback(key); fallback != "" {
		value, err = Value(fallback, settings[fallback], false)
		if err != nil {
			return "", "", err
		}
		if value != "" && value != "inherit" {
			return value.(string), "desktop-default", nil
		}
	}
	return "configured-profile", "profile-baseline", nil
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
	for _, item := range registry {
		if DesktopKey(item.Key) && item.Key != "desktop_icon_theme" {
			if _, err := Value(item.Key, settings[item.Key], false); err != nil {
				return result, err
			}
		}
	}
	resolve := func(key string) string {
		value := settings[key].(string)
		if value == "" || value == "inherit" {
			if fallback := desktopFallback(key); fallback != "" {
				return settings[fallback].(string)
			}
		}
		return value
	}
	result = DesktopMetrics{Version: 2, Font: settings["desktop_font"].(string),
		Density: settings["desktop_density"].(string), Border: settings["desktop_border"].(string),
		Style:        settings["desktop_style"].(string),
		LauncherFont: resolve("launcher_font"), LauncherTitleFont: resolve("launcher_title_font"),
		FormFont: resolve("form_font"), FormInputFont: resolve("form_input_font"),
		FormButtonFont:  resolve("form_button_font"),
		LauncherDensity: resolve("launcher_density"), FormDensity: resolve("form_density"),
		MenuItemPad: 8, MenuTitlePad: 10, FormPad: 12, MenuBorder: 1,
		ReliefTriangles: true}
	switch result.LauncherDensity {
	case "compact":
		result.MenuItemPad, result.MenuTitlePad = 4, 6
	case "spacious":
		result.MenuItemPad, result.MenuTitlePad = 12, 14
	}
	switch result.FormDensity {
	case "compact":
		result.FormPad = 6
	case "spacious":
		result.FormPad = 16
	}
	if result.Border == "strong" {
		result.MenuBorder = 3
	}
	switch result.Style {
	case "flat":
		result.LongSeparators, result.ReliefTriangles = true, false
	case "relief":
		result.MenuRelief = 2
	}
	return result, nil
}

// DesktopIconTheme selects only the reserved native tray's resolver profile.
// Application publication and explicit installed-menu profiles are independent.
func DesktopIconTheme(settings Values) (string, error) {
	value, err := Value("desktop_icon_theme", settings["desktop_icon_theme"], false)
	if err != nil {
		return "", err
	}
	if value == "" {
		return "hicolor", nil
	}
	return value.(string), nil
}

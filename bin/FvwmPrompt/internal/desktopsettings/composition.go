// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import "fmt"

// The color-role projection evolves independently of the preference file schema.
const themeCompositionVersion = 1

type ColorRole struct {
	Name                                      string
	Colorset                                  int
	Foreground, Background, Highlight, Shadow string
}

type ThemeProjection struct {
	Version int
	Roles   []ColorRole
}

// ProjectTheme validates the complete palette before producing a projection.
// It returns operation-owned values and has no command-publication authority.
func ProjectTheme(colors map[string]string) (ThemeProjection, error) {
	for _, key := range []string{"background", "surface", "raised", "text",
		"muted", "border", "accent", "on_accent"} {
		value := colors[key]
		if len(value) != 7 || value[0] != '#' {
			return ThemeProjection{}, fmt.Errorf("Invalid desktop color: %s", key)
		}
		for _, c := range value[1:] {
			if !(c >= '0' && c <= '9' || c >= 'a' && c <= 'f' || c >= 'A' && c <= 'F') {
				return ThemeProjection{}, fmt.Errorf("Invalid desktop color: %s", key)
			}
		}
	}
	projection := ThemeProjection{Version: themeCompositionVersion}
	for _, role := range []struct {
		name string
		id   int
		fg   string
		bg   string
	}{
		{"surface", 0, "text", "surface"},
		{"muted", 1, "muted", "surface"},
		{"raised", 2, "text", "raised"},
		{"border", 3, "border", "border"},
		{"accent", 4, "accent", "accent"},
		{"menu.normal", 5, "text", "surface"},
		{"menu.active", 6, "on_accent", "accent"},
		{"menu.disabled", 7, "muted", "surface"},
		{"menu.accent", 8, "accent", "surface"},
		{"panel.normal", 10, "text", "surface"},
		{"panel.active", 11, "on_accent", "accent"},
		{"panel.raised", 12, "text", "raised"},
		{"panel.selected", 13, "on_accent", "accent"},
		{"panel.muted", 14, "muted", "surface"},
	} {
		projection.Roles = append(projection.Roles, ColorRole{
			Name: role.name, Colorset: role.id, Foreground: colors[role.fg],
			Background: colors[role.bg], Highlight: colors[role.bg],
			Shadow: colors[role.bg]})
	}
	return projection, nil
}

// ResolveWallpaper applies retention before independent wallpaper selections.
// The caller supplies complete effective settings, including the current retained
// pair. Resolution returns a new pair without changing that borrowed state.
func ResolveWallpaper(settings Values, minute int) (Values, error) {
	checked, err := Checked(settings)
	if err != nil {
		return nil, err
	}
	mode, err := Appearance(checked, minute)
	if err != nil {
		return nil, err
	}
	if checked["keep_wallpaper"] == true {
		return checked["wallpaper"].(map[string]any), nil
	}
	palette := checked["wallpaper_palette"]
	if palette == "follow" {
		palette = checked["palette"]
	}
	if checked["wallpaper_appearance"] != "follow" {
		mode = checked["wallpaper_appearance"].(string)
	}
	return Values{"palette": palette, "appearance": mode}, nil
}

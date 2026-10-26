// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import (
	"fmt"
	"math"
	"strconv"
)

// The color-role projection evolves independently of the preference file schema.
const themeCompositionVersion = 2

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
func ProjectTheme(colors map[string]string, appearance, contrast string) (ThemeProjection, error) {
	if len(colors) != 8 || !has([]string{"light", "dark"}, appearance) ||
		!has([]string{"normal", "high"}, contrast) {
		return ThemeProjection{}, fmt.Errorf("Invalid desktop theme composition")
	}
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
	if contrast == "high" {
		// Own the adjusted palette; callers retain their requested source colors.
		adjusted := make(map[string]string, len(colors))
		for key, value := range colors {
			adjusted[key] = value
		}
		surface, text := "#000000", "#ffffff"
		if appearance == "light" {
			surface, text = text, surface
		}
		adjusted["surface"], adjusted["raised"] = surface, surface
		adjusted["text"], adjusted["muted"], adjusted["border"] = text, text, text
		if contrastRatio(adjusted["accent"], surface) < 4.5 {
			adjusted["accent"] = text
		}
		adjusted["on_accent"] = "#000000"
		if contrastRatio("#ffffff", adjusted["accent"]) >
			contrastRatio("#000000", adjusted["accent"]) {
			adjusted["on_accent"] = "#ffffff"
		}
		colors = adjusted
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

// contrastRatio uses sRGB relative luminance after validated #RRGGBB input.
func contrastRatio(first, second string) float64 {
	luminance := func(color string) float64 {
		var linear [3]float64
		for i := range linear {
			component, _ := strconv.ParseUint(color[1+i*2:3+i*2], 16, 8)
			value := float64(component) / 255
			linear[i] = value / 12.92
			if value > 0.04045 {
				linear[i] = math.Pow((value+0.055)/1.055, 2.4)
			}
		}
		return 0.2126*linear[0] + 0.7152*linear[1] + 0.0722*linear[2]
	}
	a, b := luminance(first), luminance(second)
	if a < b {
		a, b = b, a
	}
	return (a + 0.05) / (b + 0.05)
}

type Presentation struct {
	Contrast        string
	Transparency    bool
	LauncherOpacity int
	Constraint      string
}

// ResolvePresentation preserves requested preferences while resolving the
// strongest opaque presentation. It validates even settings hidden by a constraint.
func ResolvePresentation(settings Values) (Presentation, error) {
	checked, err := Checked(settings)
	if err != nil {
		return Presentation{}, err
	}
	result := Presentation{Contrast: checked["contrast"].(string),
		Transparency:    checked["transparency"].(bool),
		LauncherOpacity: checked["launcher_opacity"].(int)}
	if result.Contrast == "high" {
		result.Transparency, result.LauncherOpacity = false, 100
		result.Constraint = "high-contrast"
	} else if !result.Transparency {
		result.LauncherOpacity = 100
		result.Constraint = "transparency-disabled"
	}
	return result, nil
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

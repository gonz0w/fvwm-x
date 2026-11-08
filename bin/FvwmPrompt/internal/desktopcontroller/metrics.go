// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"fmt"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

func onlyDesktop(changes values) bool {
	if len(changes) == 0 {
		return false
	}
	for key := range changes {
		if !desktopsettings.DesktopKey(key) {
			return false
		}
	}
	return true
}

func (d *Controller) desktopSubmitted() bool {
	metrics, err := desktopsettings.ResolveDesktopMetrics(d.effective)
	return err == nil && d.lastDesktop != nil && *d.lastDesktop == metrics
}

// Called only after the resource-preview gate. A full color publication calls
// this after CopyMenuStyle; a metrics-only change edits the existing named style.
// Reapply explicit axes and reset only axes this controller had owned.
// Inherit restores the named profile baseline, not arbitrary external commands.
func (d *Controller) applyDesktopMetrics(metrics desktopsettings.DesktopMetrics) {
	previous := desktopsettings.DesktopMetrics{}
	if d.lastDesktop != nil {
		previous = *d.lastDesktop
	}
	options := []string{}
	if metrics.LauncherFont != "" || previous.LauncherFont != "" {
		font := `"$[infostore.slate_font]"`
		if metrics.LauncherFont != "" {
			// Policy refuses unrepresentable bytes instead of silently cleaning.
			font = `"` + metrics.LauncherFont + `"`
		}
		options = append(options, "Font "+font)
	}
	if metrics.LauncherTitleFont != "" || previous.LauncherTitleFont != "" {
		// A bare TitleFont restores Scr.DefaultFont, the profile title baseline.
		option := "TitleFont"
		if metrics.LauncherTitleFont != "" {
			option += ` "` + metrics.LauncherTitleFont + `"`
		}
		options = append(options, option)
	}
	if metrics.LauncherDensity != "inherit" || previous.LauncherDensity != "" && previous.LauncherDensity != "inherit" {
		options = append(options, fmt.Sprintf("VerticalItemSpacing %d %d, VerticalTitleSpacing %d %d",
			metrics.MenuItemPad, metrics.MenuItemPad, metrics.MenuTitlePad, metrics.MenuTitlePad))
	}
	if metrics.Border != "inherit" || previous.Border != "" && previous.Border != "inherit" {
		options = append(options, fmt.Sprintf("BorderWidth %d", metrics.MenuBorder))
	}
	if metrics.Style != "inherit" || previous.Style != "" && previous.Style != "inherit" {
		separators, triangles := "SeparatorsShort", "TrianglesSolid"
		if metrics.LongSeparators {
			separators = "SeparatorsLong"
		}
		if metrics.ReliefTriangles {
			triangles = "TrianglesRelief"
		}
		options = append(options, fmt.Sprintf("Hilight3DThickness %d, %s, %s",
			metrics.MenuRelief, separators, triangles))
	}
	if len(options) != 0 {
		d.emit("MenuStyle SlateLauncherStyle " + strings.Join(options, ", "))
	}
	d.lastDesktop = &metrics
}

func desktopFormMetrics(metrics desktopsettings.DesktopMetrics) []string {
	var lines []string
	for _, font := range []struct{ name, value string }{
		{"Font", metrics.FormFont}, {"InputFont", metrics.FormInputFont},
		{"ButtonFont", metrics.FormButtonFont},
	} {
		if font.value != "" {
			lines = append(lines, font.name+` "`+font.value+`"`)
		}
	}
	if metrics.FormDensity != "inherit" {
		lines = append(lines, fmt.Sprintf("PadVText %d", metrics.FormPad))
	}
	return lines
}

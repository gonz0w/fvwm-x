// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"fmt"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

// The desktop reserves one binary/alias pair. Custom FvwmTray instances remain
// outside this lifecycle. Submitted enablement is not observed tray ownership.
func (d *Controller) projectTray() { d.configureTray(true) }

func (d *Controller) desktopIconsSubmitted() bool {
	theme, err := desktopsettings.DesktopIconTheme(d.effective)
	return err == nil && theme == d.lastDesktopIconTheme
}

func (d *Controller) applyDesktopIcons() {
	if !d.desktopIconsSubmitted() {
		d.configureTray(false)
	}
}

func (d *Controller) configureTray(updateBar bool) {
	theme, err := desktopsettings.DesktopIconTheme(d.effective)
	if err != nil {
		d.fatal = err
		return
	}
	// Configuration reloads can arrive while resource recovery is pending.
	// Retain the submitted icon profile until the guarded theme replay.
	if d.preview.blocked || d.preview.recovering ||
		d.preview.control != nil && d.preview.control.resource != nil {
		theme = d.lastDesktopIconTheme
		if theme == "" {
			theme = "hicolor"
		}
	}

	// Execute the fixed configuration as one core function. A tray's config
	// request cannot interleave with this tuple's removal/replacement.
	modern := "Off"
	if d.effective["tray_modern"] == true {
		modern = "On"
	}
	refresh := "Refresh"
	if updateBar {
		refresh = "Reload"
	}
	commands := []string{
		"DestroyModuleConfig SlateTray:*",
		"*SlateTray Colorset 2",
		"*SlateTray StatusNotifier " + modern,
		"*SlateTray IconTheme " + theme,
		fmt.Sprintf("*SlateTray IconSize %d", d.effective["tray_icon_size"].(int)),
		`*SlateTray Font "$[infostore.slate_font]"`,
		fmt.Sprintf("*SlateTray Gap %d", d.effective["tray_gap"].(int)),
		fmt.Sprintf("*SlateTray Padding %d", d.effective["tray_padding"].(int)),
		"*SlateTray Orientation " + title(d.effective["tray_orientation"].(string)),
		"*SlateTray Visible Off",
		"SendToModule SlateTray " + refresh,
	}
	d.emit("DestroyFunc SlateTrayConfigure")
	for _, command := range commands {
		d.emit("AddToFunc SlateTrayConfigure I " + command)
	}
	d.emit("SlateTrayConfigure")
	d.lastDesktopIconTheme = theme
	if !updateBar {
		return
	}
	enabled := 0
	if d.effective["tray_enabled"] == true {
		enabled = 1
	}
	d.emit(fmt.Sprintf("SendToModule SlateBar ChangeButton tray Enabled %d", enabled))
}

func (d *Controller) applyTrayLifecycle(explicit bool) {
	requested := d.effective["tray_enabled"] == true
	initial := !d.trayInitialized
	changed := d.trayRequested != requested
	d.trayInitialized, d.trayRequested = true, requested
	if initial && !requested || !initial && !changed && !explicit {
		return
	}
	// Enable is a non-replacing asynchronous attempt. Keep an existing owner
	// alive through controller restart and explicit retries; only disable may
	// stop the reserved binary/alias pair.
	if requested {
		d.emit("Module FvwmTray SlateTray")
	} else {
		d.emit("KillModule FvwmTray SlateTray")
	}
}

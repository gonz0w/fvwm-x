// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"time"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopworker"
)

// The event loop owns this single pending transaction. A worker owns acquisition
// only; even a prepared map is not live while native preview recovery is pending.
type themeReload struct {
	instance, generation string
	job                  *job
	palettes             desktopsettings.Palettes
}

func (d *Controller) reloadTheme() error {
	if d.closed || !d.ready || d.fatal != nil {
		return errors.New("Theme reload requires a ready desktop")
	}
	if d.themeReload != nil {
		return errors.New("A theme reload is already pending")
	}
	if d.preview.blocked {
		return errors.New("Theme reload refused: preview recovery is unconfirmed; restart the controller")
	}
	if d.preview.control != nil && d.preview.control.resource == nil || d.preview.control == nil && d.preview.pending != nil {
		return errors.New("Finish the launcher opacity preview before reloading theme colors")
	}
	if !d.workerJob("theme-reload", "palettes", values{"path": filepath.Join(d.root, "palettes.json")}) {
		return errors.New("Desktop service is busy; try reloading theme colors again")
	}
	d.themeReload = &themeReload{instance: d.instance, generation: d.generation, job: d.jobs["theme-reload"]}
	d.emit("Echo FvwmDesktop: theme colors reload pending.")
	return d.fatal
}

func (d *Controller) refuseThemeReload(err error) {
	d.themeReload = nil
	if !d.closed {
		fmt.Fprintf(os.Stderr, "FvwmDesktop: Theme reload refused: %q\n", err.Error())
	}
}

func (d *Controller) finishThemeReload(completed job) {
	pending := d.themeReload
	if pending == nil || completed.origin != pending.job {
		return
	}
	if completed.err != nil {
		d.refuseThemeReload(fmt.Errorf("%s: %w", filepath.Join(d.root, "palettes.json"), completed.err))
		return
	}
	result, err := resultAs[desktopworker.PaletteResult](completed.result)
	if err == nil && result.Error != "" {
		if result.Palettes != nil || len(result.Error) > 4096 || !utf8.ValidString(result.Error) {
			err = errors.New("Invalid palette worker diagnostic")
		} else {
			err = errors.New(result.Error)
		}
	}
	if err != nil {
		d.refuseThemeReload(err)
		return
	}
	// Recheck the private worker result before granting publication authority.
	data, err := json.Marshal(result.Palettes)
	if err == nil {
		pending.palettes, err = desktopsettings.DecodePalettes(data)
	}
	if err != nil {
		d.refuseThemeReload(fmt.Errorf("%s: %w", filepath.Join(d.root, "palettes.json"), err))
		return
	}
	d.publishThemeReload()
}

func (d *Controller) publishThemeReload() {
	pending := d.themeReload
	if pending == nil || pending.palettes == nil {
		return
	}
	if d.closed {
		d.themeReload = nil
		return
	}
	if pending.instance != d.instance || pending.generation != d.generation {
		d.refuseThemeReload(errors.New("Settings changed while theme colors were being prepared; reload again"))
		return
	}
	if d.preview.blocked {
		d.refuseThemeReload(errors.New("Preview recovery is unconfirmed; working theme retained"))
		return
	}
	if d.preview.control != nil || d.preview.pending != nil || d.preview.recovering {
		if d.preview.control != nil && d.preview.control.resource != nil {
			if !d.preview.recovering {
				d.preview.finish("Theme reload requested; recovering preview resources first.", true, false)
			}
		} else {
			d.refuseThemeReload(errors.New("Finish the launcher opacity preview before reloading theme colors"))
		}
		return
	}
	presentation, err := desktopsettings.ResolvePresentation(d.effective)
	if err != nil {
		d.refuseThemeReload(err)
		return
	}
	now := time.Now()
	mode, err := desktopsettings.Appearance(d.effective, now.Hour()*60+now.Minute())
	if err != nil {
		d.refuseThemeReload(err)
		return
	}
	// Background is outside this component. Preserve it in every phase too,
	// so a later ordinary theme transition cannot publish an unaccepted edit.
	for name, phases := range pending.palettes {
		for phase, colors := range phases {
			colors["background"] = d.palettes[name][phase]["background"]
		}
	}
	palette := pending.palettes[stringAt(d.effective, "palette")][mode]
	projection, err := desktopsettings.ProjectTheme(palette, mode, presentation.Contrast)
	if err != nil {
		d.refuseThemeReload(err)
		return
	}
	if !reflect.DeepEqual(pending.palettes, d.palettes) {
		d.publishColorRoles(projection)
		if presentation.Transparency {
			d.emit("Colorset 10 RootTransparent buffer, Tint " + palette["surface"] + " 88")
		}
		if d.fatal != nil {
			// A broken command stream cannot acknowledge native rollback. Do not
			// replace controller state or claim successful rendering/publication.
			d.themeReload = nil
			return
		}
		d.palettes = pending.palettes
		d.lastThemeColors = make(map[string]string, len(palette))
		for name, color := range palette {
			d.lastThemeColors[name] = color
		}
		d.lastThemeAppearance = mode
		if view := d.inspection; view != nil {
			// Keep the current Form and any unsubmitted filter text intact.
			// Its next explicit page/refresh action displays this stale marker.
			view.status = "Theme colors changed; refresh to collect the current policy."
		}
	}
	d.themeReload = nil
	d.emit("Echo FvwmDesktop: theme colors reloaded; wallpaper and other components unchanged.")
}

// Publish the existing role component without rebuilding menus, Forms or their
// bindings. Native colorset consumers retain their own resource/user lifetimes.
func (d *Controller) publishColorRoles(projection desktopsettings.ThemeProjection) {
	for _, role := range projection.Roles {
		d.emit(fmt.Sprintf("Colorset %d Plain, fg %s, bg %s, hi %s, sh %s", role.Colorset, role.Foreground, role.Background, role.Highlight, role.Shadow))
	}
}

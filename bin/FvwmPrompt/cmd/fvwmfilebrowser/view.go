// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"errors"
	"fmt"
	"strconv"
)

func (b *browser) render() error {
	if err := b.retireDirectoryRoles(); err != nil {
		return err
	}
	if err := b.retireDetails(); err != nil {
		return err
	}
	if b.form == ^uint64(0) {
		return errors.New("form generation exhausted")
	}
	oldAlias := b.alias
	b.form++
	alias := "FvwmFiles-" + b.instance + "-" + strconv.FormatUint(b.form, 10) + "-ready"
	b.alias = alias
	b.lastControl = 0
	mode := b.requestedView()
	callback := "Command Echo FVWM_FILES_3 " + b.instance + " " + strconv.FormatUint(b.form, 10) + " "
	commands := []string{}
	if oldAlias != "" {
		commands = append(commands, "KillModule FvwmForm "+oldAlias, "DestroyModuleConfig "+oldAlias+":*")
	}
	line := func(value string) { commands = append(commands, "*"+alias+": "+value) }
	button := func(label, key, action, argument string) {
		line("Button continue \"" + label + "\" " + key)
		line(callback + action + " " + argument + " x$(picks:Base64) $(FormJob:Token)")
	}
	line("UIScale monitor")
	line(`Title "File browser"`)
	line(`Line left`)
	path := "Loading folder"
	if b.snapshot != nil {
		path = string(b.snapshot.Path)
	}
	b.feedbackLabels = make(map[string]string)
	label := func(name, value string) {
		b.feedbackLabels[name] = value
		line(`Label ` + name + ` 56 "` + displayText(value, 120) + `"`)
		line("LabelViewport " + name)
	}
	label("status", b.feedbackStatus())
	line(`Line left`)
	label("watcher", b.watchText())
	line(`Line left`)
	label("progress", b.progressText())
	line(`Line left`)
	label("readiness", b.readinessText())
	line(`Line left`)
	label("counts", b.countText())
	line(`Line left`)
	label("projection", b.projectionTextMode(mode))
	line(`Line left`)
	label("path-label", path)
	line("Line left")
	line(`Input filter 24 ""`)
	button("Filter (Alt-F)", "Alt-F", "filter", "x$(filter:Base64)")
	button("Clear (Alt-C)", "Alt-C", "unfilter", "-")
	line(`Line left`)
	button("Sort (Alt-S)", "Alt-S", "sort", "-")
	button("Reverse (Alt-R)", "Alt-R", "reverse", "-")
	button("View (F7)", "F7", "view", "-")
	line("Line left")
	if mode == "grid" {
		line("List picks 56 3")
		line("ListViewport 18 8")
	} else {
		line("List picks 56 8")
		line("ListViewport 18 1")
	}
	line("ListVersioned")
	line("ListWatch " + b.nativeWatchToken())
	entries := b.model.VisibleEntries()
	for _, item := range entries {
		selected := "off"
		if b.model.Selected[item.ID] {
			selected = "on"
		}
		icon := map[string]string{"directory": "directory", "file": "file", "symlink": "link"}[item.Kind]
		if icon == "" {
			icon = "other"
		}
		line(fmt.Sprintf("ListItem %s \"%s\" %s", selected,
			displayText(item.Label, 90), icon))
	}
	for index, entry := range entries {
		if entry.ID == b.model.Focus {
			line(fmt.Sprintf("ListFocus %d", index+1))
			break
		}
	}
	line("ListNotify " + b.nativeTag("start"))
	line("Line left")
	button("Select visible (Alt-A)", "Alt-A", "select-visible", "-")
	button("Clear selection (Alt-D)", "Alt-D", "clear-selection", "-")
	button("Properties (F2)", "F2", "properties", "-")
	line(`Button continue "Details (F1)" F1`)
	line("Command Echo FVWM_FILES_DETAILS_OPEN_1 " + b.instance + " " + strconv.FormatUint(b.form, 10) + " $(FormJob:Token)")
	for _, item := range b.inspectorLabels() {
		line("Line left")
		label(item.name, item.value)
	}
	line("Line left")
	button("Home (Alt-H)", "Alt-H", "home", "-")
	button("Up (Alt-U)", "Alt-U", "up", "-")
	button("Back (Alt-B)", "Alt-B", "back", "-")
	line("Line left")
	button("Refresh (F5)", "F5", "refresh", "-")
	button("Hidden (F6)", "F6", "hidden", "-")
	button("Watch (F8)", "F8", "watch", "-")
	button("Jobs (F9)", "F9", "jobs", "-")
	line("Line left")
	button("Open (Enter)", "^M", "focused", "-")
	button("Selected (F3)", "F3", "selected", "-")
	button("Terminal (F4)", "F4", "terminal", "-")
	line("Line left")
	button("Open folder with (F12)", "F12", "directory-roles", "-")
	line("Line left")
	line(`Text "Path:"`)
	line(`Input path 32 ""`)
	button("Go (Alt-G)", "Alt-G", "path", "x$(path:Base64)")
	button("Cancel (Alt-X)", "Alt-X", "cancel", "-")
	button("Close (Esc)", "^[", "close", "-")
	for index, action := range b.actions {
		line("Line left")
		key := "Alt-" + strconv.Itoa(index+1)
		button(displayText(action.Label, 32)+" ("+key+")", key, "custom", strconv.Itoa(index))
	}
	line(`Line left`)
	line(`Text "Arrows/PgUp/PgDn: move; Shift: extend; Space: select."`)
	line(`Line left`)
	line(`Text "Alt-letter/digit: labeled actions; Ctrl-U: clear input."`)
	line(`Line left`)
	line(`Text "Wheel: scroll; Tab: fields. Idle close: 5 minutes."`)
	line("CloseCommand Echo FVWM_FILES_3 " + b.instance + " " + strconv.FormatUint(b.form, 10) + " close - x timeout")
	line("Timeout 3000 \"Echo FVWM_FILES_3 " + b.instance + " " + strconv.FormatUint(b.form, 10) + " close - x timeout\" \"\"")
	commands = append(commands, "Module FvwmForm "+alias)
	if err := b.sendCommands(commands); err != nil {
		return err
	}
	if b.native != nil {
		packet, err := b.waitNative(b.nativeTag("start"), 1, 0)
		if err != nil {
			return err
		}
		if err := b.model.SelectNative(b.model.Generation, packet); err != nil {
			return err
		}
	}
	b.nativeGeneration = 1
	b.viewEntries = entries
	b.renderPath, b.renderView = b.currentPath(), mode
	b.replaceSelection = false
	return nil
}

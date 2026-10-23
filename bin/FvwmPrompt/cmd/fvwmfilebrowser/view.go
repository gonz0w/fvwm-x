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
	if b.form == ^uint64(0) {
		return errors.New("form generation exhausted")
	}
	oldAlias := b.alias
	b.form++
	alias := "FvwmFiles-" + b.instance + "-" + strconv.FormatUint(b.form, 10) + "-ready"
	b.alias = alias
	b.lastControl = 0
	b.nativeGeneration = 1
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
	line(`Title "File browser"`)
	line(`Line left`)
	path := "Loading folder"
	if b.snapshot != nil {
		path = string(b.snapshot.Path)
	}
	line(`Label path-label 96 "` + displayText(path, 90) + `"`)
	line(`Line left`)
	line(`Label status 96 "` + displayText(b.status, 120) + `"`)
	line(`Line left`)
	line(`Label watcher 96 "` + displayText(b.watchText(), 120) + `"`)
	line(`Line left`)
	line(`Label projection 96 "` + displayText(b.projectionText(), 120) + `"`)
	line("Line left")
	line(`Input filter 24 ""`)
	button("Apply filter", "", "filter", "x$(filter:Base64)")
	button("Clear filter", "", "unfilter", "-")
	line(`Line left`)
	button("Sort (cycle)", "", "sort", "-")
	button("Reverse order", "", "reverse", "-")
	button("View: "+b.viewing()+" (F7)", "F7", "view", "-")
	line("Line left")
	if b.viewing() == "grid" {
		line("List picks 84 4")
		line("ListGrid 4")
	} else {
		line("List picks 84 12")
	}
	line("ListVersioned")
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
	line(`Label counts 96 "` + b.countText() + `"`)
	line("Line left")
	button("Select visible", "", "select-visible", "-")
	button("Clear selection", "", "clear-selection", "-")
	button("Properties (F2)", "F2", "properties", "-")
	for _, label := range b.inspectorLabels() {
		line("Line left")
		line("Label " + label.name + " 96 \"" + displayText(label.value, 120) + "\"")
	}
	line("Line left")
	button("Home", "", "home", "-")
	button("Up", "", "up", "-")
	button("Back", "", "back", "-")
	button("Refresh (F5)", "F5", "refresh", "-")
	button("Hidden (F6)", "F6", "hidden", "-")
	button("Watch (F8)", "F8", "watch", "-")
	line("Line left")
	button("Open focused (Enter)", "^M", "focused", "-")
	button("Open selected (F3)", "F3", "selected", "-")
	button("Terminal (F4)", "F4", "terminal", "-")
	line("Line left")
	line(`Text "Path:"`)
	line(`Input path 40 ""`)
	button("Go", "", "path", "x$(path:Base64)")
	button("Cancel work", "", "cancel", "-")
	button("Close (Esc)", "^[", "close", "-")
	for index, action := range b.actions {
		line("Line left")
		button(displayText(action.Label, 32), "", "custom", strconv.Itoa(index))
	}
	line(`Line left`)
	line(`Text "Arrows/PgUp/PgDn or wheel; Shift extends selection; Space toggles; Tab moves fields. Idle close: 5 minutes."`)
	line("Timeout 3000 \"Echo FVWM_FILES_3 " + b.instance + " " + strconv.FormatUint(b.form, 10) + " close - x timeout\" \"\"")
	commands = append(commands, "Module FvwmForm "+alias)
	if err := b.sendCommands(commands); err != nil {
		return err
	}
	b.viewEntries = entries
	b.renderPath, b.renderView = b.currentPath(), b.viewing()
	b.replaceSelection = false
	if b.native != nil {
		packet, err := b.waitNative(b.nativeTag("start"), 1, 0)
		if err != nil {
			return err
		}
		if err := b.model.SelectNative(b.model.Generation, packet); err != nil {
			return err
		}
	}
	return nil
}

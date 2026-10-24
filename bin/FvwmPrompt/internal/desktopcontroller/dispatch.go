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
	"strconv"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

func desktopConfiguration(message string) (string, string, bool) {
	const prefix = "*FvwmDesktop"
	if len(message) < len(prefix) || !strings.EqualFold(message[:len(prefix)], prefix) {
		return "", "", false
	}
	text := strings.TrimPrefix(message[len(prefix):], ":")
	key, value, _ := strings.Cut(strings.TrimSpace(text), " ")
	return strings.ToLower(key), strings.TrimSpace(value), true
}
func identifier(text string) bool {
	if len(text) < 1 || len(text) > 64 {
		return false
	}
	for _, c := range text {
		if !(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_' || c == '-') {
			return false
		}
	}
	return true
}
func wordCount(argument string, count int) ([]string, error) {
	words := splitFields(argument)
	if len(words) != count {
		return nil, errors.New("Invalid desktop arguments")
	}
	return words, nil
}
func (d *Controller) Handle(message string) {
	command, argument, _ := strings.Cut(strings.TrimSpace(message), " ")
	if err := d.handle(command, argument); err != nil {
		d.notice(err.Error())
	}
}
func (d *Controller) handle(command, argument string) error {
	switch command {
	case "preview-editor":
		e, changes, err := d.editorValues(argument)
		if err != nil {
			return err
		}
		raw, ok := changes["launcher_opacity"]
		if !ok {
			return errors.New("This editor has no launcher opacity draft")
		}
		if _, _, err := d.prepare(changes); err != nil {
			return err
		}
		if err := d.preview.start(raw.(int), true, &draft{e.scope, e.generation, changes}); err != nil {
			return err
		}
		d.retireEditor()
	case "preview-opacity", "preview-setting":
		words, err := wordCount(argument, 2)
		if err != nil {
			return err
		}
		if words[0] != d.instance {
			return nil
		}
		opacity, err := desktopsettings.Value("launcher_opacity", words[1], true)
		if err != nil {
			return err
		}
		return d.preview.start(opacity.(int), command == "preview-setting", nil)
	case "preview-control":
		words, err := wordCount(argument, 3)
		if err != nil {
			return err
		}
		if words[0] == d.instance {
			return d.preview.action(words[1], words[2])
		}
	case "ipc-request":
		d.ipc(argument)
	case "barrier":
		if identifier(argument) {
			d.emit("Echo " + argument)
		}
	case "set":
		key, value, _ := strings.Cut(argument, " ")
		parsed, err := desktopsettings.Value(key, value, true)
		if err != nil {
			return err
		}
		return d.changeSettings(values{key: parsed})
	case "toggle":
		raw, ok := d.saved[argument].(bool)
		if !ok {
			return errors.New("Unknown setting")
		}
		return d.changeSettings(values{argument: !raw})
	case "reset":
		return d.resetSettings(argument)
	case "reload-settings":
		if argument != "" {
			return errors.New("reload-settings takes no arguments")
		}
		return d.reloadSettings()
	case "settings":
		if argument == "" {
			argument = "all"
		}
		if err := d.settingsMenu(argument); err != nil {
			return err
		}
		d.emit("Menu SlateSettings Monitor c c")
	case "edit-settings":
		if argument == "" {
			argument = "all"
		}
		return d.editSettings(argument, false, nil)
	case "apply-settings":
		_, changes, err := d.editorValues(argument)
		if err != nil {
			return err
		}
		return d.changeSettings(changes)
	case "cancel-settings", "defaults-settings":
		e, err := d.currentEditor(argument)
		if err != nil {
			return err
		}
		if command == "cancel-settings" {
			d.retireEditor()
		} else {
			return d.editSettings(e.scope, true, nil)
		}
	case "query-settings":
		token, scope, _ := strings.Cut(argument, " ")
		if !identifier(token) {
			return errors.New("Invalid settings query token")
		}
		if scope == "" {
			scope = "all"
		}
		result, err := d.query(scope, nil, nil)
		if err != nil {
			return err
		}
		keys, _ := desktopsettings.Scope(scope)
		for _, key := range keys {
			data, _ := json.Marshal(result[key])
			d.emit("Echo desktop-settings-" + token + " " + key + " " + string(data))
		}
		d.emit("Echo desktop-settings-" + token + " done")
	case "times":
		words, err := wordCount(argument, 2)
		if err != nil {
			return err
		}
		return d.changeSettings(values{"day": words[0], "night": words[1]})
	case "schedule":
		d.form("SlateScheduleForm", desktopsettings.Schedule(d.effective), false)
	case "check-config":
		if argument != "" {
			return errors.New("check-config takes no module arguments; it checks local.config")
		}
		return d.startCheck()
	case "check-page":
		words, err := wordCount(argument, 2)
		if err != nil {
			return err
		}
		page, err := strconv.Atoi(words[1])
		if err != nil {
			return err
		}
		return d.showReport(words[0], page)
	case "refresh":
		if width, err := strconv.Atoi(argument); err == nil && width >= 0 {
			d.width = width
		}
		d.clock = ""
		return d.theme(true)
	case "discover":
		d.workerJob("apps", "apps", values{})
	case "search":
		runes := []rune(argument)
		query := string(runes[:min(80, len(runes))])
		d.applicationMenu(&query)
	case "launch":
		index, err := strconv.Atoi(argument)
		if err != nil || index < 0 || index >= len(d.apps) {
			return errors.New("Application selection is out of range")
		}
		app := d.apps[index]
		cwd := ""
		if app.Cwd != nil {
			cwd = *app.Cwd
		}
		if app.Terminal {
			d.term(app.Argv, cwd)
		} else {
			d.spawn(app.Argv, cwd)
		}
	case "terminal":
		d.term(nil, "")
	case "run":
		if argument != "" {
			d.spawn([]string{"/bin/sh", "-c", argument}, "")
		}
	case "calendar":
		now := time.Now()
		first := time.Date(now.Year(), now.Month(), 1, 12, 0, 0, 0, now.Location())
		days := first.AddDate(0, 1, -1).Day()
		lines := []string{fmt.Sprintf("%s %d", now.Month(), now.Year()), "Mo Tu We Th Fr Sa Su"}
		line := ""
		offset := (int(first.Weekday()) + 6) % 7
		for i := 0; i < offset; i++ {
			line += "   "
		}
		for day := 1; day <= days; day++ {
			line += fmt.Sprintf("%2d ", day)
			if (day+offset)%7 == 0 || day == days {
				lines = append(lines, strings.TrimRight(line, " "))
				line = ""
			}
		}
		rows := []desktopsettings.Row{}
		for _, line := range lines {
			rows = append(rows, desktopsettings.Row{line, "Nop"})
		}
		d.menu(desktopsettings.Menu{Name: "SlateCalendar", Title: "Calendar", Rows: rows})
		d.emit("Menu SlateCalendar Monitor c c")
	case "files":
		home, _ := os.UserHomeDir()
		d.openDirectory(home)
	case "files-up":
		d.openDirectory(filepath.Dir(d.directory))
	case "files-terminal":
		d.term(nil, d.directory)
	case "file":
		index, err := strconv.Atoi(argument)
		if err != nil || index < 0 || index >= len(d.files) {
			return errors.New("File selection is out of range")
		}
		file := d.files[index]
		info, err := os.Stat(file.path)
		if err == nil && info.IsDir() {
			d.openDirectory(file.path)
		} else if found("xdg-open") != "" {
			d.spawn([]string{"xdg-open", file.path}, "")
		} else {
			d.term([]string{"less", "--", file.path}, "")
		}
	case "displays":
		if found("xrandr") != "" {
			d.term([]string{"xrandr", "--query"}, "")
		} else {
			d.notice("xrandr is unavailable on this system.")
		}
	case "network":
		if path := found("ifconfig"); path != "" {
			d.term([]string{path, "-a"}, "")
		} else if path := found("ip"); path != "" {
			d.term([]string{path, "address"}, "")
		} else {
			d.notice("No network information command is available.")
		}
	case "monitor":
		d.term([]string{"top"}, "")
	case "screenshot":
		d.screenshot()
	case "lock":
		path := found("xlock")
		if path == "" {
			path = found("slock")
		}
		if path != "" {
			d.spawn([]string{path}, "")
		} else {
			d.notice("No secure X11 locker is available. The screen has NOT been locked.")
		}
	case "volume":
		if argument == "up" || argument == "down" || argument == "mute" {
			d.workerJob("volume", "volume", values{"system": d.system, "action": argument})
		}
	case "confirm":
		if argument == "logout" || argument == "poweroff" || argument == "reboot" {
			d.form("SlateSessionForm", []string{`Title "Session"`, "Line left", "Text " + quote("Confirm "+argument+"? Save your work first."), "Line expand", `Button quit "Cancel" ^[`, "Command Nop", "Button quit " + quote(title(argument)), "Command SendToModule FvwmDesktop session " + argument}, false)
		}
	case "session":
		if argument == "logout" {
			d.emit("Quit")
		} else if argument == "poweroff" || argument == "reboot" {
			d.workerJob("volume", "power", values{"system": d.system, "action": argument})
		}
	}
	return d.fatal
}
func (d *Controller) tick() {
	d.preview.tick()
	if d.editor != nil && !time.Now().Before(d.editor.deadline) {
		d.retireEditor()
	}
	clock := time.Now().Format("Mon 02 Jan  15:04")
	if clock != d.clock {
		d.button("clock", clock)
		d.clock = clock
		if err := d.theme(false); err != nil {
			d.notice(err.Error())
		}
	}
	d.pollJobs()
}

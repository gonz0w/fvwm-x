// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"context"
	"crypto/rand"
	"encoding/base64"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"path/filepath"
	"strconv"
	"strings"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

const formName = "FvwmForm-XDGMenu-Config"

func validResultToken(token string) bool {
	if len(token) != 48 {
		return false
	}
	_, err := hex.DecodeString(token)
	return err == nil
}
func resultPath(userdir, token string) string {
	return filepath.Join(userdir, ".XDGMenu-result-"+token)
}
func formOutput(out io.Writer, userdir, token string, data []byte) error {
	if token != "" {
		return xdgsource.PublishPrivate(resultPath(userdir, token), data)
	}
	_, err := out.Write(data)
	return err
}

type fieldFlags struct {
	records []xdgmenu.Preference
	bytes   int
}

func (f *fieldFlags) String() string { return "" }
func (f *fieldFlags) Set(text string) error {
	key, value, found := strings.Cut(text, "=")
	if !found || len(text) > 3*xdgmenu.MaxValueBytes || len(f.records) >= xdgmenu.MaxItems {
		return errors.New("invalid encoded settings field")
	}
	k, err := base64.RawURLEncoding.DecodeString(key)
	if err != nil {
		return err
	}
	v, err := base64.RawURLEncoding.DecodeString(value)
	if err != nil {
		return err
	}
	if !utf8.Valid(k) || !utf8.Valid(v) || len(k)+len(v) > xdgmenu.MaxValueBytes {
		return errors.New("invalid settings field text")
	}
	if len(k)+len(v) > xdgmenu.MaxSourceBytes-f.bytes {
		return errors.New("settings fields exceed total input limit")
	}
	f.bytes += len(k) + len(v)
	f.records = append(f.records, xdgmenu.Preference{Key: string(k), Value: strings.Trim(string(v), " \t")})
	return nil
}
func settingsPath(userdir string) string { return filepath.Join(userdir, "."+formName) }
func formPreferences(userdir string, fields fieldFlags) ([]xdgmenu.Preference, []byte, error) {
	data, err := xdgsource.ReadSettings(settingsPath(userdir))
	if err != nil && !errors.Is(err, fs.ErrNotExist) {
		return nil, nil, err
	}
	previous, err := xdgmenu.ParsePreferences(data)
	if err != nil {
		return nil, nil, err
	}
	updated := make(map[string]bool)
	for _, field := range fields.records {
		if updated[field.Key] {
			return nil, nil, errors.New("duplicate settings field")
		}
		updated[field.Key] = true
	}
	records := make([]xdgmenu.Preference, 0, len(previous)+len(fields.records))
	for _, record := range previous {
		if !updated[record.Key] {
			records = append(records, record)
		}
	}
	records = append(records, fields.records...)
	encoded, err := xdgmenu.EncodePreferences(records)
	return records, encoded, err
}
func outputPath(value string, env map[string]string) (string, error) {
	if value == "" {
		value = filepath.Join(env["FVWM_USERDIR"], ".XDGMenu")
	}
	if strings.HasPrefix(value, "~/") {
		value = filepath.Join(env["HOME"], value[2:])
	}
	for _, prefix := range []string{"$FVWM_USERDIR/", "${FVWM_USERDIR}/"} {
		if strings.HasPrefix(value, prefix) {
			value = filepath.Join(env["FVWM_USERDIR"], value[len(prefix):])
		}
	}
	if !filepath.IsAbs(value) {
		return "", errors.New("menu output path must be absolute")
	}
	return filepath.Clean(value), nil
}

// Every command placeholder emits URL-base64. Field names are encoded static
// tokens too; filesystem paths and preference text never become command syntax.
func renderForm(ctx context.Context, source *xdgsource.Source, paths xdgmenu.SearchPaths, env map[string]string, userdir string, o options, out io.Writer) error {
	files, err := discover(source, paths, o.InstallPrefix)
	if err != nil {
		return err
	}
	selected, err := xdgmenu.SelectMenus(files, xdgmenu.MenuSelection{Prefix: env["XDG_MENU_PREFIX"], UserDirectory: filepath.Join(paths.Config[0], "menus"), Saved: o.Menus})
	if err != nil {
		return err
	}
	chosen := make(map[string]bool)
	for _, file := range selected {
		chosen[file.Path] = true
	}
	var commands bytes.Buffer
	nonce := make([]byte, 16)
	if _, err := rand.Read(nonce); err != nil {
		return err
	}
	token := hex.EncodeToString(nonce) + "$(FormJob:Token)"
	fmt.Fprintf(&commands, "DestroyModuleConfig %s: *\n", formName)
	line := func(text string) { fmt.Fprintf(&commands, "*%s: %s\n", formName, text) }
	quote := func(value string) (string, error) {
		if len(value) > xdgmenu.MaxValueBytes || !utf8.ValidString(value) || strings.ContainsAny(value, "\x00\r\n") {
			return "", errors.New("invalid settings Form text")
		}
		return xdgmenu.FVWMQuote(value), nil
	}
	line(`Colorset 5`)
	line(`ItemColorset 5`)
	line(`Title "$[gt.Fvwm XDGMenu Config]"`)
	line(`WarpPointer`)
	line(`Line center`)
	line(`Text "$[gt.Available Menus]"`)
	var arguments []string
	addField := func(key, variable, expression string) {
		if expression == "" {
			expression = "$(" + variable + ":Base64)"
		}
		arguments = append(arguments, "--field '"+base64.RawURLEncoding.EncodeToString([]byte(key))+"="+expression+"'")
	}
	for i, file := range files {
		if err := ctx.Err(); err != nil {
			return err
		}
		stem := strings.TrimSuffix(file.Name, ".menu")
		key := "MEN" + stem
		// A name that the historical data format cannot represent is refused
		// before producing any Form configuration, rather than renamed silently.
		if _, err := xdgmenu.EncodePreferences([]xdgmenu.Preference{{Key: key}}); err != nil {
			return err
		}
		if i%4 == 0 {
			line("Line left")
			line("Selection Menus" + strconv.Itoa(i) + " multiple")
		}
		variable := key
		if strings.ContainsAny(key, "?!)\\$(") {
			variable = "Menu" + strconv.Itoa(i)
		}
		state := "off"
		if chosen[file.Path] {
			state = "on"
		}
		label, err := quote(stem)
		if err != nil {
			return err
		}
		line("Choice " + variable + " on " + state + " " + label)
		addField(key, variable, "$("+variable+"?b24)")
	}
	if len(files) == 0 {
		line(`Line center`)
		line(`Text "$[gt.No menus found!]"`)
	}
	line("Separator")
	choiceLabels := map[string]string{
		"IncludeConfig": "Configure", "IncludeRegen": "Regenerate",
		"IncludeBoth": "Both", "IncludeNone": "None",
		"IconsOn": "Icons on", "IconsOff": "Icons off",
		"TitlesOn": "Titles on", "TitlesOff": "Titles off",
	}
	choiceGroup := func(name string, choices []string, current int) {
		line("Line left")
		line("Selection " + name + " single")
		for i, key := range choices {
			state := "off"
			if i == current {
				state = "on"
			}
			line("Choice " + key + " on " + state + " \"" + choiceLabels[key] + "\"")
			addField(key, key, "$("+key+"?b24)")
		}
	}
	include := map[string]int{"config": 0, "regenerate": 1, "both": 2, "none": 3}[o.IncludeItems]
	choiceGroup("Include", []string{"IncludeConfig", "IncludeRegen", "IncludeBoth", "IncludeNone"}, include)
	icons := 1
	if o.Icons {
		icons = 0
	}
	choiceGroup("Icons", []string{"IconsOn", "IconsOff"}, icons)
	titles := 1
	if o.Titles {
		titles = 0
	}
	choiceGroup("Titles", []string{"TitlesOn", "TitlesOff"}, titles)
	path := o.OutputPath
	if path == "" {
		path = filepath.Join(userdir, ".XDGMenu")
	}
	inputs := []struct {
		key, value string
		width      int
	}{
		{"Size", strconv.Itoa(o.Size), 5}, {"Theme", o.Theme, 20}, {"DirIcon", o.DirIcon, 20},
		{"AppIcon", o.AppIcon, 20}, {"Title", o.Title, 20}, {"InsertInto", o.InsertInto, 20},
		{"TermCmd", o.TermCmd, 30}, {"IconDir", o.IconDir, 30}, {"Path", path, 30},
	}
	for _, input := range inputs {
		label, err := quote(input.key + ":")
		if err != nil {
			return err
		}
		value, err := quote(input.value)
		if err != nil {
			return err
		}
		line("Line left")
		line("Text " + label)
		if len(value) <= 700 {
			line(fmt.Sprintf("Input %s %d %s", input.key, input.width, value))
		} else {
			line(fmt.Sprintf("Input %s %d \"\"", input.key, input.width))
			for remaining := input.value; remaining != ""; {
				n := len(remaining)
				if n > 256 {
					n = 256
					for n > 0 && !utf8.RuneStart(remaining[n]) {
						n--
					}
				}
				line("InputAppend " + xdgmenu.FVWMQuote(remaining[:n]))
				remaining = remaining[n:]
			}
		}
		addField(input.key, input.key, "")
	}
	line("Line expand")
	for index, button := range []struct{ label, action string }{{"Export Menu", "export"}, {"Regenerate", "regenerate"}, {"Save", "save"}} {
		line(`Button continue "$[gt.` + button.label + `]" F` + strconv.Itoa(index+1))
		command := "!fvwm-menu-desktop --form-action " + button.action + " --result-token " + token
		if len(command)+len(strings.Join(arguments, " ")) > xdgmenu.MaxCommandBytes {
			return errors.New("settings form exceeds command limit")
		}
		line("Command " + command)
		for _, argument := range arguments {
			line("CommandAppend  " + argument)
		}
		line("Command PipeRead `fvwm-menu-desktop --form-result " + token + "`")
	}
	line(`Button restart "$[gt.Reset]"`)
	line("Command Nop")
	line(`Button continue "$[gt.Help]"`)
	line("Command Module FvwmForm FvwmForm-XDGMenuHelp")
	line(`Button quit "$[gt.Close]"`)
	line("Command Nop")
	fmt.Fprintln(&commands, "Module FvwmForm", formName)
	if commands.Len() > xdgmenu.MaxRenderBytes {
		return errors.New("settings form exceeds output limit")
	}
	if bytes.Count(commands.Bytes(), []byte{'\n'}) > maxReadCommands {
		return errors.New("settings form exceeds the core Read command-count limit")
	}
	_, err = out.Write(commands.Bytes())
	return err
}

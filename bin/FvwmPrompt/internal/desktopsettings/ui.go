// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import (
	"errors"
	"fmt"
	"strings"
	"unicode"
)

const highContrastHint = "High contrast uses opaque surfaces; requested transparency and opacity are retained."

const wallpaperRetentionHint = "Wallpaper is retained; turn off Keep current wallpaper to apply."

func label(value string, limit int) string {
	var out []rune
	for _, c := range value {
		if unicode.IsPrint(c) && !strings.ContainsRune("\\\"$%&", c) {
			out = append(out, c)
		}
	}
	out = []rune(strings.TrimSpace(string(out)))
	if len(out) > limit {
		out = out[:limit]
	}
	return string(out)
}

func quote(value string) string { return `"` + label(value, 1024) + `"` }

func text(value any) string { return strings.ToLower(fmt.Sprint(value)) }

func title(value string) string {
	if value == "" {
		return value
	}
	return strings.ToUpper(value[:1]) + value[1:]
}

type Row [2]string
type Menu struct {
	Name  string `json:"name"`
	Title string `json:"title"`
	Rows  []Row  `json:"rows"`
}
type EditorPlan struct {
	Keys  []string `json:"keys"`
	Lines []string `json:"lines"`
}

func Editor(s Snapshot,
	scope string,
	defaults bool,
	draft Values,
	token string) (EditorPlan,
	error) {
	var plan EditorPlan
	if _, err := identity(token); err != nil {
		return plan, err
	}
	keys, err := Scope(scope)
	if err != nil {
		return plan, err
	}
	presentation, err := ResolvePresentation(s.Effective)
	if err != nil {
		return plan, err
	}
	groups := []string{}
	for _, key := range keys {
		item, _ := find(key)
		if !has(groups, item.Group) {
			groups = append(groups, item.Group)
		}
	}
	for _, group := range groups {
		for _, key := range keys {
			item, _ := find(key)
			if item.Group == group {
				plan.Keys = append(plan.Keys, key)
			}
		}
	}
	if draft != nil {
		if len(draft) != len(keys) {
			return plan, errors.New("Incomplete settings editor draft")
		}
		for _, key := range keys {
			if _, err := Value(key, draft[key], false); err != nil {
				return plan, err
			}
		}
	}
	lines := []string{`Title "Desktop settings"`,
		"Colorset 2",
		"ItemColorset 12",
		"Line left",
		`Text "Edit saved preferences. Apply saves them together."`}
	if presentation.Contrast == "high" || has(keys, "contrast") {
		lines = append(lines, "Line left", "Text "+quote(highContrastHint))
	}
	previous := ""
	for _, key := range plan.Keys {
		item, _ := find(key)
		value := s.Saved[key]
		if defaults {
			value = item.Default
		} else if draft != nil {
			value = draft[key]
		}
		if item.Group != previous {
			lines = append(lines, "Line left", "Text "+quote(title(item.Group)))
			if item.Group == "wallpaper" && s.Effective["keep_wallpaper"] == true {
				lines = append(lines, "Line left", "Text "+quote(wallpaperRetentionHint))
			}
			previous = item.Group
		}
		lines = append(lines, "Line left", "Text "+quote(item.Description))
		if item.Type == "time" || item.Type == "integer" {
			width := 5
			if item.Type == "integer" {
				width = 3
			}
			lines = append(lines, fmt.Sprintf("Input %s %d %s", key, width, quote(text(value))))
		} else {
			choices := []any{}
			for _, c := range item.Choices {
				choices = append(choices, c)
			}
			if item.Type == "boolean" {
				choices = []any{true, false}
			}
			lines = append(lines, "Selection "+key+" single")
			for _, choice := range choices {
				name := text(choice)
				caption := title(name)
				if b, ok := choice.(bool); ok {
					caption = "Off"
					if b {
						caption = "On"
					}
				}
				selected := "off"
				if text(choice) == text(value) {
					selected = "on"
				}
				lines = append(lines, "Choice "+key+"_"+name+" "+name+" "+selected+" "+quote(caption))
			}
		}
		if _, ok := s.Overrides[key]; ok {
			lines = append(lines,
				"Line left",
				"Text "+quote("Effective request: "+text(s.Effective[key])+" (module configuration override)"))
		}
	}
	if s.PersistenceError != nil {
		lines = append(lines, "Line left", "Text "+quote(*s.PersistenceError))
	}
	action := "SendToModule FvwmDesktop "
	fields := []string{}
	for _, key := range plan.Keys {
		fields = append(fields, "$("+key+")")
	}
	arguments := token + " " + strings.Join(fields, " ")
	lines = append(lines,
		"Line left",
		`Text "Cancel or closing this window leaves preferences unchanged."`,
		"Line expand",
		`Button quit "Cancel" ^[`,
		"Command "+action+"cancel-settings "+token,
		`Button continue "Defaults (F2)" F2`,
		"Command "+action+"defaults-settings "+token,
		`Button continue "Apply (F3)" F3`,
		"Command "+action+"apply-settings "+arguments,
		fmt.Sprintf("Timeout %d %s \"\"",
			EditorSeconds*10,
			quote(action+"cancel-settings "+token)))
	if has(plan.Keys, "launcher_opacity") {
		lines = append(lines,
			"Line left",
			`Text "Preview tests opacity only, then returns to this captured draft."`,
			`Text "Opacity preview is unavailable while high contrast is selected."`,
			`Button continue "Preview opacity (F4)" F4`,
			"Command "+action+"preview-editor "+arguments)
	}
	plan.Lines = lines
	return plan, nil
}

func Menus(s Snapshot, scope string) ([]Menu, error) {
	query, err := Query(s, scope, true)
	if err != nil {
		return nil, err
	}
	keys, _ := Scope(scope)
	menus := []Menu{}
	rows := []Row{}
	for _, key := range keys {
		item := query[key].(map[string]any)
		name := "SlateSetting_" + key
		choices := []any{}
		setting, _ := find(key)
		for _, c := range setting.Choices {
			choices = append(choices, c)
		}
		if setting.Type == "boolean" {
			choices = []any{true, false}
		}
		options := []Row{{setting.Description,
			"Greyed"},
			{"Saved: " + text(s.Saved[key]),
				"Greyed"},
			{"Source: " + text(item["source"]),
				"Greyed"}}
		if resolution, ok := item["resolution"].(string); ok {
			options = append(options, Row{"Selection: " + text(item["resolved"]) + " (" + resolution + ")", "Greyed"})
		}
		if item["runtime_constraint"] == "wallpaper-retained" {
			options = append(options, Row{wallpaperRetentionHint, "Greyed"})
		}
		if item["runtime_constraint"] == "high-contrast" {
			options = append(options, Row{highContrastHint, "Greyed"})
		}
		if len(choices) > 0 {
			for _, value := range choices {
				options = append(options,
					Row{text(value),
						"SendToModule FvwmDesktop set " + key + " " + text(value)})
			}
		} else if setting.Type == "integer" {
			options = append(options,
				Row{"Edit saved value...",
					"SendToModule FvwmDesktop edit-settings " + key})
		} else {
			options = append(options, Row{"Edit schedule...", "SendToModule FvwmDesktop schedule"})
		}
		options = append(options, Row{"Reset this setting", "SendToModule FvwmDesktop reset " + key})
		menus = append(menus, Menu{name, key, options})
		caption := key + ": " + text(s.Effective[key])
		if item["runtime_constraint"] == "high-contrast" {
			caption = key + ": " + text(item["resolved"]) + " (high contrast; requested " + text(s.Effective[key]) + ")"
		}
		rows = append(rows, Row{caption, "Popup " + name})
	}
	opacity := []Row{}
	if s.Effective["contrast"] == "high" {
		opacity = append(opacity, Row{"Opacity preview is unavailable in high contrast.", "Greyed"})
	}
	for _, n := range []int{70, 85, 94, 100} {
		if s.Effective["contrast"] == "high" {
			break
		}
		opacity = append(opacity,
			Row{fmt.Sprint(n) + "%",
				"SendToModule FvwmDesktop preview-setting " + s.Instance + " " + fmt.Sprint(n)})
	}
	menus = append(menus, Menu{"SlateOpacityPreview", "Runtime launcher opacity", opacity})
	rows = append(rows,
		Row{"Preview launcher opacity...",
			"Popup SlateOpacityPreview"},
		Row{"",
			"Nop"},
		Row{"Edit saved preferences...",
			"SendToModule FvwmDesktop edit-settings " + scope},
		Row{"Reset " + scope,
			"SendToModule FvwmDesktop reset " + scope},
		Row{"Reload saved settings",
			"SendToModule FvwmDesktop reload-settings"},
		Row{"Check local.config...",
			"SendToModule FvwmDesktop check-config"},
		Row{"Configuration inspector...", "SendToModule FvwmDesktop inspect all"},
		Row{"FVWM commands submitted; external Style/Colorset overrides are unobserved",
			"Greyed"})
	if s.PersistenceError != nil {
		rows = append(rows, Row{*s.PersistenceError, "Greyed"})
	}
	return append(menus, Menu{"SlateSettings", "Desktop settings", rows}), nil
}

func AppearanceMenu(settings Values) Menu {
	selected := func(caption, value string) string {
		if settings["palette"] == value || settings["appearance"] == value {
			return "* " + caption
		}
		return "  " + caption
	}
	rows := []Row{}
	for _, name := range registry[0].Choices {
		rows = append(rows,
			Row{selected(title(name),
				name),
				"SendToModule FvwmDesktop set palette " + name})
	}
	rows = append(rows, Row{"", "Nop"})
	for _, name := range []string{"light", "dark", "auto"} {
		rows = append(rows,
			Row{selected(title(name),
				name),
				"SendToModule FvwmDesktop set appearance " + name})
	}
	rows = append(rows,
		Row{"Day " + text(settings["day"]) + " / Night " + text(settings["night"]),
			"SendToModule FvwmDesktop schedule"})
	for _, pair := range [][2]string{{"transparency",
		"Transparency"},
		{"motion",
			"Motion"},
		{"keep_wallpaper",
			"Keep current wallpaper"}} {
		state := "  Off"
		if settings[pair[0]] == true {
			state = "  On"
		}
		if pair[0] == "transparency" && settings["contrast"] == "high" {
			state = "  Off (high contrast; requested " + text(settings[pair[0]]) + ")"
		}
		rows = append(rows, Row{pair[1] + state, "SendToModule FvwmDesktop toggle " + pair[0]})
	}
	rows = append(rows,
		Row{"Configuration inspector...", "SendToModule FvwmDesktop inspect all"},
		Row{"Refresh application list",
			"SendToModule FvwmDesktop discover"},
		Row{"Desktop settings...",
			"SendToModule FvwmDesktop settings"},
		Row{"Edit saved preferences...",
			"SendToModule FvwmDesktop edit-settings"})
	rows = append(rows, Row{"", "Nop"})
	for _, contrast := range []string{"normal", "high"} {
		caption := "  " + title(contrast) + " contrast"
		if settings["contrast"] == contrast {
			caption = "* " + title(contrast) + " contrast"
		}
		rows = append(rows, Row{caption, "SendToModule FvwmDesktop set contrast " + contrast})
	}
	if settings["contrast"] == "high" {
		rows = append(rows, Row{highContrastHint, "Greyed"})
	}
	return Menu{"SlateAppearance", "Appearance", rows}
}

func Schedule(settings Values) []string {
	return []string{`Title "Day / night schedule"`,
		"Line left",
		`Text "Day starts (HH:MM)"`,
		"Input day 5 " + quote(text(settings["day"])),
		"Line left",
		`Text "Night starts (HH:MM)"`,
		"Input night 5 " + quote(text(settings["night"])),
		"Line expand",
		`Button quit "Save" ^M`,
		"Command SendToModule FvwmDesktop times $(day) $(night)",
		`Button quit "Cancel" ^[`,
		"Command Nop"}
}

func Preview(instance, view string, opacity int, saved bool) ([]string, error) {
	if _, err := identity(instance); err != nil {
		return nil, err
	}
	if len(view) != 16 {
		return nil, errors.New("Invalid preview view")
	}
	if _, err := identity(view + view); err != nil {
		return nil, err
	}
	if opacity < 0 || opacity > 100 {
		return nil, errors.New("Invalid menu opacity")
	}
	callback := "Command SendToModule FvwmDesktop preview-control " + instance + " " + view
	suffix := "% (runtime only)"
	hint := "Preferences stay unchanged. Theme updates or restart replace it."
	if saved {
		suffix = "%"
		hint = "Keep is runtime only. Keep and save writes this preference."
	}
	lines := []string{`Title "Preview menu opacity"`,
		"Colorset 2",
		"ItemColorset 12",
		"Line left",
		`Text "Launcher opacity: ` + fmt.Sprint(opacity) + suffix + `"`,
		"Line left",
		`Text "Keep or revert within 15 seconds."`,
		"Line left",
		`Text "Close the window to let the deadline restore the old value."`,
		"Line left",
		"Text " + quote(hint),
		"Line expand",
		`Button continue "Revert" ^[`,
		callback + " revert",
		`Button continue "Keep current value" F1`,
		callback + " confirm"}
	if saved {
		lines = append(lines, `Button continue "Keep and save (F2)" F2`, callback+" save")
	}
	// This independent native consumer timer never confirms or commits a file.
	return append(lines, `Timeout 160 Nop ""`), nil
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package desktopsettings owns data-only preference policy and native UI plans.
// It neither writes preference files nor publishes FVWM commands. The resident
// controller retains commit, generation, editor and preview lifetime authority.
package desktopsettings

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"strconv"
	"strings"
	"unicode/utf8"
)

const StateVersion = 4
const StateLimit = 8192
const EditorSeconds = 300

type Values = map[string]any

type Setting struct {
	Key         string   `json:"-"`
	Type        string   `json:"type"`
	Default     any      `json:"default"`
	Choices     []string `json:"choices,omitempty"`
	Minimum     any      `json:"minimum,omitempty"`
	Maximum     any      `json:"maximum,omitempty"`
	Group       string   `json:"group"`
	Description string   `json:"description"`
}

// Order is part of menu/editor presentation. Return copies to callers; no caller
// may mutate the registry or turn one operation's draft into shared policy.
var registry = []Setting{
	{Key: "palette",
		Type:    "enum",
		Default: "slate",
		Choices: []string{"slate",
			"dune",
			"grove",
			"ink"},
		Group:       "appearance",
		Description: "Desktop color palette"},
	{Key: "appearance",
		Type:    "enum",
		Default: "auto",
		Choices: []string{"light",
			"dark",
			"auto"},
		Group:       "appearance",
		Description: "Light, dark or scheduled appearance"},
	{Key: "day",
		Type:        "time",
		Default:     "07:00",
		Minimum:     "00:00",
		Maximum:     "23:59",
		Group:       "schedule",
		Description: "Local time when light appearance starts"},
	{Key: "night",
		Type:        "time",
		Default:     "19:00",
		Minimum:     "00:00",
		Maximum:     "23:59",
		Group:       "schedule",
		Description: "Local time when dark appearance starts"},
	{Key: "transparency",
		Type:        "boolean",
		Default:     true,
		Group:       "effects",
		Description: "Tinted root background and menu snapshots"},
	{Key: "motion",
		Type:        "boolean",
		Default:     false,
		Group:       "effects",
		Description: "Menu repositioning and window shade steps"},
	{Key: "keep_wallpaper",
		Type:        "boolean",
		Default:     false,
		Group:       "appearance",
		Description: "Retain the current palette wallpaper"},
	{Key: "launcher_opacity",
		Type:        "integer",
		Default:     94,
		Minimum:     0,
		Maximum:     100,
		Group:       "effects",
		Description: "Launcher opacity (percent)"},
	{Key: "wallpaper_palette",
		Type:        "enum",
		Default:     "follow",
		Choices:     []string{"follow", "slate", "dune", "grove", "ink"},
		Group:       "wallpaper",
		Description: "Wallpaper palette (follow uses the desktop palette)"},
	{Key: "wallpaper_appearance",
		Type:        "enum",
		Default:     "follow",
		Choices:     []string{"follow", "light", "dark"},
		Group:       "wallpaper",
		Description: "Wallpaper appearance (follow uses the desktop schedule)"},
	{Key: "contrast",
		Type:        "enum",
		Default:     "normal",
		Choices:     []string{"normal", "high"},
		Group:       "appearance",
		Description: "Normal or high contrast desktop presentation"},
}

func Registry() map[string]Setting {
	result := map[string]Setting{}
	for _, item := range registry {
		item.Choices = append([]string(nil), item.Choices...)
		result[item.Key] = item
	}
	return result
}

func Defaults() Values {
	result := Values{"wallpaper": Values{"palette": "slate", "appearance": "dark"}}
	for _, item := range registry {
		result[item.Key] = item.Default
	}
	return result
}

func find(key string) (Setting, bool) {
	for _, item := range registry {
		if item.Key == key {
			return item, true
		}
	}
	return Setting{}, false
}

func has(values []string, value string) bool {
	for _, v := range values {
		if v == value {
			return true
		}
	}
	return false
}

func Minute(value string) (int, error) {
	if len(value) != 5 || value[2] != ':' {
		return 0, errors.New("time must be HH:MM")
	}
	for _, i := range []int{0, 1, 3, 4} {
		if value[i] < '0' || value[i] > '9' {
			return 0, errors.New("time must be HH:MM")
		}
	}
	hour, _ := strconv.Atoi(value[:2])
	minute, _ := strconv.Atoi(value[3:])
	if hour > 23 {
		return 0, errors.New("hour must be 00..23")
	}
	if minute > 59 {
		return 0, errors.New("time must be HH:MM")
	}
	return hour*60 + minute, nil
}

func integer(value any) (int, bool) {
	switch v := value.(type) {
	case int:
		return v, true
	case json.Number:
		n, e := strconv.Atoi(string(v))
		return n, e == nil
	}
	return 0, false
}

func Value(key string, value any, text bool) (any, error) {
	item, ok := find(key)
	if !ok {
		return nil, fmt.Errorf("Unknown setting: %s", key)
	}
	valid := false
	switch item.Type {
	case "boolean":
		if s, ok := value.(string); text && ok {
			switch strings.ToLower(s) {
			case "on", "true", "yes", "1":
				value = true
			case "off", "false", "no", "0":
				value = false
			}
		}
		_, valid = value.(bool)
	case "integer":
		if s, ok := value.(string); text && ok && len(s) >= 1 && len(s) <= 3 {
			digits := true
			for _, c := range s {
				digits = digits && c >= '0' && c <= '9'
			}
			if digits {
				value, _ = strconv.Atoi(s)
			}
		}
		n, ok := integer(value)
		valid = ok && n >= 0 && n <= 100
		if valid {
			value = n
		}
	case "enum":
		s, ok := value.(string)
		valid = ok && has(item.Choices, s)
	case "time":
		s, ok := value.(string)
		if ok {
			_, err := Minute(s)
			if err != nil {
				return nil, err
			}
			valid = true
		}
	}
	if !valid {
		return nil, fmt.Errorf("Invalid value for %s", key)
	}
	return value, nil
}

func Scope(scope string) ([]string, error) {
	keys := []string{}
	for _, item := range registry {
		if scope == "all" || scope == item.Key || scope == "group:"+item.Group {
			keys = append(keys, item.Key)
		}
	}
	if len(keys) == 0 {
		return nil, fmt.Errorf("Unknown settings scope: %s", scope)
	}
	return keys, nil
}

func Checked(values Values) (Values, error) {
	if len(values) != len(registry)+1 {
		return nil, errors.New("Invalid settings fields")
	}
	result := Values{}
	for _, item := range registry {
		value, err := Value(item.Key, values[item.Key], false)
		if err != nil {
			return nil, err
		}
		result[item.Key] = value
	}
	if result["day"] == result["night"] {
		return nil, errors.New("Day and night times must be different")
	}
	wallpaper, ok := values["wallpaper"].(map[string]any)
	if !ok || len(wallpaper) != 2 || !has(registry[0].Choices,
		stringValue(wallpaper["palette"])) || !has([]string{"light",
		"dark"},
		stringValue(wallpaper["appearance"])) {
		return nil, errors.New("Invalid retained wallpaper")
	}
	result["wallpaper"] = Values{"palette": wallpaper["palette"],
		"appearance": wallpaper["appearance"]}
	return result, nil
}

func Migrate(values Values) Values {
	result := Defaults()
	for _, item := range registry {
		if value, err := Value(item.Key, values[item.Key], false); err == nil {
			result[item.Key] = value
		}
	}
	if result["day"] == result["night"] {
		result["day"], result["night"] = "07:00", "19:00"
	}
	if wallpaper,
		ok := values["wallpaper"].(map[string]any); ok && has(registry[0].Choices,
		stringValue(wallpaper["palette"])) && has([]string{"light",
		"dark"},
		stringValue(wallpaper["appearance"])) {
		result["wallpaper"] = Values{"palette": wallpaper["palette"],
			"appearance": wallpaper["appearance"]}
	}
	return result
}

func Appearance(settings Values, minute int) (string, error) {
	if minute < 0 || minute >= 1440 {
		return "", errors.New("Invalid local minute")
	}
	if settings["appearance"] != "auto" {
		return stringValue(settings["appearance"]), nil
	}
	day, err := Minute(stringValue(settings["day"]))
	if err != nil {
		return "", err
	}
	night, err := Minute(stringValue(settings["night"]))
	if err != nil {
		return "", err
	}
	light := day <= minute && minute < night
	if day > night {
		light = minute >= day || minute < night
	}
	if light {
		return "light", nil
	}
	return "dark", nil
}

func stringValue(v any) string { s, _ := v.(string); return s }

// DecodeJSON refuses ambiguity at every object depth. UseNumber preserves the
// distinction between integer tokens, floats and booleans in stored settings.
func DecodeJSON(data []byte) (any, error) {
	if len(data) > 65536 || !utf8.Valid(data) {
		return nil, errors.New("Invalid settings JSON")
	}
	d := json.NewDecoder(bytes.NewReader(data))
	d.UseNumber()
	var read func(int) (any, error)
	read = func(depth int) (any, error) {
		if depth > 16 {
			return nil, errors.New("Settings JSON nesting exceeds limit")
		}
		token, err := d.Token()
		if err != nil {
			return nil, err
		}
		switch t := token.(type) {
		case json.Delim:
			if t == '{' {
				value := Values{}
				for d.More() {
					key, err := d.Token()
					if err != nil {
						return nil, err
					}
					name, ok := key.(string)
					if !ok {
						return nil, errors.New("Invalid settings field")
					}
					if _, found := value[name]; found {
						return nil, errors.New("Duplicate settings field: " + name)
					}
					v, err := read(depth + 1)
					if err != nil {
						return nil, err
					}
					value[name] = v
				}
				_, err = d.Token()
				return value, err
			}
			if t == '[' {
				value := []any{}
				for d.More() {
					v, err := read(depth + 1)
					if err != nil {
						return nil, err
					}
					value = append(value, v)
				}
				_, err = d.Token()
				return value, err
			}
			return nil, errors.New("Invalid JSON delimiter")
		default:
			return t, nil
		}
	}
	value, err := read(0)
	if err != nil {
		return nil, err
	}
	if _, err = d.Token(); err != io.EOF {
		return nil, errors.New("Trailing settings JSON")
	}
	return value, nil
}

func DecodeState(data []byte) (Values, error) {
	if len(data) > StateLimit {
		return nil, errors.New("Settings file exceeds 8192 bytes")
	}
	decoded, err := DecodeJSON(data)
	if err != nil {
		return nil, err
	}
	values, ok := decoded.(map[string]any)
	if !ok {
		return nil, errors.New("Settings must be a JSON object")
	}
	version, exists := values["schema_version"]
	if !exists {
		return Migrate(values), nil
	}
	n, ok := integer(version)
	if !ok || n < 1 || n > StateVersion {
		return nil, errors.New("Unsupported settings schema; file was preserved")
	}
	delete(values, "schema_version")
	// Validate the historical field count before supplying only the defaults
	// absent in that schema. Checked then rejects unknown, missing or bad fields.
	expected := map[int]int{1: 8, 2: 9, 3: 11, 4: 12}[n]
	if len(values) != expected {
		return nil, errors.New("Invalid settings fields")
	}
	for _, field := range []struct {
		key     string
		version int
		value   any
	}{
		{"launcher_opacity", 2, 94},
		{"wallpaper_palette", 3, "follow"},
		{"wallpaper_appearance", 3, "follow"},
		{"contrast", 4, "normal"},
	} {
		if n < field.version {
			if _, exists := values[field.key]; exists {
				return nil, errors.New("Invalid legacy settings fields")
			}
			values[field.key] = field.value
		}
	}
	return Checked(values)
}

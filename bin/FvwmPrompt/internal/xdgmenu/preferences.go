// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"bytes"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"unicode/utf8"
)

const preferencePrefix = "*FvwmForm-XDGMenu-ConfigDefault:"

// Preference retains the legacy Form's unquoted value tail, including spaces.
// Records are data, not FVWM commands. A consumer must read them directly;
// arbitrary values must not be fed through shell text or the core Read engine.
type Preference struct {
	Key   string
	Value string
}

func preferenceKey(key string) bool {
	return key != "" && len(key) <= MaxValueBytes && utf8.ValidString(key) &&
		!strings.ContainsFunc(key, func(c rune) bool { return c <= 32 || c == 127 })
}

func preferenceValue(value string) bool {
	return len(value) <= MaxValueBytes && utf8.ValidString(value) &&
		!strings.ContainsAny(value, "\x00\r\n")
}

func ParsePreferences(data []byte) ([]Preference, error) {
	if len(data) > MaxSourceBytes || !utf8.Valid(data) || bytes.IndexByte(data, 0) >= 0 {
		return nil, errors.New("invalid or excessive menu preference source")
	}
	var result []Preference
	remaining := string(data)
	for line := 1; remaining != ""; line++ {
		text, tail, _ := strings.Cut(remaining, "\n")
		remaining = tail
		if line > MaxItems || len(text) > MaxValueBytes {
			return nil, errors.New("menu preference source exceeds line limits")
		}
		text = strings.Trim(text, " \t\r")
		if text == "" || strings.HasPrefix(text, "#") {
			continue
		}
		if !strings.HasPrefix(text, preferencePrefix) {
			// Existing files may contain unrelated module directives. They
			// have no meaning to this private data reader and are not run.
			continue
		}
		text = strings.TrimLeft(text[len(preferencePrefix):], " \t")
		end := strings.IndexAny(text, " \t")
		key, value := text, ""
		if end >= 0 {
			key, value = text[:end], strings.Trim(text[end:], " \t")
		}
		if !preferenceKey(key) || !preferenceValue(value) {
			return nil, fmt.Errorf("invalid menu preference at line %d", line)
		}
		result = append(result, Preference{Key: key, Value: value})
	}
	return result, nil
}

// EncodePreferences emits the existing single-line record format, without
// interpreting dollar signs, quotes or backslashes. It performs no publication.
func EncodePreferences(records []Preference) ([]byte, error) {
	if len(records) > MaxItems {
		return nil, errors.New("too many menu preferences")
	}
	var out strings.Builder
	for _, record := range records {
		if !preferenceKey(record.Key) || !preferenceValue(record.Value) ||
			strings.Trim(record.Value, " \t") != record.Value {
			return nil, errors.New("menu preference cannot be represented in legacy data format")
		}
		length := len(preferencePrefix) + len(record.Key) + len(record.Value) + 3
		if length-1 > MaxValueBytes || length > MaxSourceBytes-out.Len() {
			return nil, errors.New("encoded menu preferences exceed source limits")
		}
		out.WriteString(preferencePrefix + " " + record.Key + " " + record.Value + "\n")
	}
	return []byte(out.String()), nil
}

type ProviderDefaults struct {
	Icons         bool
	Size          int
	Titles        bool
	Theme         string
	Title         string
	InsertInto    string
	InstallPrefix string
	IconDir       string
	DirIcon       string
	AppIcon       string
	IncludeItems  string
	TermCmd       string
	OutputPath    string
	Menus         []string
}

// DefaultsFromPreferences applies records in file order, retaining the legacy
// names and empty-field behavior. CLI overrides belong to the later owner.
// Unknown fields stay available in ParsePreferences but do not alter defaults.
func DefaultsFromPreferences(records []Preference) (ProviderDefaults, error) {
	d := ProviderDefaults{Size: 24, Titles: true, Theme: "gnome", Title: "XDGMenu",
		IconDir: "~/.fvwm/icons", DirIcon: "gnome-fs-directory", AppIcon: "gnome-applications",
		IncludeItems: "both", TermCmd: "xterm -e"}
	if len(records) > MaxItems {
		return ProviderDefaults{}, errors.New("too many menu preferences")
	}
	bytes := MaxSourceBytes
	menuOrder := []string{}
	menus := make(map[string]bool)
	for _, record := range records {
		key, value := record.Key, record.Value
		if !preferenceKey(key) || !preferenceValue(value) || len(key)+len(value) > bytes {
			return ProviderDefaults{}, errors.New("invalid or excessive menu preferences")
		}
		bytes -= len(key) + len(value)
		if strings.HasPrefix(key, "MEN") && len(key) > 3 {
			name := key[3:]
			if _, present := menus[name]; !present {
				menuOrder = append(menuOrder, name)
			}
			menus[name] = value == "on"
			continue
		}
		if value == "" {
			continue
		}
		switch key {
		case "Size":
			size, err := strconv.Atoi(value)
			if err != nil {
				return ProviderDefaults{}, errors.New("invalid menu preference icon size")
			}
			d.Size = size
		case "IconsOn", "IconsOff", "TitlesOn", "TitlesOff":
			if value != "on" {
				continue
			}
			switch key {
			case "IconsOn":
				d.Icons = true
			case "IconsOff":
				d.Icons = false
			case "TitlesOn":
				d.Titles = true
			case "TitlesOff":
				d.Titles = false
			}
		case "IncludeConfig", "IncludeRegen", "IncludeBoth", "IncludeNone":
			if value == "on" {
				switch key {
				case "IncludeConfig":
					d.IncludeItems = "config"
				case "IncludeRegen":
					d.IncludeItems = "regenerate"
				case "IncludeBoth":
					d.IncludeItems = "both"
				case "IncludeNone":
					d.IncludeItems = "none"
				}
			}
		case "Theme":
			d.Theme = value
		case "Title":
			d.Title = value
		case "InsertInto":
			d.InsertInto = value
		case "Installprefix":
			d.InstallPrefix = value
		case "IconDir":
			d.IconDir = value
		case "DirIcon":
			d.DirIcon = value
		case "AppIcon":
			d.AppIcon = value
		case "TermCmd":
			d.TermCmd = value
		case "Path":
			d.OutputPath = value
		}
	}
	for _, name := range menuOrder {
		if menus[name] {
			d.Menus = append(d.Menus, name)
		}
	}
	return d, nil
}

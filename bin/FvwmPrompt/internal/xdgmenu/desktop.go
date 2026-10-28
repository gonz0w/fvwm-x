// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package xdgmenu contains pure input policy for the full desktop menu
// provider. It owns no environment, processes, files or authoritative X state.
package xdgmenu

import (
	"bytes"
	"errors"
	"fmt"
	"strings"
	"unicode/utf8"
)

const MaxSourceBytes = 8 * 1024 * 1024
const MaxValueBytes = 64 * 1024
const MaxItems = 100000
const MaxDepth = 128

// Desktop keeps unexpanded values, including separately named action groups.
// Field-code expansion must follow string unescaping, exactly once.
type Desktop struct {
	Groups map[string]map[string]string
}

func ParseDesktop(data []byte) (*Desktop, error) {
	if len(data) > MaxSourceBytes || !utf8.Valid(data) || bytes.IndexByte(data, 0) >= 0 {
		return nil, errors.New("desktop entry exceeds limits or is not valid UTF-8 text")
	}
	d := &Desktop{Groups: make(map[string]map[string]string)}
	group := ""
	items := 0
	remaining := string(data)
	for line := 1; remaining != ""; line++ {
		text, tail, _ := strings.Cut(remaining, "\n")
		remaining = tail
		if line > MaxItems {
			return nil, errors.New("desktop entry exceeds line limit")
		}
		text = strings.Trim(text, " \t\r")
		if text == "" || strings.HasPrefix(text, "#") {
			continue
		}
		items++
		if items > MaxItems || len(text) > MaxValueBytes {
			return nil, fmt.Errorf("desktop entry line %d exceeds limits", line)
		}
		if strings.HasPrefix(text, "[") && strings.HasSuffix(text, "]") {
			group = text[1 : len(text)-1]
			if group == "" || strings.ContainsAny(group, "[]") ||
				strings.ContainsFunc(group, func(c rune) bool { return c < 32 || c == 127 }) || d.Groups[group] != nil {
				return nil, fmt.Errorf("invalid or duplicate desktop group at line %d", line)
			}
			d.Groups[group] = make(map[string]string)
			continue
		}
		key, value, ok := strings.Cut(text, "=")
		key, value = strings.Trim(key, " \t"), strings.Trim(value, " \t")
		if !ok || group == "" || !validKey(key) {
			return nil, fmt.Errorf("invalid desktop entry at line %d", line)
		}
		if _, exists := d.Groups[group][key]; exists {
			return nil, fmt.Errorf("duplicate desktop key at line %d", line)
		}
		d.Groups[group][key] = value
	}
	if d.Groups["Desktop Entry"] == nil {
		return nil, errors.New("missing Desktop Entry group")
	}
	return d, nil
}

func validKey(key string) bool {
	base, locale, localized := strings.Cut(key, "[")
	if base == "" {
		return false
	}
	for _, c := range base {
		if !(c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' ||
			c >= '0' && c <= '9' || c == '-') {
			return false
		}
	}
	if localized {
		if !strings.HasSuffix(locale, "]") || len(locale) < 2 {
			return false
		}
		for _, c := range locale[:len(locale)-1] {
			if !(c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' ||
				c >= '0' && c <= '9' || strings.ContainsRune("_.@-", c)) {
				return false
			}
		}
		return true
	}
	return true
}

// decodeValue handles both ordinary strings and semicolon-separated lists.
// Splitting before unescaping would incorrectly split an escaped semicolon.
func decodeValue(raw string, list bool) ([]string, error) {
	if len(raw) > MaxValueBytes || !utf8.ValidString(raw) {
		return nil, errors.New("invalid desktop value")
	}
	var result []string
	var value strings.Builder
	for i := 0; i < len(raw); i++ {
		c := raw[i]
		if c == '\\' {
			i++
			if i == len(raw) {
				return nil, errors.New("incomplete desktop escape")
			}
			switch raw[i] {
			case 's':
				c = ' '
			case 'n':
				c = '\n'
			case 't':
				c = '\t'
			case 'r':
				c = '\r'
			case '\\':
				c = '\\'
			case ';':
				if !list {
					return nil, errors.New("list escape in desktop string")
				}
				c = ';'
			default:
				return nil, errors.New("unknown desktop escape")
			}
		} else if c == ';' && list {
			result = append(result, value.String())
			value.Reset()
			continue
		} else if c < 32 || c == 127 {
			return nil, errors.New("unescaped control in desktop value")
		}
		value.WriteByte(c)
	}
	if !list || value.Len() != 0 || !strings.HasSuffix(raw, ";") {
		result = append(result, value.String())
	}
	return result, nil
}

func (d *Desktop) String(group, key string) (string, error) {
	values, err := decodeValue(d.Groups[group][key], false)
	if err != nil {
		return "", err
	}
	return values[0], nil
}

func (d *Desktop) List(group, key string) ([]string, error) {
	raw := d.Groups[group][key]
	if raw == "" {
		return nil, nil
	}
	return decodeValue(raw, true)
}

func (d *Desktop) Boolean(group, key string) (bool, error) {
	if _, present := d.Groups[group][key]; !present {
		return false, nil
	}
	switch d.Groups[group][key] {
	case "false":
		return false, nil
	case "true":
		return true, nil
	default:
		return false, fmt.Errorf("invalid desktop boolean %s", key)
	}
}

// LocalePreferences preserves the provider's environment precedence. C/POSIX
// suppress LANGUAGE overrides; expansion into locale fallbacks remains the
// responsibility of Localized/LocaleCandidates.
func LocalePreferences(env map[string]string) []string {
	locale := env["LC_ALL"]
	if locale == "" {
		locale = env["LC_MESSAGES"]
	}
	if locale == "" {
		locale = env["LANG"]
	}
	if locale == "C" || locale == "POSIX" || strings.HasPrefix(locale, "C.") {
		return nil
	}
	var result []string
	if env["LANGUAGE"] != "" {
		result = strings.Split(env["LANGUAGE"], ":")
	}
	if locale != "" {
		result = append(result, locale)
	}
	return result
}

func LocaleCandidates(locale string) []string {
	base, modifier, _ := strings.Cut(locale, "@")
	base, _, _ = strings.Cut(base, ".")
	if base == "" || base == "C" || base == "POSIX" {
		return nil
	}
	language, country, _ := strings.Cut(base, "_")
	result := []string{}
	if modifier != "" && country != "" {
		result = append(result, base+"@"+modifier)
	}
	if country != "" {
		result = append(result, base)
	}
	if modifier != "" {
		result = append(result, language+"@"+modifier)
	}
	return append(result, language)
}

func (d *Desktop) Localized(group, key string, locales []string) (string, error) {
	if _, ok := d.Groups[group][key]; !ok {
		return "", fmt.Errorf("localized desktop key %s has no base value", key)
	}
	for _, locale := range locales {
		for _, candidate := range LocaleCandidates(locale) {
			if raw, ok := d.Groups[group][key+"["+candidate+"]"]; ok {
				values, err := decodeValue(raw, false)
				if err != nil {
					return "", err
				}
				return values[0], nil
			}
		}
	}
	return d.String(group, key)
}

// Visible applies only menu presentation policy. TryExec lookup is provided
// by the caller so this policy neither executes nor searches for applications.
func (d *Desktop) Visible(desktops []string, executable func(string) bool) (bool, error) {
	group := "Desktop Entry"
	for _, key := range []string{"Hidden", "NoDisplay"} {
		value, err := d.Boolean(group, key)
		if err != nil || value {
			return false, err
		}
	}
	entryType, err := d.String(group, "Type")
	if err != nil || entryType != "Application" {
		return false, err
	}
	only, err := d.List(group, "OnlyShowIn")
	if err != nil {
		return false, err
	}
	not, err := d.List(group, "NotShowIn")
	if err != nil {
		return false, err
	}
	if len(only) != 0 && len(not) != 0 {
		return false, errors.New("both OnlyShowIn and NotShowIn are present")
	}
	match := func(values []string) bool {
		for _, value := range values {
			for _, desktop := range desktops {
				if value != "" && value == desktop {
					return true
				}
			}
		}
		return false
	}
	if len(only) != 0 && !match(only) || match(not) {
		return false, nil
	}
	tryExec, err := d.String(group, "TryExec")
	if err != nil {
		return false, err
	}
	if tryExec != "" && (executable == nil || !executable(tryExec)) {
		return false, nil
	}
	return true, nil
}

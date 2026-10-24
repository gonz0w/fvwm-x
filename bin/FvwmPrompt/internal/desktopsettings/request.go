// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import (
	"encoding/hex"
	"errors"
	"fmt"
)

type Snapshot struct {
	Saved, Effective, Overrides Values
	Generation, Instance        string
	PersistenceError            *string
	Submitted                   bool
}

func fields(value Values, names ...string) bool {
	if len(value) != len(names) {
		return false
	}
	for _, key := range names {
		if _, ok := value[key]; !ok {
			return false
		}
	}
	return true
}

func object(value any) (Values, error) {
	v, ok := value.(map[string]any)
	if !ok {
		return nil, errors.New("Expected settings object")
	}
	return v, nil
}

func identity(value any) (string, error) {
	s, ok := value.(string)
	if !ok || len(s) != 32 {
		return "", errors.New("Invalid settings identity")
	}
	if _, e := hex.DecodeString(s); e != nil {
		return "", e
	}
	for _, c := range s {
		if c >= 'A' && c <= 'F' {
			return "", errors.New("Invalid settings identity")
		}
	}
	return s, nil
}

func snapshot(value any) (Snapshot, error) {
	var s Snapshot
	v, err := object(value)
	if err != nil || !fields(v,
		"saved",
		"effective",
		"overrides",
		"generation",
		"instance",
		"persistence_error",
		"submitted") {
		return s, errors.New("Invalid settings snapshot")
	}
	for key, dest := range map[string]*Values{"saved": &s.Saved, "effective": &s.Effective} {
		values, e := object(v[key])
		if e != nil {
			return s, e
		}
		*dest, e = Checked(values)
		if e != nil {
			return s, e
		}
	}
	s.Overrides, err = object(v["overrides"])
	if err != nil {
		return s, err
	}
	for key, value := range s.Overrides {
		if _, e := Value(key, value, false); e != nil {
			return s, e
		}
	}
	s.Generation, err = identity(v["generation"])
	if err != nil {
		return s, err
	}
	s.Instance, err = identity(v["instance"])
	if err != nil {
		return s, err
	}
	if v["persistence_error"] != nil {
		text, ok := v["persistence_error"].(string)
		if !ok || len(text) > 1024 {
			return s, errors.New("Invalid persistence diagnostic")
		}
		s.PersistenceError = &text
	}
	var ok bool
	s.Submitted, ok = v["submitted"].(bool)
	if !ok {
		return s, errors.New("Invalid submission flag")
	}
	return s, nil
}

func merge(base, changes Values) Values {
	result := Values{}
	for k, v := range base {
		result[k] = v
	}
	for k, v := range changes {
		result[k] = v
	}
	return result
}

func Prepare(s Snapshot, changes Values, minute int) (Values, error) {
	for k, v := range changes {
		if _, e := Value(k, v, false); e != nil {
			return nil, e
		}
	}
	candidate, err := Checked(merge(s.Saved, changes))
	if err != nil {
		return nil, err
	}
	effective, err := Checked(merge(candidate, s.Overrides))
	if err != nil {
		return nil, err
	}
	mode, err := Appearance(effective, minute)
	if err != nil {
		return nil, err
	}
	if effective["keep_wallpaper"] == true {
		wallpaper := s.Effective["wallpaper"].(map[string]any)
		candidate["wallpaper"] = Values{"palette": wallpaper["palette"],
			"appearance": wallpaper["appearance"]}
	} else {
		candidate["wallpaper"] = Values{"palette": effective["palette"], "appearance": mode}
	}
	wallpaper := candidate["wallpaper"].(map[string]any)
	effective["wallpaper"] = Values{"palette": wallpaper["palette"],
		"appearance": wallpaper["appearance"]}
	return Values{"candidate": candidate, "effective": effective}, nil
}

func Query(s Snapshot, scope string, observed bool) (Values, error) {
	keys, err := Scope(scope)
	if err != nil {
		return nil, err
	}
	result := Values{}
	for _, key := range keys {
		item, _ := find(key)
		v := Values{"type": item.Type,
			"default":            item.Default,
			"group":              item.Group,
			"description":        item.Description,
			"owner":              "FvwmDesktop",
			"available":          true,
			"capability":         "owned-desktop-preference",
			"preview":            false,
			"native_preview":     key == "launcher_opacity",
			"runtime_constraint": nil,
			"saved":              s.Saved[key],
			"effective":          s.Effective[key],
			"source":             "saved",
			"application":        "not-submitted",
			"generation":         s.Generation,
			"persistence_error":  s.PersistenceError}
		if len(item.Choices) > 0 {
			v["choices"] = append([]string(nil), item.Choices...)
		}
		if item.Minimum != nil {
			v["minimum"], v["maximum"] = item.Minimum, item.Maximum
		}
		if key == "launcher_opacity" && s.Effective["transparency"] == false {
			v["runtime_constraint"] = "transparency-disabled"
		}
		if _, found := s.Overrides[key]; found {
			v["source"] = "module-configuration"
		}
		if observed && s.Submitted {
			v["application"] = "submitted-to-fvwm"
		}
		result[key] = v
	}
	return result, nil
}

// Evaluate is a pure, bounded private operation. No request can name a path,
// execute an action, commit a file or publish a generated presentation plan.
func Evaluate(data []byte) (any, error) {
	decoded, err := DecodeJSON(data)
	if err != nil {
		return nil, err
	}
	r, err := object(decoded)
	if err != nil {
		return nil, err
	}
	operation, ok := r["operation"].(string)
	if !ok {
		return nil, errors.New("Invalid settings operation")
	}
	names := map[string][]string{
		"registry":        {"operation"},
		"appearance-menu": {"operation", "values"},
		"schedule":        {"operation", "values"},
		"preview-form":    {"operation", "instance", "view", "opacity", "saved"},
		"value":           {"operation", "key", "value", "text"},
		"scope":           {"operation", "scope"},
		"minute":          {"operation", "value"},
		"checked":         {"operation", "values"},
		"migrate":         {"operation", "values"},
		"decode":          {"operation", "data"},
		"appearance":      {"operation", "values", "minute"},
		"prepare":         {"operation", "snapshot", "changes", "minute"},
		"query":           {"operation", "snapshot", "scope", "observed"},
		"editor":          {"operation", "snapshot", "scope", "defaults", "draft", "token"},
		"menus":           {"operation", "snapshot", "scope"},
		"editor-values":   {"operation", "keys", "values"},
	}

	expected, ok := names[operation]
	if !ok || !fields(r, expected...) {
		return nil, errors.New("Invalid settings operation fields")
	}
	switch operation {
	case "registry":
		return Values{"registry": Registry(), "keys": mustKeys(), "defaults": Defaults()}, nil
	case "value":
		key, ok := r["key"].(string)
		text, valid := r["text"].(bool)
		if !ok || !valid {
			return nil, errors.New("Invalid setting value request")
		}
		return Value(key, r["value"], text)
	case "scope":
		scope, ok := r["scope"].(string)
		if !ok {
			return nil, errors.New("Invalid settings scope")
		}
		return Scope(scope)
	case "minute":
		value, ok := r["value"].(string)
		if !ok {
			return nil, errors.New("time must be HH:MM")
		}
		return Minute(value)
	case "checked":
		values, e := object(r["values"])
		if e != nil {
			return nil, e
		}
		return Checked(values)
	case "migrate":
		values, _ := r["values"].(map[string]any)
		return Migrate(values), nil
	case "decode":
		data, ok := r["data"].(string)
		if !ok {
			return nil, errors.New("Invalid stored settings")
		}
		return DecodeState([]byte(data))
	case "appearance":
		values, e := object(r["values"])
		if e != nil {
			return nil, e
		}
		values, e = Checked(values)
		if e != nil {
			return nil, e
		}
		minute, ok := integer(r["minute"])
		if !ok {
			return nil, errors.New("Invalid local minute")
		}
		return Appearance(values, minute)
	case "appearance-menu", "schedule":
		values, e := object(r["values"])
		if e != nil {
			return nil, e
		}
		values, e = Checked(values)
		if e != nil {
			return nil, e
		}
		if operation == "schedule" {
			return Schedule(values), nil
		}
		return AppearanceMenu(values), nil
	case "preview-form":
		instance, ok := r["instance"].(string)
		view, valid := r["view"].(string)
		opacity, numeric := integer(r["opacity"])
		saved, boolean := r["saved"].(bool)
		if !ok || !valid || !numeric || !boolean {
			return nil, errors.New("Invalid preview form request")
		}
		return Preview(instance, view, opacity, saved)
	case "editor-values":
		return editorValues(r["keys"], r["values"])
	}
	s, err := snapshot(r["snapshot"])
	if err != nil {
		return nil, err
	}
	switch operation {
	case "prepare":
		changes, e := object(r["changes"])
		if e != nil {
			return nil, e
		}
		minute, ok := integer(r["minute"])
		if !ok {
			return nil, errors.New("Invalid local minute")
		}
		return Prepare(s, changes, minute)
	case "query":
		scope, ok := r["scope"].(string)
		observed, valid := r["observed"].(bool)
		if !ok || !valid {
			return nil, errors.New("Invalid settings query")
		}
		return Query(s, scope, observed)
	case "menus":
		scope, ok := r["scope"].(string)
		if !ok {
			return nil, errors.New("Invalid settings scope")
		}
		return Menus(s, scope)
	case "editor":
		scope, ok := r["scope"].(string)
		defaults, valid := r["defaults"].(bool)
		if !ok || !valid {
			return nil, errors.New("Invalid editor request")
		}
		token, e := identity(r["token"])
		if e != nil {
			return nil, e
		}
		var draft Values
		if r["draft"] != nil {
			draft, e = object(r["draft"])
			if e != nil {
				return nil, e
			}
		}
		return Editor(s, scope, defaults, draft, token)
	}
	return nil, fmt.Errorf("Unsupported settings operation: %s", operation)
}

func mustKeys() []string { keys, _ := Scope("all"); return keys }

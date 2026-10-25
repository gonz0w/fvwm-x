// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import (
	"encoding/hex"
	"errors"
)

type Snapshot struct {
	Saved, Effective, Overrides Values
	Generation, Instance        string
	PersistenceError            *string
	Submitted                   bool
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

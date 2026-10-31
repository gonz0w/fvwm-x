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
	RuntimeHeld                 PreviewValues
	Native                      NativeObservation
	SessionAvailable            bool
}

// NativeObservation is immutable controller-loop state, independent of saved intent.
type NativeObservation struct {
	Readiness, ResourceState, Run, Reason string
	Sequence                              uint64
	XSerial                               uint32
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
	// Carry only valid, unmodified keys from confirmed unsaved runtime state.
	// Apply these before retention resolution, so a boolean keep_wallpaper hold
	// cannot substitute the saved wallpaper during an unrelated edit.
	for key, held := range s.RuntimeHeld {
		if _, explicit := changes[key]; !explicit && held.ValidFor(key) {
			effective[key] = held.Value()
		}
	}
	// Retention belongs to the current effective wallpaper, including one
	// selected by an override, rather than the older saved wallpaper pair.
	if effective["keep_wallpaper"] == true {
		effective["wallpaper"] = s.Effective["wallpaper"]
	}
	wallpaper, err := ResolveWallpaper(effective, minute)
	if err != nil {
		return nil, err
	}
	candidate["wallpaper"] = wallpaper
	effective["wallpaper"] = Values{"palette": wallpaper["palette"],
		"appearance": wallpaper["appearance"]}
	return Values{"candidate": candidate, "effective": effective}, nil
}

func Query(s Snapshot, scope string, observed bool) (Values, error) {
	keys, err := Scope(scope)
	if err != nil {
		return nil, err
	}
	presentation, err := ResolvePresentation(s.Effective)
	if err != nil {
		return nil, err
	}
	result := Values{}
	for _, key := range keys {
		item, _ := find(key)
		// This flag identifies a dedicated native UI consumer. Admission still
		// requires its direction, current policy and native resource capability;
		// it does not advertise a generic settings IPC preview operation.
		v := Values{"type": item.Type,
			"default":            item.Default,
			"group":              item.Group,
			"description":        item.Description,
			"owner":              "FvwmDesktop",
			"available":          true,
			"capability":         "owned-desktop-preference",
			"preview":            false,
			"native_preview":     PreviewKey(key),
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
		if key == "wallpaper_palette" || key == "wallpaper_appearance" {
			if _, err := Checked(s.Effective); err != nil {
				return nil, err
			}
			resolved, resolution := s.Effective[key], "explicit"
			if s.Effective["keep_wallpaper"] == true {
				wallpaper, ok := s.Effective["wallpaper"].(map[string]any)
				if !ok {
					return nil, errors.New("Invalid retained wallpaper")
				}
				resolved = wallpaper[key[len("wallpaper_"):]]
				resolution = "retained"
				v["runtime_constraint"] = "wallpaper-retained"
			} else if resolved == "follow" {
				resolution = "follow"
				if key == "wallpaper_palette" {
					resolved = s.Effective["palette"]
				} else {
					// Auto remains schedule policy here. Query never reads a live
					// clock or claims to observe the currently painted wallpaper.
					resolved = s.Effective["appearance"]
				}
			}
			v["resolved"], v["resolution"] = resolved, resolution
			v["composition_version"] = themeCompositionVersion
		}
		if key == "transparency" || key == "launcher_opacity" {
			v["resolved"], v["resolution"] = presentation.Transparency, "requested"
			v["composition_version"] = themeCompositionVersion
			if key == "launcher_opacity" {
				v["resolved"] = presentation.LauncherOpacity
			}
			if presentation.Constraint == "high-contrast" ||
				key == "launcher_opacity" && presentation.Constraint != "" {
				v["resolution"] = presentation.Constraint
				v["runtime_constraint"] = presentation.Constraint
			}
		}
		if _, found := s.Overrides[key]; found {
			v["source"] = "module-configuration"
		}
		if held, exists := s.RuntimeHeld[key]; exists && held.ValidFor(key) {
			v["source"] = "runtime-kept-preference-unsaved"
			v["persistence_error"] = "Runtime unsaved."
		}
		if observed && s.Submitted {
			v["application"] = "submitted-to-fvwm"
		}
		if NativeKey(key) {
			v["native_mapping"] = true
		}
		if ApplicationKey(key) {
			v["capability"] = "optional-native-application-preference"
			v["application"] = "not-observed"
			if observed {
				v["application"] = "native-" + s.Native.Readiness
				v["readiness"], v["resource_state"] = s.Native.Readiness, s.Native.ResourceState
				v["native_run"], v["native_sequence"] = s.Native.Run, s.Native.Sequence
				v["xserial"], v["failure_reason"] = s.Native.XSerial, s.Native.Reason
			}
		}
		if TrayKey(key) {
			v["capability"] = "optional-tray-preference"
			v["readiness"] = "unobserved"
			v["requested"] = s.Effective[key]
			if observed && s.Submitted {
				v["application"] = "requested-state-submitted"
			}
		}
		if SessionKey(key) {
			v["requested"] = s.Saved[key]
			v["requested_source"] = "saved"
			if override, ok := s.Overrides[key]; ok {
				v["requested"], v["requested_source"] = override, "module-configuration"
			}
			if observed {
				v["source"] = "unavailable"
				if s.SessionAvailable {
					v["source"] = "session-owner"
				}
			}
			v["application"] = "not-submitted"
			if observed {
				v["application"] = "live-unavailable"
			}
			if key == "session_bus" {
				v["application"] = "next-login"
			} else if observed && s.SessionAvailable {
				v["application"] = "live-owner-observed"
			}
			if observed && !s.SessionAvailable {
				v["effective"] = nil
			}
			v["available"] = s.SessionAvailable
			v["capability"] = "optional-session-service-preference"
		}
		result[key] = v
	}
	return result, nil
}

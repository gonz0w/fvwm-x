// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"unicode"
	"unicode/utf8"
)

const MaxStartupMetadataBytes = 255

// StartupMetadata preserves the Desktop Entry tri-state: false overrides class
// fallback, true opts in, and absent uses a valid StartupWMClass. Nil metadata
// keeps historical records byte-identical when serialized with omitempty.
type StartupMetadata struct {
	Name          string `json:"name,omitempty"`
	Icon          string `json:"icon,omitempty"`
	ApplicationID string `json:"application_id,omitempty"`
	WMClass       string `json:"wmclass,omitempty"`
	Notify        *bool  `json:"notify,omitempty"`
}

func startupText(value string) bool {
	if len(value) > MaxStartupMetadataBytes || !utf8.ValidString(value) {
		return false
	}
	for _, c := range value {
		if unicode.IsControl(c) {
			return false
		}
	}
	return true
}

func ValidStartupMetadata(metadata *StartupMetadata) bool {
	if metadata == nil {
		return true
	}
	for _, value := range []string{metadata.Name, metadata.Icon, metadata.ApplicationID, metadata.WMClass} {
		if !startupText(value) {
			return false
		}
	}
	return !StartupEnabled(metadata) || metadata.Name != ""
}

func StartupEnabled(metadata *StartupMetadata) bool {
	if metadata == nil {
		return false
	}
	if metadata.Notify != nil {
		return *metadata.Notify
	}
	return metadata.WMClass != "" && startupText(metadata.WMClass)
}

// StartupMetadataBytes conservatively accounts for JSON escaping and keys.
func StartupMetadataBytes(metadata *StartupMetadata) int {
	if metadata == nil {
		return 0
	}
	return 128 + 6*(len(metadata.Name)+len(metadata.Icon)+len(metadata.ApplicationID)+len(metadata.WMClass))
}

// ParseStartupMetadata reads parent application policy for primary/actions and
// role consumers. Unannounceable optional text preserves an ordinary launch;
// malformed StartupNotify retains the Desktop Entry typed-key error policy.
func ParseStartupMetadata(entry *Desktop, name, icon, id string) (*StartupMetadata, error) {
	metadata := &StartupMetadata{Name: name, Icon: icon, ApplicationID: id}
	if _, present := entry.Groups["Desktop Entry"]["StartupNotify"]; present {
		notify, err := entry.Boolean("Desktop Entry", "StartupNotify")
		if err != nil {
			return nil, err
		}
		metadata.Notify = &notify
	}
	class, err := entry.String("Desktop Entry", "StartupWMClass")
	if err == nil && startupText(class) {
		metadata.WMClass = class
	}
	if metadata.Notify == nil && metadata.WMClass == "" {
		return nil, nil
	}
	if !ValidStartupMetadata(metadata) {
		return nil, nil
	}
	return metadata, nil
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/json"
	"errors"
	"path/filepath"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

const (
	dockingStateVersion  = 1
	dockingStateLimit    = 64 * 1024
	dockingWindowLimit   = 64
	dockingSelectorLimit = 440
	dockingOffsetLimit   = 65535
	dockingDeskLimit     = 1<<31 - 1
)

type dockingProfile struct {
	Name      string          `json:"name"`
	Connector string          `json:"connector"`
	Desk      *int            `json:"desk,omitempty"`
	Panel     bool            `json:"panel,omitempty"`
	Windows   []dockingWindow `json:"windows"`
}

type dockingWindow struct {
	Instance string `json:"instance"`
	Class    string `json:"class"`
	OffsetX  int    `json:"offset_x"`
	OffsetY  int    `json:"offset_y"`
}

func dockingProfilePath(dir, name string) (string, error) {
	if dir == "" || !validDockingName(name) {
		return "", errors.New("Invalid docking profile path")
	}
	return filepath.Join(dir, name+".json"), nil
}

func validDockingName(name string) bool {
	if len(name) == 0 || len(name) > 32 {
		return false
	}
	for i := 0; i < len(name); i++ {
		c := name[i]
		if (c < 'A' || c > 'Z') && (c < 'a' || c > 'z') &&
			(c < '0' || c > '9') && c != '_' && c != '-' {
			return false
		}
	}
	return true
}

func validDockingText(text string) bool {
	if len(text) == 0 || len(text) > 256 || !utf8.ValidString(text) {
		return false
	}
	for i := 0; i < len(text); i++ {
		if text[i] < 32 || text[i] == 127 {
			return false
		}
	}
	return true
}

func (profile dockingProfile) validate() error {
	if !validDockingName(profile.Name) || !validDockingText(profile.Connector) ||
		len(profile.Windows) > dockingWindowLimit ||
		(profile.Desk != nil && (*profile.Desk < 0 || *profile.Desk > dockingDeskLimit)) {
		return errors.New("Invalid docking profile fields")
	}
	seen := make(map[[2]string]struct{}, len(profile.Windows))
	for _, window := range profile.Windows {
		if !validDockingText(window.Instance) || !validDockingText(window.Class) ||
			len(window.Instance)+len(window.Class) > dockingSelectorLimit ||
			window.OffsetX < -dockingOffsetLimit || window.OffsetX > dockingOffsetLimit ||
			window.OffsetY < -dockingOffsetLimit || window.OffsetY > dockingOffsetLimit {
			return errors.New("Invalid docking window fields")
		}
		key := [2]string{window.Instance, window.Class}
		if _, exists := seen[key]; exists {
			return errors.New("Duplicate docking window selector")
		}
		seen[key] = struct{}{}
	}
	return nil
}

func dockingInteger(value any, min, max int) (int, bool) {
	number, ok := value.(json.Number)
	if !ok {
		return 0, false
	}
	result, err := number.Int64()
	if err != nil || result < int64(min) || result > int64(max) {
		return 0, false
	}
	return int(result), true
}

func decodeDockingProfile(data []byte) (dockingProfile, error) {
	decoded, err := desktopsettings.DecodeJSON(data)
	if err != nil {
		return dockingProfile{}, errors.New("Invalid docking profile JSON")
	}
	fields, ok := decoded.(desktopsettings.Values)
	if !ok {
		return dockingProfile{}, errors.New("Docking profile must be an object")
	}
	if _, ok := dockingInteger(fields["schema_version"], dockingStateVersion, dockingStateVersion); !ok {
		return dockingProfile{}, errors.New("Unsupported docking profile schema")
	}
	name, nameOK := fields["name"].(string)
	connector, connectorOK := fields["connector"].(string)
	rows, rowsOK := fields["windows"].([]any)
	if !nameOK || !connectorOK || !rowsOK || len(rows) > dockingWindowLimit {
		return dockingProfile{}, errors.New("Invalid docking profile fields")
	}
	profile := dockingProfile{Name: name, Connector: connector, Windows: make([]dockingWindow, 0, len(rows))}
	expectedFields := 4
	if rawDesk, present := fields["desk"]; present {
		desk, ok := dockingInteger(rawDesk, 0, dockingDeskLimit)
		if !ok {
			return dockingProfile{}, errors.New("Invalid docking workspace")
		}
		profile.Desk = &desk
		expectedFields++
	}
	if rawPanel, present := fields["panel"]; present {
		panel, ok := rawPanel.(bool)
		if !ok {
			return dockingProfile{}, errors.New("Invalid docking panel option")
		}
		profile.Panel = panel
		expectedFields++
	}
	if len(fields) != expectedFields {
		return dockingProfile{}, errors.New("Unknown docking profile fields")
	}
	for _, row := range rows {
		window, ok := row.(desktopsettings.Values)
		if !ok || len(window) != 4 {
			return dockingProfile{}, errors.New("Invalid docking window fields")
		}
		instance, instanceOK := window["instance"].(string)
		class, classOK := window["class"].(string)
		x, xOK := dockingInteger(window["offset_x"], -dockingOffsetLimit, dockingOffsetLimit)
		y, yOK := dockingInteger(window["offset_y"], -dockingOffsetLimit, dockingOffsetLimit)
		if !instanceOK || !classOK || !xOK || !yOK {
			return dockingProfile{}, errors.New("Invalid docking window fields")
		}
		profile.Windows = append(profile.Windows, dockingWindow{
			Instance: instance, Class: class, OffsetX: x, OffsetY: y,
		})
	}
	if err := profile.validate(); err != nil {
		return dockingProfile{}, err
	}
	return profile, nil
}

func loadDockingProfile(dir, name string) (dockingProfile, *string, error) {
	path, err := dockingProfilePath(dir, name)
	if err != nil {
		return dockingProfile{}, nil, err
	}
	data, revision, err := loadPrivateState(path, dockingStateLimit)
	if err != nil || revision == nil {
		return dockingProfile{}, nil, err
	}
	profile, err := decodeDockingProfile(data)
	if err != nil {
		return dockingProfile{}, nil, err
	}
	if profile.Name != name {
		return dockingProfile{}, nil, errors.New("Docking profile name does not match its file")
	}
	return profile, revision, nil
}

func saveDockingProfile(dir string, profile dockingProfile, expected *string) (*string, error) {
	path, err := dockingProfilePath(dir, profile.Name)
	if err != nil {
		return nil, err
	}
	if err := profile.validate(); err != nil {
		return nil, err
	}
	if profile.Windows == nil {
		profile.Windows = []dockingWindow{}
	}
	state := struct {
		SchemaVersion int `json:"schema_version"`
		dockingProfile
	}{dockingStateVersion, profile}
	data, err := json.Marshal(state)
	if err != nil {
		return nil, err
	}
	return savePrivateState(path, ".docking-state.lock", append(data, '\n'), dockingStateLimit, expected)
}

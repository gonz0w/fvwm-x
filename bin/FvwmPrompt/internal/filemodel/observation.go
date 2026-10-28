// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filemodel

import (
	"errors"
	"os"
	"path/filepath"
	"strings"
)

// Observation is a bounded single-target snapshot. It never reads siblings or
// opens the final target. Missing/unavailable references remain displayable.
type Observation struct {
	Path     []byte   `json:"path_bytes"`
	Parent   Identity `json:"parent"`
	Identity Identity `json:"identity"`
	Kind     string   `json:"kind"`
	Status   string   `json:"status"`
}

func ObserveTarget(path []byte) (Observation, error) {
	o := Observation{Path: append([]byte(nil), path...), Status: "unavailable"}
	if !validPath(string(path)) {
		return o, errors.New("invalid absolute file target")
	}
	// Ignore trailing separators only for the metadata lookup so a final
	// symlink cannot become actionable by spelling it with a trailing slash.
	lookup := strings.TrimRight(string(path), string(filepath.Separator))
	if lookup == "" {
		lookup = string(filepath.Separator)
	}
	for _, component := range strings.Split(lookup, string(filepath.Separator)) {
		if component == "." || component == ".." {
			return o, errors.New("file targets cannot contain dot path components")
		}
	}
	parent, err := os.Stat(filepath.Dir(lookup))
	if err != nil || !parent.IsDir() {
		return o, nil
	}
	o.Parent = identity(parent)
	info, err := os.Lstat(lookup)
	if os.IsNotExist(err) {
		o.Status = "missing"
		return o, nil
	}
	if err != nil {
		return o, nil
	}
	o.Identity, o.Kind = identity(info), kind(info.Mode())
	current, err := os.Stat(filepath.Dir(lookup))
	if err != nil || !sameRoot(o.Parent, identity(current)) {
		return o, nil
	}
	o.Status = "present"
	return o, nil
}

// Validate repeats the observation immediately before literal argv dispatch.
// As with Snapshot.Validate, this is not an atomic lease over the application.
func (o Observation) Validate() (string, string, error) {
	current, err := ObserveTarget(o.Path)
	if err != nil || o.Status != "present" || current.Status != "present" ||
		!sameRoot(o.Parent, current.Parent) || o.Identity != current.Identity || o.Kind != current.Kind {
		return "", "", errors.New("target changed or is unavailable; refresh before opening")
	}
	if o.Kind != "file" && o.Kind != "directory" {
		return "", "", errors.New("symlink and special-file actions are unavailable")
	}
	return string(o.Path), o.Kind, nil
}

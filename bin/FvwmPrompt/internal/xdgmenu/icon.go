// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"errors"
	"path"
	"strconv"
	"strings"
	"unicode/utf8"
)

type IconDirectory struct {
	Path                                     string
	Size, Scale, Minimum, Maximum, Threshold int64
	Type                                     string
}
type IconTheme struct {
	Inherits    []string
	Directories []IconDirectory
}

func ValidIconTheme(value string) bool {
	if value == "" || len(value) > MaxValueBytes || !utf8.ValidString(value) || value == "." || value == ".." {
		return false
	}
	for _, c := range value {
		if c < 32 || c == 127 || c == '/' || c == ',' || c == '\\' {
			return false
		}
	}
	return true
}
func iconInteger(values map[string]string, key string, fallback int64) (int64, error) {
	value, present := values[key]
	if !present {
		return fallback, nil
	}
	parsed, err := strconv.ParseInt(value, 10, 64)
	if err != nil || parsed < 0 || parsed > 65535 {
		return 0, errors.New("invalid icon directory size/scale")
	}
	return parsed, nil
}

// ParseIconTheme uses the same bounded INI value policy as desktop entries.
// Theme lists are comma-separated; individual size products use int64 even on
// architectures where the caller's native int would not contain the product.
func ParseIconTheme(data []byte) (*IconTheme, error) {
	if len(data) > MaxSourceBytes-len("[Desktop Entry]\n") {
		return nil, errors.New("icon index exceeds input limit")
	}
	d, err := ParseDesktop(append([]byte("[Desktop Entry]\n"), data...))
	if err != nil {
		return nil, err
	}
	if d.Groups["Icon Theme"] == nil {
		return nil, errors.New("missing Icon Theme group")
	}
	theme := &IconTheme{}
	parents, err := d.String("Icon Theme", "Inherits")
	if err != nil {
		return nil, err
	}
	if parents != "" {
		for _, parent := range strings.Split(parents, ",") {
			parent = strings.TrimSpace(parent)
			if !ValidIconTheme(parent) {
				return nil, errors.New("invalid inherited icon theme")
			}
			theme.Inherits = append(theme.Inherits, parent)
		}
	}
	var names []string
	for _, key := range []string{"Directories", "ScaledDirectories"} {
		value, err := d.String("Icon Theme", key)
		if err != nil {
			return nil, err
		}
		if value != "" {
			names = append(names, strings.Split(value, ",")...)
		}
	}
	if len(names) > MaxItems {
		return nil, errors.New("too many icon directories")
	}
	seen := make(map[string]bool)
	for _, name := range names {
		name = strings.TrimSpace(name)
		if name == "" || !pathValue(name) || path.IsAbs(name) || path.Clean(name) != name || name == ".." || strings.HasPrefix(name, "../") {
			return nil, errors.New("invalid icon subdirectory")
		}
		if seen[name] {
			continue
		}
		seen[name] = true
		values := d.Groups[name]
		if values == nil {
			return nil, errors.New("missing icon subdirectory group")
		}
		directory := IconDirectory{Path: name, Type: values["Type"]}
		if directory.Type == "" {
			directory.Type = "Threshold"
		}
		directory.Size, err = iconInteger(values, "Size", 0)
		if err != nil || directory.Size == 0 {
			return nil, errors.New("missing or invalid icon Size")
		}
		directory.Scale, err = iconInteger(values, "Scale", 1)
		if err != nil || directory.Scale == 0 {
			return nil, errors.New("invalid icon Scale")
		}
		directory.Minimum, err = iconInteger(values, "MinSize", directory.Size)
		if err != nil {
			return nil, err
		}
		directory.Maximum, err = iconInteger(values, "MaxSize", directory.Size)
		if err != nil {
			return nil, err
		}
		directory.Threshold, err = iconInteger(values, "Threshold", 2)
		if err != nil {
			return nil, err
		}
		if directory.Minimum == 0 || directory.Minimum > directory.Maximum || directory.Type != "Fixed" && directory.Type != "Scalable" && directory.Type != "Threshold" {
			return nil, errors.New("invalid icon size range/type")
		}
		theme.Directories = append(theme.Directories, directory)
	}
	return theme, nil
}

func (directory IconDirectory) Distance(size, scale int64) (int64, error) {
	if size < 1 || size > 65535 || scale < 1 || scale > 65535 || directory.Scale < 1 || directory.Scale > 65535 || directory.Size < 1 || directory.Size > 65535 {
		return 0, errors.New("invalid icon lookup geometry")
	}
	low, high := directory.Size, directory.Size
	switch directory.Type {
	case "Fixed":
	case "Scalable":
		low, high = directory.Minimum, directory.Maximum
	case "Threshold":
		low, high = directory.Size-directory.Threshold, directory.Size+directory.Threshold
	default:
		return 0, errors.New("invalid icon directory type")
	}
	if directory.Minimum < 0 || directory.Minimum > 65535 || directory.Maximum < 0 || directory.Maximum > 65535 || directory.Threshold < 0 || directory.Threshold > 65535 || low > high || directory.Type == "Scalable" && directory.Minimum == 0 {
		return 0, errors.New("invalid icon lookup range")
	}
	actual := size * scale
	low *= directory.Scale
	high *= directory.Scale
	if actual < low {
		return low - actual, nil
	}
	if actual > high {
		return actual - high, nil
	}
	return 0, nil
}

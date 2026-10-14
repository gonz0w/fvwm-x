// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"errors"
	"path/filepath"
	"sort"
	"strings"
	"unicode/utf8"
)

// SearchPaths is a preference-ordered snapshot, with user locations first.
// It does not consult the process environment or filesystem.
type SearchPaths struct {
	Config []string
	Data   []string
}

func pathValue(value string) bool {
	return len(value) <= MaxValueBytes && utf8.ValidString(value) &&
		!strings.ContainsRune(value, 0)
}

func SearchDirectories(home string, environment map[string]string) (SearchPaths, error) {
	if !pathValue(home) || !filepath.IsAbs(home) {
		return SearchPaths{}, errors.New("XDG search requires an absolute home directory")
	}
	resolve := func(homeKey, dirsKey, fallback, defaults string) ([]string, error) {
		user, system := environment[homeKey], environment[dirsKey]
		if !pathValue(user) || !pathValue(system) {
			return nil, errors.New("XDG search environment exceeds path limits")
		}
		if !filepath.IsAbs(user) {
			user = filepath.Join(home, fallback)
		}
		if !pathValue(user) {
			return nil, errors.New("XDG user directory exceeds path limits")
		}
		if system == "" {
			system = defaults
		}
		result := []string{filepath.Clean(user)}
		seen := map[string]bool{result[0]: true}
		for remaining := system; remaining != ""; {
			value, tail, _ := strings.Cut(remaining, ":")
			remaining = tail
			// Empty and relative entries never imply the current directory.
			if !filepath.IsAbs(value) {
				continue
			}
			value = filepath.Clean(value)
			if !seen[value] {
				result = append(result, value)
				seen[value] = true
			}
		}
		return result, nil
	}
	config, err := resolve("XDG_CONFIG_HOME", "XDG_CONFIG_DIRS", ".config", "/etc/xdg")
	if err != nil {
		return SearchPaths{}, err
	}
	data, err := resolve("XDG_DATA_HOME", "XDG_DATA_DIRS", ".local/share", "/usr/local/share:/usr/share")
	if err != nil {
		return SearchPaths{}, err
	}
	return SearchPaths{Config: config, Data: data}, nil
}

// MenuDirectory contains file names already collected by the acquisition
// layer. That layer owns regular-file checks, diagnostics and cancellation.
type MenuDirectory struct {
	Path  string
	Names []string
}

type MenuFile struct {
	Path string
	Name string
}

// DiscoverMenus applies XDG precedence to supplied directories in search
// order. It uses no hard-coded system-directory key and never changes input.
func DiscoverMenus(directories []MenuDirectory) ([]MenuFile, error) {
	if len(directories) > MaxItems {
		return nil, errors.New("too many menu directories")
	}
	remaining, bytes, outputBytes := MaxItems, MaxSourceBytes, MaxSourceBytes
	seen := make(map[string]bool)
	var result []MenuFile
	for _, directory := range directories {
		if !pathValue(directory.Path) || !filepath.IsAbs(directory.Path) ||
			len(directory.Names) > remaining || len(directory.Path) > bytes {
			return nil, errors.New("menu inventory exceeds limits or has an invalid directory")
		}
		remaining -= len(directory.Names)
		bytes -= len(directory.Path)
		names := append([]string(nil), directory.Names...)
		for _, name := range names {
			if !pathValue(name) || len(name) > bytes || strings.Contains(name, "/") || name == "." || name == ".." || name == "" {
				return nil, errors.New("invalid or excessive menu inventory name")
			}
			bytes -= len(name)
		}
		sort.Strings(names)
		for _, name := range names {
			if !strings.HasSuffix(name, ".menu") || seen[name] {
				continue
			}
			seen[name] = true
			filename := filepath.Join(directory.Path, name)
			if len(filename) > MaxValueBytes || len(filename)+len(name) > outputBytes {
				return nil, errors.New("menu inventory output exceeds path limits")
			}
			outputBytes -= len(filename) + len(name)
			result = append(result, MenuFile{Path: filename, Name: name})
		}
	}
	return result, nil
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"errors"
	"path"
	"path/filepath"
	"sort"
	"strings"
)

// DesktopFile is relative to an AppDir. Entry remains caller-owned and must
// not be mutated while the pool is in use. Hidden entries stay in the pool:
// filtering first would incorrectly reveal a lower-precedence application.
type DesktopFile struct {
	Relative string
	Entry    *Desktop
}

type AppDirectory struct {
	Path    string
	Entries []DesktopFile
}

type PoolEntry struct {
	Path  string
	Entry *Desktop
}

func DesktopID(relative string) (string, error) {
	if !pathValue(relative) || relative == "" || path.IsAbs(relative) ||
		path.Clean(relative) != relative {
		return "", errors.New("invalid desktop relative path")
	}
	for _, part := range strings.Split(relative, "/") {
		if part == ".." || part == "." || part == "" {
			return "", errors.New("invalid desktop path component")
		}
	}
	if !strings.HasSuffix(relative, ".desktop") {
		return "", errors.New("not a desktop file")
	}
	return strings.ReplaceAll(relative, "/", "-"), nil
}

// BuildPool takes AppDirs in XML order, where the last directory wins.
// Duplicate directories use their last occurrence. Within one directory,
// ambiguous flattened IDs use the first lexically sorted relative name.
func BuildPool(directories []AppDirectory) (map[string]PoolEntry, error) {
	if len(directories) > MaxItems {
		return nil, errors.New("too many application directories")
	}
	last := make(map[string]int)
	remaining, bytes := MaxItems, MaxSourceBytes
	for i, directory := range directories {
		if !pathValue(directory.Path) || !filepath.IsAbs(directory.Path) ||
			len(directory.Entries) > remaining || len(directory.Path) > bytes {
			return nil, errors.New("application inventory exceeds limits or has an invalid directory")
		}
		remaining -= len(directory.Entries)
		bytes -= len(directory.Path)
		for _, entry := range directory.Entries {
			if !pathValue(entry.Relative) || len(entry.Relative) > bytes {
				return nil, errors.New("application inventory exceeds path limits")
			}
			bytes -= len(entry.Relative)
		}
		last[filepath.Clean(directory.Path)] = i
	}
	pool := make(map[string]PoolEntry)
	outputBytes := MaxSourceBytes
	for i, directory := range directories {
		if last[filepath.Clean(directory.Path)] != i {
			continue
		}
		entries := append([]DesktopFile(nil), directory.Entries...)
		sort.SliceStable(entries, func(i, j int) bool { return entries[i].Relative < entries[j].Relative })
		seen := make(map[string]bool)
		for _, entry := range entries {
			id, err := DesktopID(entry.Relative)
			if err != nil || entry.Entry == nil {
				return nil, errors.New("invalid application inventory entry")
			}
			if seen[id] {
				continue
			}
			seen[id] = true
			filename := filepath.Join(directory.Path, entry.Relative)
			if len(filename) > MaxValueBytes || len(filename)+len(id) > outputBytes {
				return nil, errors.New("application pool output exceeds path limits")
			}
			// Charge every insertion, including replacements, so total work is
			// bounded even when a long directory repeatedly supplies one ID.
			outputBytes -= len(filename) + len(id)
			pool[id] = PoolEntry{Path: filename, Entry: entry.Entry}
		}
	}
	return pool, nil
}

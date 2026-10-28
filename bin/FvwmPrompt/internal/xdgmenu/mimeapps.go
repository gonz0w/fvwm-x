// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"bytes"
	"errors"
	"path/filepath"
	"sort"
	"strings"
	"unicode/utf8"
)

const DirectoryRoleMIME = "inode/directory"
const MaxDirectoryRoleRoots = 64
const MaxDirectoryRoleDesktops = 16
const MaxDirectoryRoleSourceFiles = 1152
const MaxDirectoryRoleSourceBytes = 64 * 1024
const MaxDirectoryRoleCandidates = 2048
const MaxDirectoryRoleIDBytes = 4096
const MaxDirectoryRoleReferences = 8192
const MaxDirectoryRolePolicyBytes = 16 * 1024 * 1024

// DirectoryMIMEApps contains only this consumer's known directory MIME role.
// Other types remain unowned; this is not a MIME classifier or database.
type DirectoryMIMEApps struct {
	Defaults, Added, Removed []string
}

type DirectoryMIMEAppsLocation struct {
	Path            string
	DataRank        int // -1 for configuration; otherwise the data-root index.
	DesktopSpecific bool
}

type DirectoryMIMEAppsFile struct {
	DataRank        int
	DesktopSpecific bool
	Values          DirectoryMIMEApps
}

// DirectoryMIMEEntry describes the effective first-precedence desktop file.
// Ineligible and Hidden owners remain here to mask lower same-ID files.
type DirectoryMIMEEntry struct {
	ID                 string
	Rank               int
	Declares, Eligible bool
}

type DirectoryMIMEResolution struct {
	IDs       []string
	DefaultID string
}

func validDirectoryDesktopID(id string) bool {
	value, err := DesktopID(id)
	return err == nil && value == id && len(id) <= MaxDirectoryRoleIDBytes &&
		!strings.ContainsFunc(id, func(c rune) bool { return c < 32 || c == 127 })
}

// ParseDirectoryMIMEApps parses bounded INI syntax separately from Desktop Entry
// keys (which cannot contain a MIME slash). It never rewrites a preference file.
func ParseDirectoryMIMEApps(data []byte) (DirectoryMIMEApps, error) {
	result := DirectoryMIMEApps{}
	if len(data) > MaxDirectoryRoleSourceBytes || !utf8.Valid(data) || bytes.IndexByte(data, 0) >= 0 {
		return result, errors.New("invalid or oversized MIME application settings")
	}
	group := ""
	groups := make(map[string]bool)
	keys := make(map[string]bool)
	for remaining := string(data); remaining != ""; {
		line, rest, _ := strings.Cut(remaining, "\n")
		remaining = rest
		line = strings.Trim(line, " \t\r")
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		if strings.HasPrefix(line, "[") && strings.HasSuffix(line, "]") {
			group = line[1 : len(line)-1]
			if group == "" || strings.ContainsAny(group, "[]") || groups[group] ||
				strings.ContainsFunc(group, func(c rune) bool { return c < 32 || c == 127 }) {
				return DirectoryMIMEApps{}, errors.New("invalid or duplicate MIME settings group")
			}
			groups[group] = true
			continue
		}
		key, raw, found := strings.Cut(line, "=")
		key, raw = strings.TrimSpace(key), strings.Trim(raw, " \t")
		if !found || group == "" || key == "" || strings.ContainsAny(key, "[]") ||
			strings.ContainsFunc(key, func(c rune) bool { return c < 32 || c == 127 }) {
			return DirectoryMIMEApps{}, errors.New("invalid MIME settings assignment")
		}
		if strings.EqualFold(key, DirectoryRoleMIME) {
			key = DirectoryRoleMIME
		}
		identity := group + "\x00" + key
		if keys[identity] {
			return DirectoryMIMEApps{}, errors.New("duplicate MIME settings key")
		}
		keys[identity] = true
		if key != DirectoryRoleMIME || group != "Default Applications" && group != "Added Associations" && group != "Removed Associations" {
			continue
		}
		var ids []string
		if raw != "" {
			values, err := decodeValue(raw, true)
			if err != nil {
				return DirectoryMIMEApps{}, err
			}
			seen := make(map[string]bool)
			for _, id := range values {
				if !validDirectoryDesktopID(id) {
					return DirectoryMIMEApps{}, errors.New("invalid MIME application desktop ID")
				}
				if !seen[id] {
					if len(ids) >= MaxDirectoryRoleCandidates {
						return DirectoryMIMEApps{}, errors.New("too many MIME application IDs")
					}
					ids = append(ids, id)
					seen[id] = true
				}
			}
		}
		switch group {
		case "Default Applications":
			result.Defaults = ids
		case "Added Associations":
			result.Added = ids
		case "Removed Associations":
			result.Removed = ids
		}
	}
	added := make(map[string]bool)
	for _, id := range result.Added {
		added[id] = true
	}
	for _, id := range result.Removed {
		if added[id] {
			return DirectoryMIMEApps{}, errors.New("conflicting added and removed MIME association")
		}
	}
	return result, nil
}

// DirectoryMIMEAppsLocations implements MIME Applications 1.0.1 section 2.
// Desktop-specific files contribute defaults only. Generic data-root records
// are retained even when absent: they also place that root's desktop owners.
func DirectoryMIMEAppsLocations(paths SearchPaths, currentDesktop string) ([]DirectoryMIMEAppsLocation, error) {
	if len(paths.Config)+len(paths.Data) > MaxDirectoryRoleRoots || len(currentDesktop) > MaxValueBytes ||
		!utf8.ValidString(currentDesktop) || strings.ContainsRune(currentDesktop, 0) {
		return nil, errors.New("directory role search exceeds environment limits")
	}
	components := CurrentDesktops(currentDesktop)
	if len(components) > MaxDirectoryRoleDesktops {
		return nil, errors.New("too many current desktop names")
	}
	var desktops []string
	seen := make(map[string]bool)
	for _, component := range components {
		if component == "" {
			continue
		}
		if strings.ContainsRune(component, '/') || strings.ContainsFunc(component, func(c rune) bool { return c < 32 || c == 127 }) {
			return nil, errors.New("invalid current desktop name")
		}
		name := []byte(component)
		for i, c := range name {
			if c >= 'A' && c <= 'Z' {
				name[i] = c + ('a' - 'A')
			}
		}
		lower := string(name)
		if !seen[lower] {
			desktops = append(desktops, lower)
			seen[lower] = true
		}
	}
	var result []DirectoryMIMEAppsLocation
	appendRoot := func(root string, rank int) error {
		if !pathValue(root) || !filepath.IsAbs(root) || filepath.Clean(root) != root {
			return errors.New("invalid directory role search root")
		}
		for _, desktop := range desktops {
			path := filepath.Join(root, desktop+"-mimeapps.list")
			if !pathValue(path) {
				return errors.New("MIME settings path exceeds limits")
			}
			result = append(result, DirectoryMIMEAppsLocation{Path: path, DataRank: rank, DesktopSpecific: true})
		}
		path := filepath.Join(root, "mimeapps.list")
		if !pathValue(path) {
			return errors.New("MIME settings path exceeds limits")
		}
		result = append(result, DirectoryMIMEAppsLocation{Path: path, DataRank: rank})
		return nil
	}
	for _, root := range paths.Config {
		if err := appendRoot(root, -1); err != nil {
			return nil, err
		}
	}
	for rank, root := range paths.Data {
		if err := appendRoot(filepath.Join(root, "applications"), rank); err != nil {
			return nil, err
		}
	}
	if len(result) > MaxDirectoryRoleSourceFiles {
		return nil, errors.New("too many MIME settings sources")
	}
	return result, nil
}

// ResolveDirectoryMIMEApps follows section 3's association ordering independently
// of section 4's defaults. Added/Removed settings cannot change a desktop owner
// from an earlier data root. A Default never creates an association.
func ResolveDirectoryMIMEApps(files []DirectoryMIMEAppsFile, entries []DirectoryMIMEEntry) (DirectoryMIMEResolution, error) {
	if len(files) > MaxDirectoryRoleSourceFiles || len(entries) > MaxDirectoryRoleCandidates {
		return DirectoryMIMEResolution{}, errors.New("directory role inventory exceeds limits")
	}
	owners := make(map[string]DirectoryMIMEEntry)
	byRank := make(map[int][]string)
	references, remaining := MaxDirectoryRoleReferences, MaxDirectoryRolePolicyBytes
	chargeID := func(id string) error {
		if references == 0 || !validDirectoryDesktopID(id) || len(id) > remaining {
			return errors.New("directory role references exceed limits or contain an invalid ID")
		}
		references--
		remaining -= len(id)
		return nil
	}
	for _, entry := range entries {
		if err := chargeID(entry.ID); err != nil {
			return DirectoryMIMEResolution{}, err
		}
		if entry.Rank < 0 || entry.Rank >= MaxDirectoryRoleRoots {
			return DirectoryMIMEResolution{}, errors.New("invalid directory role desktop owner")
		}
		if _, exists := owners[entry.ID]; exists {
			return DirectoryMIMEResolution{}, errors.New("duplicate effective directory role desktop ID")
		}
		owners[entry.ID] = entry
		byRank[entry.Rank] = append(byRank[entry.Rank], entry.ID)
	}
	for rank := range byRank {
		sort.Strings(byRank[rank])
	}
	blacklist, included := make(map[string]bool), make(map[string]bool)
	result := DirectoryMIMEResolution{IDs: []string{}}
	add := func(id string) {
		if owner, ok := owners[id]; ok && owner.Eligible && !blacklist[id] && !included[id] {
			result.IDs = append(result.IDs, id)
			included[id] = true
		}
	}
	for _, file := range files {
		if file.DataRank < -1 || file.DataRank >= MaxDirectoryRoleRoots {
			return DirectoryMIMEResolution{}, errors.New("invalid MIME settings directory rank")
		}
		for _, values := range [][]string{file.Values.Defaults, file.Values.Added, file.Values.Removed} {
			if len(values) > MaxDirectoryRoleCandidates {
				return DirectoryMIMEResolution{}, errors.New("too many MIME settings IDs")
			}
			for _, id := range values {
				if err := chargeID(id); err != nil {
					return DirectoryMIMEResolution{}, err
				}
			}
		}
		if file.DesktopSpecific {
			continue
		}
		for _, id := range file.Values.Added {
			if owner, ok := owners[id]; ok && owner.Rank >= file.DataRank {
				add(id)
			}
		}
		for _, id := range file.Values.Removed {
			if owner, ok := owners[id]; ok && owner.Rank >= file.DataRank {
				blacklist[id] = true
			}
		}
		if file.DataRank >= 0 {
			for _, id := range byRank[file.DataRank] {
				if owners[id].Declares {
					add(id)
				}
			}
			for _, id := range byRank[file.DataRank] {
				blacklist[id] = true
			}
		}
	}
	for _, file := range files {
		for _, id := range file.Values.Defaults {
			if included[id] {
				result.DefaultID = id
				return result, nil
			}
		}
	}
	if len(result.IDs) != 0 {
		result.DefaultID = result.IDs[0]
	}
	return result, nil
}

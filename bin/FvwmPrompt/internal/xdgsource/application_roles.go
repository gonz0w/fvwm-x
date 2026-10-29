// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"errors"
	"fmt"
	"io/fs"
	"path/filepath"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

// ApplicationRoleResult describes a known canonical MIME type and its bounded
// most-specific-first fallback chain. Discovery never changes MIME preferences.
type ApplicationRoleResult struct {
	Handlers  []DirectoryHandler
	DefaultID string
	MIME      string
	Types     []string
}

type applicationRoleSettings struct {
	location xdgmenu.DirectoryMIMEAppsLocation
	values   map[string]xdgmenu.DirectoryMIMEApps
}

func (s *Source) mimeTypeGraph(paths xdgmenu.SearchPaths) (xdgmenu.MIMETypeGraph, error) {
	sources := make([]xdgmenu.MIMETypeSource, 0, len(paths.Data))
	for _, root := range paths.Data {
		var source xdgmenu.MIMETypeSource
		for _, input := range []struct {
			name string
			data *[]byte
		}{{"aliases", &source.Aliases}, {"subclasses", &source.Subclasses}} {
			path := filepath.Join(root, "mime", input.name)
			data, err := s.readLimited(path, xdgmenu.MaxMIMETypeSourceBytes)
			if errors.Is(err, fs.ErrNotExist) {
				continue
			}
			if err != nil {
				return xdgmenu.MIMETypeGraph{}, fmt.Errorf("MIME metadata %s: %w", path, err)
			}
			*input.data = data
		}
		sources = append(sources, source)
	}
	return xdgmenu.NewMIMETypeGraph(sources)
}

// ApplicationRoles consumes the same bounded XDG settings and single desktop
// inventory as DirectoryRoles, with installed aliases/subclasses added to the
// same 16 MiB acquisition budget. Use a fresh Source for launch revalidation.
func (s *Source) ApplicationRoles(mime string, options DirectoryRoleOptions) (ApplicationRoleResult, error) {
	empty := ApplicationRoleResult{Handlers: []DirectoryHandler{}}
	if _, err := xdgmenu.NormalizeMIMEType(mime); err != nil {
		return empty, err
	}
	if options.Executable == nil || !roleText(options.Terminal, xdgmenu.MaxValueBytes) {
		return empty, errors.New("application role lookup requires valid executable policy")
	}
	paths, err := xdgmenu.SearchDirectories(options.Home, options.Environment)
	if err != nil {
		return empty, err
	}
	locations, err := xdgmenu.DirectoryMIMEAppsLocations(paths, options.Environment["XDG_CURRENT_DESKTOP"])
	if err != nil {
		return empty, err
	}
	s.bytes, s.items = min(s.bytes, MaxDirectoryRoleBytes), min(s.items, MaxDirectoryRoleItems)
	graph, err := s.mimeTypeGraph(paths)
	if err != nil {
		return empty, err
	}
	types, err := graph.Types(mime)
	if err != nil {
		return empty, err
	}
	settings := make([]applicationRoleSettings, 0, len(locations))
	references := xdgmenu.MaxDirectoryRoleReferences
	for _, location := range locations {
		data, err := s.readLimited(location.Path, MaxDirectoryRoleSourceBytes)
		if errors.Is(err, fs.ErrNotExist) {
			data, err = nil, nil
		}
		if err != nil {
			return empty, err
		}
		values, err := xdgmenu.ParseApplicationMIMEApps(data, graph)
		if err != nil {
			return empty, fmt.Errorf("MIME settings %s: %w", location.Path, err)
		}
		// Charge the retained settings once across the complete type chain,
		// including unrelated keys. Per-type resolution grants no new budget.
		for _, value := range values {
			count := len(value.Defaults) + len(value.Added) + len(value.Removed)
			if count > references {
				return empty, errors.New("application role association references exceed limits")
			}
			references -= count
		}
		settings = append(settings, applicationRoleSettings{location: location, values: values})
	}
	owners, handlers, err := s.roleInventory(paths, options, &graph, strings.Join(types, "\x00"), &references)
	if err != nil {
		return empty, err
	}
	result := ApplicationRoleResult{Handlers: []DirectoryHandler{}, MIME: types[0], Types: types}
	included := make(map[string]bool)
	remaining := MaxDirectoryRoleResultBytes - 4096 - 6*len(result.MIME)
	for _, mime := range types {
		remaining -= 6*len(mime) + 4
	}
	for _, mime := range types {
		files := make([]xdgmenu.DirectoryMIMEAppsFile, 0, len(settings))
		for _, file := range settings {
			files = append(files, xdgmenu.DirectoryMIMEAppsFile{DataRank: file.location.DataRank,
				DesktopSpecific: file.location.DesktopSpecific, Values: file.values[mime]})
		}
		entries := make([]xdgmenu.DirectoryMIMEEntry, 0, len(owners))
		for _, owner := range owners {
			declares := false
			for _, value := range owner.Types {
				declares = declares || value == mime
			}
			entries = append(entries, xdgmenu.DirectoryMIMEEntry{ID: owner.ID, Rank: owner.Rank,
				Declares: declares, Eligible: owner.Eligible})
		}
		resolution, err := xdgmenu.ResolveDirectoryMIMEApps(files, entries)
		if err != nil {
			return empty, err
		}
		// MIME Applications section 4 runs completely for each type: an
		// exact association fallback precedes even an explicit parent default.
		if result.DefaultID == "" {
			result.DefaultID = resolution.DefaultID
		}
		for _, id := range resolution.IDs {
			if included[id] {
				continue
			}
			if len(result.Handlers) >= MaxDirectoryRoleResults {
				return empty, errors.New("too many application role results")
			}
			handler := handlers[id]
			size := 256 + xdgmenu.StartupMetadataBytes(handler.Startup)
			for _, value := range []string{handler.ID, handler.Name, handler.Icon, handler.DesktopPath, handler.Exec, handler.WorkingDir, handler.Fingerprint} {
				size += 6*len(value) + 4
			}
			if size > remaining {
				return empty, errors.New("application role result exceeds byte limit")
			}
			remaining -= size
			included[id] = true
			result.Handlers = append(result.Handlers, handler)
		}
	}
	return result, nil
}

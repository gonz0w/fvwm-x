// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"errors"
	"io/fs"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

func legacyDesktop(d *xdgmenu.Desktop) (*xdgmenu.Desktop, error) {
	copy := &xdgmenu.Desktop{Groups: make(map[string]map[string]string, len(d.Groups))}
	for group, values := range d.Groups {
		copy.Groups[group] = values
	}
	values := make(map[string]string, len(d.Groups["Desktop Entry"])+1)
	for key, value := range d.Groups["Desktop Entry"] {
		values[key] = value
	}
	copy.Groups["Desktop Entry"] = values
	categories, err := d.List("Desktop Entry", "Categories")
	if err != nil {
		return nil, err
	}
	for _, category := range categories {
		if category == "Legacy" {
			return copy, nil
		}
	}
	var encoded strings.Builder
	escape := strings.NewReplacer(`\`, `\\`, ";", `\;`, " ", `\s`,
		"\n", `\n`, "\r", `\r`, "\t", `\t`)
	for _, category := range categories {
		encoded.WriteString(escape.Replace(category))
		encoded.WriteByte(';')
	}
	encoded.WriteString("Legacy;")
	if encoded.Len() > xdgmenu.MaxValueBytes {
		return nil, errors.New("legacy categories exceed value limit")
	}
	values["Categories"] = encoded.String()
	return copy, nil
}

func (s *Source) locationPool(location xdgmenu.AppLocation) (map[string]xdgmenu.PoolEntry, error) {
	if !location.Legacy {
		return s.pool(location.Path)
	}
	if pool, ok := s.pools[location]; ok {
		return pool, nil
	}
	pool := make(map[string]xdgmenu.PoolEntry)
	var filenames []string
	err := s.walk(location.Path, location.Path, 0, make(map[string]bool), func(path string) error { filenames = append(filenames, path); return nil })
	if errors.Is(err, fs.ErrNotExist) {
		err = nil
	}
	if err != nil {
		return nil, err
	}
	sort.Strings(filenames)
	for _, path := range filenames {
		id := location.Prefix + filepath.Base(path)
		if len(id) > xdgmenu.MaxValueBytes {
			return nil, errors.New("legacy desktop identity exceeds limit")
		}
		if _, ok := pool[id]; ok {
			continue
		}
		d, err := s.desktop(path)
		if errors.Is(err, fs.ErrNotExist) {
			continue
		}
		if err != nil {
			return nil, err
		}
		d, err = legacyDesktop(d)
		if err != nil {
			return nil, err
		}
		if err = s.charge(1, len(id)); err != nil {
			return nil, err
		}
		pool[id] = xdgmenu.PoolEntry{Path: path, Entry: d}
	}
	s.pools[location] = pool
	return pool, nil
}

// LegacyMenu synthesizes the specification's hierarchy, leaving all parsed
// desktop records immutable. Explicit categories suppress automatic Filename
// inclusion; the legacy pool still adds the Legacy category for normal rules.
func (s *Source) LegacyMenu(directory, prefix string) (*xdgmenu.Node, error) {
	if !validPath(directory) || len(prefix) > xdgmenu.MaxValueBytes || strings.ContainsAny(prefix, "/\x00") {
		return nil, errors.New("invalid legacy hierarchy")
	}
	return s.legacyMenu(directory, prefix, 0, make(map[string]bool))
}

func (s *Source) legacyMenu(directory, prefix string, depth int, active map[string]bool) (*xdgmenu.Node, error) {
	if depth >= xdgmenu.MaxDepth {
		return nil, errors.New("legacy hierarchy exceeds depth")
	}
	if err := s.ctx.Err(); err != nil {
		return nil, err
	}
	canonical, err := filepath.EvalSymlinks(directory)
	if err != nil {
		return nil, err
	}
	if active[canonical] {
		return nil, nil
	}
	active[canonical] = true
	defer delete(active, canonical)
	entries, err := s.entries(directory)
	if err != nil {
		return nil, err
	}
	name := filepath.Base(directory)
	if name == string(filepath.Separator) {
		name = "Applications"
	}
	root := &xdgmenu.Node{Tag: "Menu", Children: []*xdgmenu.Node{
		{Tag: "Name", Text: name},
		{Tag: "AppDir", Text: directory, Attr: map[string]string{"fvwm-legacy": "true", "fvwm-legacy-prefix": prefix}},
		{Tag: "DirectoryDir", Text: directory},
	}}
	include := &xdgmenu.Node{Tag: "Include"}
	for _, entry := range entries {
		path := filepath.Join(directory, entry.Name())
		if !validPath(path) {
			return nil, errors.New("legacy path exceeds limit")
		}
		info, err := os.Stat(path)
		if errors.Is(err, fs.ErrNotExist) {
			continue
		}
		if err != nil {
			return nil, err
		}
		if info.IsDir() {
			child, err := s.legacyMenu(path, prefix, depth+1, active)
			if err != nil {
				return nil, err
			}
			if child != nil {
				root.Children = append(root.Children, child)
			}
		} else if entry.Name() == ".directory" {
			root.Children = append(root.Children, &xdgmenu.Node{Tag: "Directory", Text: ".directory"})
		} else if strings.HasSuffix(entry.Name(), ".desktop") {
			d, err := s.desktop(path)
			if errors.Is(err, fs.ErrNotExist) {
				continue
			}
			if err != nil {
				return nil, err
			}
			if _, present := d.Groups["Desktop Entry"]["Categories"]; !present {
				id := prefix + entry.Name()
				if len(id) > xdgmenu.MaxValueBytes {
					return nil, errors.New("legacy desktop identity exceeds limit")
				}
				include.Children = append(include.Children, &xdgmenu.Node{Tag: "Filename", Text: id})
			}
		}
	}
	if err = s.charge(3+len(root.Children)+len(include.Children), len(name)+len(prefix)); err != nil {
		return nil, err
	}
	root.Children = append(root.Children, include)
	return root, nil
}

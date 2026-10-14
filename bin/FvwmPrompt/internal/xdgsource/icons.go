// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"errors"
	"io/fs"
	"math"
	"os"
	"path/filepath"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

type IconFinder struct {
	Source     *Source
	Roots      []string
	Theme      string
	Size       int
	Extensions []string
	themes     map[string]*xdgmenu.IconTheme
	remaining  int
}

func NewIconFinder(source *Source, home string, paths xdgmenu.SearchPaths, theme string, size int) *IconFinder {
	roots := []string{filepath.Join(home, ".icons")}
	for _, path := range paths.Data {
		roots = append(roots, filepath.Join(path, "icons"))
	}
	roots = append(roots, "/usr/share/pixmaps")
	return &IconFinder{Source: source, Roots: roots, Theme: theme, Size: size, Extensions: []string{"png", "svg", "xpm"}, themes: make(map[string]*xdgmenu.IconTheme), remaining: xdgmenu.MaxAllocationWork}
}
func (f *IconFinder) charge() error {
	if f.Source == nil {
		return errors.New("missing icon acquisition owner")
	}
	if err := f.Source.ctx.Err(); err != nil {
		return err
	}
	f.remaining--
	if f.remaining < 0 {
		return errors.New("icon lookup exceeds job work limit")
	}
	return nil
}
func (f *IconFinder) exists(path string) (bool, error) {
	if err := f.charge(); err != nil {
		return false, err
	}
	if !validPath(path) {
		return false, errors.New("invalid icon path")
	}
	info, err := os.Stat(path)
	if errors.Is(err, fs.ErrNotExist) {
		return false, nil
	}
	if err != nil {
		return false, err
	}
	return info.Mode().IsRegular() && info.Size() <= MaxJobBytes, nil
}
func (f *IconFinder) theme(name string) (*xdgmenu.IconTheme, error) {
	if err := f.charge(); err != nil {
		return nil, err
	}
	if !xdgmenu.ValidIconTheme(name) {
		return nil, errors.New("invalid icon theme")
	}
	if theme, ok := f.themes[name]; ok {
		return theme, nil
	}
	for _, root := range f.Roots {
		data, err := f.Source.ReadMenu(filepath.Join(root, name, "index.theme"))
		if errors.Is(err, fs.ErrNotExist) {
			continue
		}
		if err != nil {
			return nil, err
		}
		theme, err := xdgmenu.ParseIconTheme(data)
		if err != nil {
			return nil, err
		}
		f.themes[name] = theme
		return theme, nil
	}
	f.themes[name] = nil
	return nil, nil
}
func (f *IconFinder) lookup(name, themeName string, visited map[string]bool, depth int) (string, error) {
	if depth >= xdgmenu.MaxDepth {
		return "", errors.New("icon inheritance exceeds depth")
	}
	if visited[themeName] {
		return "", nil
	}
	visited[themeName] = true
	theme, err := f.theme(themeName)
	if err != nil || theme == nil {
		return "", err
	}
	best := ""
	distance := int64(math.MaxInt64)
	for _, directory := range theme.Directories {
		delta, err := directory.Distance(int64(f.Size), 1)
		if err != nil {
			return "", err
		}
		for _, root := range f.Roots {
			for _, extension := range f.Extensions {
				filename := filepath.Join(root, themeName, directory.Path, name+"."+extension)
				found, err := f.exists(filename)
				if err != nil {
					return "", err
				}
				if !found {
					continue
				}
				if delta == 0 && directory.Scale == 1 {
					return filename, nil
				}
				if delta < distance {
					best, distance = filename, delta
				}
			}
		}
	}
	if best != "" {
		return best, nil
	}
	for _, parent := range theme.Inherits {
		found, err := f.lookup(name, parent, visited, depth+1)
		if err != nil || found != "" {
			return found, err
		}
	}
	return "", nil
}

func (f *IconFinder) Find(name string) (string, error) {
	if name == "" {
		return "", nil
	}
	if f.Size < 1 || f.Size > 65535 {
		return "", nil
	}
	if filepath.IsAbs(name) {
		supported := false
		for _, extension := range f.Extensions {
			if filepath.Ext(name) == "."+extension {
				supported = true
				break
			}
		}
		if !supported {
			return "", nil
		}
		found, err := f.exists(name)
		if err != nil || !found {
			return "", err
		}
		return name, nil
	}
	if strings.ContainsAny(name, "/\\\x00") || len(name) > xdgmenu.MaxValueBytes {
		return "", errors.New("invalid icon name")
	}
	for _, extension := range f.Extensions {
		if strings.HasSuffix(name, "."+extension) {
			name = strings.TrimSuffix(name, "."+extension)
			break
		}
	}
	visited := make(map[string]bool)
	if f.Theme != "" {
		found, err := f.lookup(name, f.Theme, visited, 0)
		if err != nil || found != "" {
			return found, err
		}
	}
	if !visited["hicolor"] {
		found, err := f.lookup(name, "hicolor", visited, 0)
		if err != nil || found != "" {
			return found, err
		}
	}
	for _, root := range f.Roots {
		for _, extension := range f.Extensions {
			filename := filepath.Join(root, name+"."+extension)
			found, err := f.exists(filename)
			if err != nil {
				return "", err
			}
			if found {
				return filename, nil
			}
		}
	}
	return "", nil
}

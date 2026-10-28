// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

const MaxDirectoryRoleSourceBytes = xdgmenu.MaxDirectoryRoleSourceBytes
const MaxDirectoryRoleBytes = xdgmenu.MaxDirectoryRolePolicyBytes
const MaxDirectoryRoleItems = 8192
const MaxDirectoryRoleDepth = 16
const MaxDirectoryRoleResults = 128
const MaxDirectoryRoleResultBytes = 1024 * 1024

type DirectoryRoleOptions struct {
	Home        string
	Environment map[string]string
	Terminal    string
	Executable  func(string) bool
}

// DirectoryHandler keeps unexpanded launch metadata inside the worker. Folder
// bytes are substituted only after fresh role/entry and root revalidation.
type DirectoryHandler struct {
	ID, Name, Icon, DesktopPath, Exec, WorkingDir, Fingerprint string
	Terminal                                                   bool
}

type DirectoryRoleResult struct {
	Handlers  []DirectoryHandler
	DefaultID string
}

func roleText(value string, limit int) bool {
	return len(value) <= limit && utf8.ValidString(value) && !strings.ContainsRune(value, 0)
}

func directoryHandler(id, path string, data []byte, entry *xdgmenu.Desktop, options DirectoryRoleOptions) (DirectoryHandler, bool, bool, error) {
	var handler DirectoryHandler
	hidden, err := entry.Boolean("Desktop Entry", "Hidden")
	if err != nil {
		return handler, false, false, err
	}
	if hidden {
		return handler, false, false, nil
	}
	typeName, err := entry.String("Desktop Entry", "Type")
	if err != nil {
		return handler, false, false, err
	}
	if typeName != "Application" {
		return handler, false, false, nil
	}
	types, err := entry.List("Desktop Entry", "MimeType")
	if err != nil {
		return handler, false, false, err
	}
	declares := false
	for _, value := range types {
		declares = declares || strings.EqualFold(value, xdgmenu.DirectoryRoleMIME)
	}
	locales := xdgmenu.LocalePreferences(options.Environment)
	name, err := entry.Localized("Desktop Entry", "Name", locales)
	if err != nil {
		return handler, declares, false, err
	}
	icon, err := xdgmenu.LocalizedIcon(entry, "Desktop Entry", locales)
	if err != nil {
		return handler, declares, false, err
	}
	terminal, err := entry.Boolean("Desktop Entry", "Terminal")
	if err != nil {
		return handler, declares, false, err
	}
	if _, err := entry.Boolean("Desktop Entry", "DBusActivatable"); err != nil {
		return handler, declares, false, err
	}
	directory, err := entry.String("Desktop Entry", "Path")
	if err != nil {
		return handler, declares, false, err
	}
	tryExec, err := entry.String("Desktop Entry", "TryExec")
	if err != nil {
		return handler, declares, false, err
	}
	rawExec := entry.Groups["Desktop Entry"]["Exec"]
	for _, value := range []string{name, icon, path, directory, tryExec, rawExec} {
		if !roleText(value, xdgmenu.MaxValueBytes) {
			return handler, declares, false, errors.New("directory role metadata exceeds text limits")
		}
	}
	if name == "" || rawExec == "" || tryExec != "" && !options.Executable(tryExec) || terminal && (options.Terminal == "" || !options.Executable(options.Terminal)) {
		return handler, declares, false, nil
	}
	if directory != "" {
		if !filepath.IsAbs(directory) {
			return handler, declares, false, errors.New("directory role working directory must be absolute")
		}
		info, err := os.Stat(directory)
		if errors.Is(err, fs.ErrNotExist) {
			return handler, declares, false, nil
		}
		if err != nil {
			return handler, declares, false, err
		}
		if !info.IsDir() {
			return handler, declares, false, nil
		}
	}
	// Validate the grammar and executable without serializing any expanded
	// caller path. Both operand kinds are present only as fixed validation data.
	argv, err := xdgmenu.ExpandExec(rawExec, xdgmenu.ExecContext{Name: name, Icon: icon, Location: path, Files: []string{"/"}, URLs: []string{"file:///"}})
	if err != nil {
		return handler, declares, false, fmt.Errorf("invalid directory role Exec: %w", err)
	}
	if !options.Executable(argv[0]) {
		return handler, declares, false, nil
	}
	digest := sha256.New()
	for _, value := range []string{id, path, name, icon, directory, rawExec} {
		digest.Write([]byte(value))
		digest.Write([]byte{0})
	}
	digest.Write(data)
	handler = DirectoryHandler{ID: id, Name: name, Icon: icon, DesktopPath: path, Exec: rawExec, WorkingDir: directory,
		Fingerprint: hex.EncodeToString(digest.Sum(nil)), Terminal: terminal}
	return handler, declares, true, nil
}

// DirectoryRoles consumes one bounded Source job. Use a fresh New(ctx) for a
// later query/launch revalidation: cached input is intentionally immutable here.
func (s *Source) DirectoryRoles(options DirectoryRoleOptions) (DirectoryRoleResult, error) {
	result := DirectoryRoleResult{Handlers: []DirectoryHandler{}}
	if options.Executable == nil || !roleText(options.Terminal, xdgmenu.MaxValueBytes) {
		return result, errors.New("directory role lookup requires valid executable policy")
	}
	paths, err := xdgmenu.SearchDirectories(options.Home, options.Environment)
	if err != nil {
		return result, err
	}
	locations, err := xdgmenu.DirectoryMIMEAppsLocations(paths, options.Environment["XDG_CURRENT_DESKTOP"])
	if err != nil {
		return result, err
	}
	s.bytes, s.items = min(s.bytes, MaxDirectoryRoleBytes), min(s.items, MaxDirectoryRoleItems)
	files := make([]xdgmenu.DirectoryMIMEAppsFile, 0, len(locations))
	references := xdgmenu.MaxDirectoryRoleReferences
	for _, location := range locations {
		data, err := s.readLimited(location.Path, MaxDirectoryRoleSourceBytes)
		if errors.Is(err, fs.ErrNotExist) {
			data, err = nil, nil
		}
		if err != nil {
			return result, err
		}
		values, err := xdgmenu.ParseDirectoryMIMEApps(data)
		if err != nil {
			return result, fmt.Errorf("MIME settings %s: %w", location.Path, err)
		}
		count := len(values.Defaults) + len(values.Added) + len(values.Removed)
		if count > references {
			return result, errors.New("directory role association references exceed limits")
		}
		references -= count
		files = append(files, xdgmenu.DirectoryMIMEAppsFile{DataRank: location.DataRank, DesktopSpecific: location.DesktopSpecific, Values: values})
	}
	owners := []xdgmenu.DirectoryMIMEEntry{}
	handlers, seen := make(map[string]DirectoryHandler), make(map[string]bool)
	candidates := 0
	for rank, root := range paths.Data {
		root = filepath.Join(root, "applications")
		// Only an absent root is an empty inventory. Once walking starts,
		// a disappearing nested directory/read failure must not turn into a
		// successful partial inventory by swallowing its ErrNotExist.
		info, err := os.Stat(root)
		if errors.Is(err, fs.ErrNotExist) {
			continue
		}
		if err != nil {
			return result, err
		}
		if !info.IsDir() {
			return result, errors.New("directory role application root is not a directory")
		}
		readOwner := func(path string) error {
			relative, err := filepath.Rel(root, path)
			if err != nil {
				return err
			}
			id, err := xdgmenu.DesktopID(filepath.ToSlash(relative))
			if err != nil || !roleText(id, xdgmenu.MaxDirectoryRoleIDBytes) || strings.ContainsFunc(id, func(c rune) bool { return c < 32 || c == 127 }) {
				return errors.New("invalid directory role desktop ID")
			}
			if seen[id] {
				return nil
			}
			if references == 0 {
				return errors.New("directory role desktop references exceed limits")
			}
			references--
			seen[id] = true
			data, err := s.readLimited(path, MaxDirectoryRoleSourceBytes)
			if err != nil {
				return err
			}
			entry, err := xdgmenu.ParseDesktop(data)
			if err != nil {
				return fmt.Errorf("directory application %s: %w", path, err)
			}
			handler, declares, eligible, err := directoryHandler(id, path, data, entry, options)
			if err != nil {
				return fmt.Errorf("directory application %s: %w", path, err)
			}
			owners = append(owners, xdgmenu.DirectoryMIMEEntry{ID: id, Rank: rank, Declares: declares, Eligible: eligible})
			if eligible {
				handlers[id] = handler
			}
			return nil
		}
		var filenames []string
		err = s.walkLimited(root, root, 0, MaxDirectoryRoleDepth, make(map[string]bool), func(path string) error {
			candidates++
			if candidates > xdgmenu.MaxDirectoryRoleCandidates {
				return errors.New("too many directory role desktop candidates")
			}
			if err := s.charge(0, len(path)); err != nil {
				return err
			}
			filenames = append(filenames, path)
			return nil
		})
		if err != nil {
			return result, err
		}
		// Match the existing menu pool's lexical ownership for ambiguous
		// flattened IDs inside one root, independently of traversal order.
		sort.Strings(filenames)
		for _, path := range filenames {
			if err := readOwner(path); err != nil {
				return result, err
			}
		}
	}
	resolution, err := xdgmenu.ResolveDirectoryMIMEApps(files, owners)
	if err != nil {
		return result, err
	}
	if len(resolution.IDs) > MaxDirectoryRoleResults {
		return result, errors.New("too many directory application results")
	}
	remaining := MaxDirectoryRoleResultBytes - 4096
	for _, id := range resolution.IDs {
		handler := handlers[id]
		size := 256
		for _, value := range []string{handler.ID, handler.Name, handler.Icon, handler.DesktopPath, handler.Exec, handler.WorkingDir, handler.Fingerprint} {
			size += 6*len(value) + 4
		}
		if size > remaining {
			return DirectoryRoleResult{}, errors.New("directory application result exceeds byte limit")
		}
		remaining -= size
		result.Handlers = append(result.Handlers, handler)
	}
	result.DefaultID = resolution.DefaultID
	return result, nil
}

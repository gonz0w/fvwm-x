// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"context"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"unicode"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"golang.org/x/sys/unix"
)

var errAutostartLimit = errors.New("autostart scan exceeds input limits")

type autostartSource struct {
	ctx       context.Context
	remaining int
}

func (s *autostartSource) charge(bytes int) error {
	if err := s.ctx.Err(); err != nil {
		return err
	}
	if bytes < 0 || bytes > s.remaining {
		return errAutostartLimit
	}
	s.remaining -= bytes
	return nil
}

// filenames counts every observed directory entry, even irrelevant files.
// A desktop filename reserves precedence before its type or contents are read.
func (s *autostartSource) filenames(path string) ([]string, error) {
	if !validPath(path) {
		return nil, errors.New("invalid autostart directory path")
	}
	if err := s.charge(len(path)); err != nil {
		return nil, err
	}
	file, err := openFile(path, true)
	if errors.Is(err, fs.ErrNotExist) {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	defer file.Close()
	var names []string
	observed := 0
	for {
		if err := s.ctx.Err(); err != nil {
			return nil, err
		}
		// One extra observation detects overflow without an unbounded ReadDir.
		batch, readErr := file.ReadDir(min(128, xdgmenu.MaxAutostartDirectoryEntries-observed+1))
		if len(batch) > xdgmenu.MaxAutostartDirectoryEntries-observed {
			return nil, errAutostartLimit
		}
		observed += len(batch)
		for _, entry := range batch {
			name := entry.Name()
			if err := s.charge(len(name)); err != nil {
				return nil, err
			}
			if !strings.HasSuffix(name, ".desktop") {
				continue
			}
			if name == ".desktop" || !utf8.ValidString(name) || strings.ContainsRune(name, 0) || len(name) > xdgmenu.MaxValueBytes {
				return nil, errors.New("invalid autostart desktop filename")
			}
			names = append(names, name)
		}
		if readErr == io.EOF {
			break
		}
		if readErr != nil {
			return nil, readErr
		}
	}
	return names, nil
}

func (s *autostartSource) read(path string) ([]byte, error) {
	if err := s.charge(len(path)); err != nil {
		return nil, err
	}
	// Reuse nonblocking open/type validation. The open helper allocates no file
	// data; our stricter size and aggregate checks precede the first buffer.
	file, err := openLimitedFile(path, false, int(^uint(0)>>1))
	if err != nil {
		return nil, err
	}
	defer file.Close()
	info, err := file.Stat()
	if err != nil {
		return nil, err
	}
	if info.Size() < 0 || info.Size() > xdgmenu.MaxAutostartFileBytes || info.Size() > int64(s.remaining) {
		return nil, errAutostartLimit
	}
	var data []byte
	buffer := make([]byte, 32768)
	for {
		if err := s.ctx.Err(); err != nil {
			return nil, err
		}
		n, readErr := file.Read(buffer)
		if n > xdgmenu.MaxAutostartFileBytes-len(data) {
			return nil, errAutostartLimit
		}
		if err := s.charge(n); err != nil {
			return nil, err
		}
		data = append(data, buffer[:n]...)
		if readErr == io.EOF {
			return data, nil
		}
		if readErr != nil {
			return nil, readErr
		}
	}
}

func autostartDiagnostic(scan *xdgmenu.AutostartScan, name, reason string) {
	if len(scan.Diagnostics) >= xdgmenu.MaxAutostartDiagnostics {
		return
	}
	text := strings.Map(func(c rune) rune {
		if unicode.IsControl(c) {
			return ' '
		}
		return c
	}, name+": "+reason)
	if len(text) > xdgmenu.MaxAutostartDiagnosticBytes {
		text = text[:xdgmenu.MaxAutostartDiagnosticBytes]
		for !utf8.ValidString(text) {
			text = text[:len(text)-1]
		}
	}
	scan.Diagnostics = append(scan.Diagnostics, text)
}

// autostartRootLimit checks the bounded environment before SearchDirectories
// allocates its preference snapshot. Match its absolute-path and dedup policy.
func autostartRootLimit(home string, environment map[string]string) error {
	if !filepath.IsAbs(home) || len(home) > xdgmenu.MaxValueBytes || !utf8.ValidString(home) || strings.ContainsRune(home, 0) {
		return errors.New("invalid autostart home directory")
	}
	for _, key := range []string{"XDG_CONFIG_HOME", "XDG_CONFIG_DIRS", "XDG_CURRENT_DESKTOP", "PATH", "LC_ALL", "LC_MESSAGES", "LANG", "LANGUAGE"} {
		value := environment[key]
		if len(value) > xdgmenu.MaxValueBytes || !utf8.ValidString(value) || strings.ContainsRune(value, 0) {
			return errors.New("invalid or excessive autostart environment")
		}
	}
	user := environment["XDG_CONFIG_HOME"]
	if !filepath.IsAbs(user) {
		user = filepath.Join(home, ".config")
	}
	seen := map[string]bool{filepath.Clean(user): true}
	remaining := environment["XDG_CONFIG_DIRS"]
	if remaining == "" {
		remaining = "/etc/xdg"
	}
	for remaining != "" {
		value, tail, _ := strings.Cut(remaining, ":")
		remaining = tail
		if filepath.IsAbs(value) {
			value = filepath.Clean(value)
			if !seen[value] {
				if len(seen) >= xdgmenu.MaxAutostartConfigRoots {
					return errAutostartLimit
				}
				seen[value] = true
			}
		}
	}
	return nil
}

// ScanAutostart may be called only in the isolated scan worker. Cooperative
// cancellation cannot interrupt a blocked stat/open/read; the session owner
// enforces the worker deadline. These checks are observations, not a guarantee
// that executable files or working directories cannot change before launch.
func ScanAutostart(ctx context.Context, home string, environment map[string]string) (xdgmenu.AutostartScan, error) {
	var none xdgmenu.AutostartScan
	if ctx == nil {
		ctx = context.Background()
	}
	if err := ctx.Err(); err != nil {
		return none, err
	}
	if err := autostartRootLimit(home, environment); err != nil {
		return none, err
	}
	// Autostart consumes config roots only; unrelated data-root environment
	// cannot expand this inventory or prevent the optional startup scan.
	paths, err := xdgmenu.SearchDirectories(home, map[string]string{
		"XDG_CONFIG_HOME": environment["XDG_CONFIG_HOME"],
		"XDG_CONFIG_DIRS": environment["XDG_CONFIG_DIRS"],
	})
	if err != nil {
		return none, err
	}
	source := autostartSource{ctx: ctx, remaining: xdgmenu.MaxAutostartInputBytes}
	winners := make(map[string]string)
	for _, root := range paths.Config {
		directory := filepath.Join(root, "autostart")
		names, err := source.filenames(directory)
		if err != nil {
			return none, fmt.Errorf("autostart inventory: %w", err)
		}
		for _, name := range names {
			if _, exists := winners[name]; exists {
				continue
			}
			if len(winners) >= xdgmenu.MaxAutostartEntries {
				return none, errAutostartLimit
			}
			path := filepath.Join(directory, name)
			if !validPath(path) {
				return none, errors.New("autostart winner path exceeds limits")
			}
			winners[name] = path
		}
	}
	names := make([]string, 0, len(winners))
	for name := range winners {
		names = append(names, name)
	}
	sort.Strings(names)
	resolveExecutable := func(path string) string {
		check := func(path string) bool {
			if !validPath(path) || ctx.Err() != nil {
				return false
			}
			info, err := os.Stat(path)
			return err == nil && info.Mode().IsRegular() &&
				info.Mode().Perm()&0111 != 0 && unix.Access(path, unix.X_OK) == nil
		}
		if filepath.IsAbs(path) {
			if check(path) {
				return path
			}
			return ""
		}
		if strings.ContainsRune(path, '/') {
			return ""
		}
		// Empty/relative PATH components never imply the worker's cwd.
		for remaining := environment["PATH"]; remaining != ""; {
			part, tail, _ := strings.Cut(remaining, ":")
			remaining = tail
			candidate := filepath.Join(part, path)
			if filepath.IsAbs(part) && check(candidate) {
				return candidate
			}
		}
		return ""
	}
	options := xdgmenu.AutostartOptions{Desktops: strings.Split(environment["XDG_CURRENT_DESKTOP"], ":"), Locales: xdgmenu.LocalePreferences(environment), Executable: func(path string) bool { return resolveExecutable(path) != "" },
		Directory: func(path string) bool {
			if ctx.Err() != nil {
				return false
			}
			info, err := os.Stat(path)
			return err == nil && info.IsDir()
		}}
	var scan xdgmenu.AutostartScan
	for _, name := range names {
		path := winners[name]
		data, err := source.read(path)
		if errors.Is(err, errAutostartLimit) || ctx.Err() != nil {
			if ctx.Err() != nil {
				return none, ctx.Err()
			}
			return none, err
		}
		if err != nil {
			autostartDiagnostic(&scan, name, "unavailable: "+err.Error())
			continue
		}
		desktop, err := xdgmenu.ParseDesktop(data)
		if err != nil {
			autostartDiagnostic(&scan, name, "invalid: "+err.Error())
			continue
		}
		launch, reason, err := xdgmenu.PlanAutostart(desktop, name, path, options)
		if err := ctx.Err(); err != nil {
			return none, err
		}
		if err != nil {
			autostartDiagnostic(&scan, name, "invalid: "+err.Error())
			continue
		}
		if len(launch.Argv) != 0 {
			// Pin the observed executable. The session owner must not repeat
			// a PATH lookup that could admit relative or empty components.
			program := resolveExecutable(launch.Argv[0])
			if err := ctx.Err(); err != nil {
				return none, err
			}
			if program == "" {
				autostartDiagnostic(&scan, name, "unavailable: Exec executable not found in absolute PATH components")
				continue
			}
			launch.Argv[0] = program
		}
		if reason != "" {
			autostartDiagnostic(&scan, name, reason)
		}
		if len(launch.Argv) != 0 {
			scan.Launches = append(scan.Launches, launch)
		}
	}
	if err := xdgmenu.ValidateAutostartScan(scan); err != nil {
		return none, err
	}
	return scan, nil
}

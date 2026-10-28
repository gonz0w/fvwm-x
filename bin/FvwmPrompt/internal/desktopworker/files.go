// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopworker

import (
	"context"
	"errors"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"syscall"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

func readBounded(path string) ([]byte, error) {
	fd, err := syscall.Open(path, syscall.O_RDONLY|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0)
	if err != nil {
		return nil, err
	}
	file := os.NewFile(uintptr(fd), path)
	defer file.Close()
	info, err := file.Stat()
	if err != nil {
		return nil, err
	}
	if !info.Mode().IsRegular() || info.Size() > CommandLimit {
		return nil, errors.New("desktop source is not a bounded regular file")
	}
	data, err := io.ReadAll(io.LimitReader(file, CommandLimit+1))
	if len(data) > CommandLimit {
		return nil, errors.New("desktop source grew beyond limit")
	}
	return data, err
}

var errEntryLimit = errors.New("desktop directory entry limit reached")

func entryNames(path string) ([]string, error) {
	file, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer file.Close()
	var names []string
	for len(names) <= 8192 {
		batch, err := file.Readdirnames(64)
		names = append(names, batch...)
		if err == io.EOF {
			sort.Strings(names)
			return names, nil
		}
		if err != nil {
			return nil, err
		}
	}
	return nil, errEntryLimit
}

// launcherExec adopts the shared Desktop Entry grammar while preserving this
// worker's smaller record/argv limits. It never invokes a shell or expansion.
func launcherExec(value, name, location, icon string) ([]string, error) {
	argv, err := xdgmenu.ExpandExec(value, xdgmenu.ExecContext{Name: name, Icon: icon, Location: location})
	if err != nil {
		return nil, err
	}
	if len(argv) > 4096 {
		return nil, errors.New("too many launcher arguments")
	}
	total := 0
	for _, value := range argv {
		if len(value) > CommandLimit-total {
			return nil, errors.New("expanded launcher argv exceeds limit")
		}
		total += len(value)
	}
	return argv, nil
}

type Application struct {
	Name     string   `json:"name"`
	Argv     []string `json:"argv"`
	Cwd      *string  `json:"cwd"`
	Terminal bool     `json:"terminal"`
}

func defaultDirectories() ([]string, error) {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil, err
	}
	paths, err := xdgmenu.SearchDirectories(home, map[string]string{
		"XDG_DATA_HOME": os.Getenv("XDG_DATA_HOME"),
		"XDG_DATA_DIRS": os.Getenv("XDG_DATA_DIRS"),
	})
	if err != nil {
		return nil, err
	}
	directories := make([]string, 0, len(paths.Data))
	for _, root := range paths.Data {
		directories = append(directories, filepath.Join(root, "applications"))
	}
	return directories, nil
}

func applications(ctx context.Context, directories []string, env Environment) ([]Application, error) {
	if directories == nil {
		var err error
		directories, err = defaultDirectories()
		if err != nil {
			return nil, err
		}
	}
	if len(directories) > 64 {
		return nil, errors.New("too many application roots")
	}
	desktops := xdgmenu.CurrentDesktops(os.Getenv("XDG_CURRENT_DESKTOP"))
	locales := xdgmenu.LocalePreferences(map[string]string{
		"LC_ALL": os.Getenv("LC_ALL"), "LC_MESSAGES": os.Getenv("LC_MESSAGES"),
		"LANG": os.Getenv("LANG"), "LANGUAGE": os.Getenv("LANGUAGE"),
	})
	result := []Application{}
	seen := map[string]bool{}
	visited, total := 0, 0
	appendApplication := func(app Application) error {
		size := 128 + 6*len(app.Name)
		for _, value := range app.Argv {
			size += 6*len(value) + 4
		}
		if app.Cwd != nil {
			size += 6 * len(*app.Cwd)
		}
		if size > ResultLimit-total-4096 {
			return errors.New("application result limit reached")
		}
		total += size
		result = append(result, app)
		return nil
	}
	for _, directory := range directories {
		var files []string
		var walk func(string, int) error
		walk = func(path string, depth int) error {
			if ctx.Err() != nil {
				return ctx.Err()
			}
			if depth > 16 {
				return nil
			}
			names, err := env.Entries(path)
			if errors.Is(err, errEntryLimit) {
				return err
			}
			if err != nil {
				return nil
			}
			for _, name := range names {
				visited++
				if visited > 8192 {
					return errors.New("application scan entry limit reached")
				}
				if len(files) >= 2048 {
					break
				}
				child := filepath.Join(path, name)
				info, err := os.Lstat(child)
				if err != nil {
					continue
				}
				if info.IsDir() {
					if err = walk(child, depth+1); err != nil {
						return err
					}
				} else if strings.HasSuffix(name, ".desktop") {
					files = append(files, child)
				}
			}
			return nil
		}
		if err := walk(directory, 0); err != nil {
			return nil, err
		}
		sort.Strings(files)
		for _, path := range files {
			if ctx.Err() != nil {
				return nil, ctx.Err()
			}
			rel, _ := filepath.Rel(directory, path)
			identity := strings.ReplaceAll(rel, string(filepath.Separator), "-")
			if seen[identity] {
				continue
			}
			seen[identity] = true
			data, err := env.Read(path)
			if err != nil {
				continue
			}
			entry, err := xdgmenu.ParseDesktop(data)
			if err != nil {
				continue
			}
			visible, err := entry.Visible(desktops, func(name string) bool { return available(env, name) })
			if err != nil || !visible {
				continue
			}
			name, err := entry.Localized("Desktop Entry", "Name", locales)
			if err != nil || name == "" {
				continue
			}
			icon, err := xdgmenu.LocalizedIcon(entry, "Desktop Entry", locales)
			if err != nil {
				continue
			}
			terminal, err := entry.Boolean("Desktop Entry", "Terminal")
			if err != nil {
				continue
			}
			directory, err := entry.String("Desktop Entry", "Path")
			if err != nil || len(directory) > 4096 {
				continue
			}
			var cwd *string
			if directory != "" {
				if !filepath.IsAbs(directory) {
					continue
				}
				info, err := os.Stat(directory)
				if err != nil || !info.IsDir() {
					continue
				}
				cwd = &directory
			}
			argv, err := launcherExec(entry.Groups["Desktop Entry"]["Exec"], name, path, icon)
			if err != nil || !available(env, argv[0]) {
				continue
			}
			if err := appendApplication(Application{name, argv, cwd, terminal}); err != nil {
				return nil, err
			}
			for _, action := range xdgmenu.DeclaredActions(entry, locales, name, icon) {
				if len(result) == 512 {
					break
				}
				argv, err := launcherExec(action.Exec, name, path, action.Icon)
				if err != nil || !available(env, argv[0]) {
					continue
				}
				if err := appendApplication(Application{action.Label, argv, cwd, terminal}); err != nil {
					return nil, err
				}
			}
			if len(result) == 512 {
				break
			}
		}
		if len(result) == 512 {
			break
		}
	}
	sort.SliceStable(result, func(i, j int) bool { return strings.ToLower(result[i].Name) < strings.ToLower(result[j].Name) })
	return result, nil
}

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

// launcherExec preserves the launcher's existing POSIX word quoting, including
// single quotes and removed unselected file codes. It performs no expansion or
// shell evaluation. The full XDG menu's stricter Exec grammar stays in xdgmenu.
func launcherExec(value, name, location, icon string) ([]string, error) {
	var words []string
	var word strings.Builder
	quote := rune(0)
	escaped, started := false, false
	for _, c := range value {
		if c == 0 {
			return nil, errors.New("NUL in desktop Exec")
		}
		if escaped {
			if quote == '"' && c != '"' && c != '\\' {
				word.WriteRune('\\')
			}
			word.WriteRune(c)
			escaped = false
			started = true
			continue
		}
		if c == '\\' && quote != '\'' {
			escaped = true
			started = true
			continue
		}
		if quote != 0 {
			if c == quote {
				quote = 0
			} else {
				word.WriteRune(c)
			}
			continue
		}
		if c == '\'' || c == '"' {
			quote = c
			started = true
			continue
		}
		if strings.ContainsRune(" \t\r\n", c) {
			if started {
				words = append(words, word.String())
				word.Reset()
				started = false
			}
			continue
		}
		word.WriteRune(c)
		started = true
	}
	if escaped || quote != 0 {
		return nil, errors.New("incomplete desktop Exec quoting")
	}
	if started {
		words = append(words, word.String())
	}
	var argv []string
	for _, word := range words {
		switch word {
		case "%f", "%F", "%u", "%U":
			continue
		case "%i":
			if icon != "" {
				argv = append(argv, "--icon", icon)
			}
		case "%c":
			argv = append(argv, name)
		case "%k":
			argv = append(argv, location)
		default:
			literal := strings.ReplaceAll(word, "%%", "\x01")
			if strings.Contains(literal, "%") {
				return nil, errors.New("unsupported/embedded desktop Exec field code")
			}
			argv = append(argv, strings.ReplaceAll(literal, "\x01", "%"))
		}
	}
	total := 0
	for _, value := range argv {
		if len(value) > CommandLimit-total {
			return nil, errors.New("expanded launcher argv exceeds limit")
		}
		total += len(value)
	}
	if len(argv) > 4096 {
		return nil, errors.New("too many launcher arguments")
	}
	if len(argv) == 0 || argv[0] == "" {
		return nil, errors.New("empty desktop Exec")
	}
	return argv, nil
}

type Application struct {
	Name     string   `json:"name"`
	Argv     []string `json:"argv"`
	Cwd      *string  `json:"cwd"`
	Terminal bool     `json:"terminal"`
}

func defaultDirectories() []string {
	home, _ := os.UserHomeDir()
	user := os.Getenv("XDG_DATA_HOME")
	if user == "" {
		user = filepath.Join(home, ".local/share")
	}
	system := os.Getenv("XDG_DATA_DIRS")
	if system == "" {
		system = "/usr/local/share:/usr/share"
	}
	dirs := []string{filepath.Join(user, "applications")}
	for _, root := range strings.Split(system, ":") {
		if root != "" {
			dirs = append(dirs, filepath.Join(root, "applications"))
		}
	}
	return dirs
}

func applications(ctx context.Context, directories []string, env Environment) ([]Application, error) {
	if directories == nil {
		directories = defaultDirectories()
	}
	if len(directories) > 64 {
		return nil, errors.New("too many application roots")
	}
	current := os.Getenv("XDG_CURRENT_DESKTOP")
	if current == "" {
		current = "FVWM"
	}
	desktops := strings.Split(current, ":")
	result := []Application{}
	seen := map[string]bool{}
	visited, total := 0, 0
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
			fields := entry.Groups["Desktop Entry"]
			if fields["Type"] != "Application" || strings.EqualFold(fields["Hidden"], "true") || strings.EqualFold(fields["NoDisplay"], "true") {
				continue
			}
			matches := func(value string) bool {
				for _, item := range strings.Split(value, ";") {
					for _, desktop := range desktops {
						if item != "" && item == desktop {
							return true
						}
					}
				}
				return false
			}
			if fields["OnlyShowIn"] != "" && !matches(fields["OnlyShowIn"]) || matches(fields["NotShowIn"]) {
				continue
			}
			if fields["TryExec"] != "" && !available(env, fields["TryExec"]) {
				continue
			}
			name := fields["Name"]
			if name == "" {
				name = strings.TrimSuffix(filepath.Base(path), ".desktop")
			}
			argv, err := launcherExec(fields["Exec"], name, path, fields["Icon"])
			if err != nil || !available(env, argv[0]) {
				continue
			}
			var cwd *string
			if value := fields["Path"]; value != "" {
				info, err := os.Stat(value)
				if err != nil || !info.IsDir() {
					continue
				}
				cwd = &value
			}
			size := 128 + 6*len(name)
			for _, value := range argv {
				size += 6*len(value) + 4
			}
			if cwd != nil {
				size += 6 * len(*cwd)
			}
			if size > ResultLimit-total-4096 {
				return nil, errors.New("application result limit reached")
			}
			total += size
			result = append(result, Application{name, argv, cwd, strings.EqualFold(fields["Terminal"], "true")})
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

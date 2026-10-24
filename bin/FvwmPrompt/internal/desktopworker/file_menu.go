// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopworker

import (
	"context"
	"io"
	"os"
	"path/filepath"
	"sort"
	"syscall"
)

type FileMenuEntry struct {
	Path      string `json:"path"`
	Name      string `json:"name"`
	Directory bool   `json:"directory"`
}

func fileMenu(ctx context.Context, path string) ([]FileMenuEntry, error) {
	fd, err := syscall.Open(path, syscall.O_RDONLY|syscall.O_DIRECTORY|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0)
	if err != nil {
		return nil, err
	}
	file := os.NewFile(uintptr(fd), path)
	defer file.Close()
	entries, err := file.ReadDir(1000)
	if err != nil && err != io.EOF {
		return nil, err
	}
	result := []FileMenuEntry{}
	for _, entry := range entries {
		if err := ctx.Err(); err != nil {
			return nil, err
		}
		name := filepath.Join(path, entry.Name())
		info, err := os.Stat(name)
		result = append(result, FileMenuEntry{name, entry.Name(), err == nil && info.IsDir()})
	}
	sort.SliceStable(result, func(i, j int) bool {
		if result[i].Directory != result[j].Directory {
			return result[i].Directory
		}
		return Casefold(result[i].Name) < Casefold(result[j].Name)
	})
	return result[:min(100, len(result))], nil
}

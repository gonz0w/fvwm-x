//go:build go1.25

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filejobs

import (
	"errors"
	"path/filepath"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

// TrashFilesPath is a pure location policy shared by worker and native UI.
// XDG relative values are ignored; no directory is created or inspected here.
func TrashFilesPath(home, dataHome string) (string, error) {
	if dataHome == "" || !filepath.IsAbs(dataHome) {
		if !validPath([]byte(home)) {
			return "", errors.New("home trash location unavailable")
		}
		dataHome = filepath.Join(home, ".local", "share")
	}
	if !validPath([]byte(dataHome)) || len(filepath.Join(dataHome, "Trash", "info", strings.Repeat("0", 32)+".trashinfo.pending")) > filemodel.MaxPath {
		return "", errors.New("trash data path is unavailable or exceeds its budget")
	}
	return filepath.Join(dataHome, "Trash", "files"), nil
}

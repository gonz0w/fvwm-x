// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package runtimepath locates installed self-workers without requiring procfs.
package runtimepath

import (
	"errors"
	"os"
	"path/filepath"
)

// Executable prefers the OS-reported image path. A minimal Linux root may lack
// procfs; then only an explicitly absolute invocation can identify the image.
// Resolve symlinks in either path: OpenBSD can report the invocation alias.
// Never guess through PATH or a working directory that may have changed since
// process construction.
func Executable(invoked string) (string, error) {
	image, err := os.Executable()
	return resolve(image, err, invoked)
}

func resolve(image string, err error, invoked string) (string, error) {
	if err == nil && image != "" {
		return filepath.EvalSymlinks(image)
	}
	if err == nil {
		err = errors.New("Executable path is unavailable")
	}
	if !filepath.IsAbs(invoked) {
		return "", err
	}
	return filepath.EvalSymlinks(invoked)
}

//go:build !linux && !freebsd && !openbsd && !darwin

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"errors"
	"os"
)

func newNativeDirectoryWatch(directory *os.File) (nativeDirectoryWatch, error) {
	return nil, errors.New("native directory watching is unavailable")
}

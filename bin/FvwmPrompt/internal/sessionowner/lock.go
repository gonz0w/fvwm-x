// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"crypto/sha256"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
	"golang.org/x/sys/unix"
)

func privateDirectory(fd int, private bool) (*os.File, error) {
	file := os.NewFile(uintptr(fd), "session owner directory")
	var info unix.Stat_t
	mask := uint32(0022)
	if private {
		mask = 0077
	}
	err := unix.Fstat(fd, &info)
	if err == nil && (info.Mode&unix.S_IFMT != unix.S_IFDIR || info.Uid != uint32(os.Geteuid()) || uint32(info.Mode)&mask != 0) {
		err = errors.New("session owner directory must be owned with safe permissions")
	}
	if err != nil {
		file.Close()
		return nil, err
	}
	return file, nil
}

func openDirectory(path string, private bool) (*os.File, error) {
	if !filepath.IsAbs(path) || len(path) > 4096 {
		return nil, errors.New("invalid session owner directory")
	}
	fd, err := unix.Open(filepath.Clean(path), unix.O_RDONLY|unix.O_DIRECTORY|unix.O_NOFOLLOW|unix.O_NONBLOCK|unix.O_CLOEXEC, 0)
	if err != nil {
		return nil, err
	}
	return privateDirectory(fd, private)
}

// Never chmod an existing directory or replace a lock inode. All leaf creation
// is relative to a held safe parent, so a rename cannot redirect publication.
func childDirectory(parent *os.File, name string, private bool) (*os.File, error) {
	if err := unix.Mkdirat(int(parent.Fd()), name, 0700); err != nil && !errors.Is(err, unix.EEXIST) {
		return nil, err
	}
	fd, err := unix.Openat(int(parent.Fd()), name, unix.O_RDONLY|unix.O_DIRECTORY|unix.O_NOFOLLOW|unix.O_NONBLOCK|unix.O_CLOEXEC, 0)
	if err != nil {
		return nil, err
	}
	return privateDirectory(fd, private)
}

func cacheDirectory(env map[string]string) (*os.File, error) {
	var root *os.File
	var err error
	if path := env["XDG_CACHE_HOME"]; filepath.IsAbs(path) {
		root, err = openDirectory(path, false)
	} else {
		var home *os.File
		home, err = openDirectory(env["HOME"], false)
		if err == nil {
			root, err = childDirectory(home, ".cache", false)
			home.Close()
		}
	}
	if err != nil {
		return nil, err
	}
	defer root.Close()
	fvwm, err := childDirectory(root, "fvwm3", false)
	if err != nil {
		return nil, err
	}
	defer fvwm.Close()
	return childDirectory(fvwm, "session-owner", true)
}

func acquireLock(environment []string) (io.Closer, error) {
	env := environmentMap(environment)
	if env["DISPLAY"] == "" || len(env["DISPLAY"]) > 4096 {
		return nil, errors.New("missing or excessive DISPLAY")
	}
	var directory *os.File
	if env["XDG_RUNTIME_DIR"] != "" {
		directory, _ = openDirectory(env["XDG_RUNTIME_DIR"], true)
	}
	if directory == nil {
		var err error
		directory, err = cacheDirectory(env)
		if err != nil {
			return nil, err
		}
	}
	defer directory.Close()
	key := sha256.Sum256([]byte(localDisplayKey(env["DISPLAY"])))
	name := fmt.Sprintf("fvwm-session-%d-%x.lock", os.Geteuid(), key)
	return xdgsource.LockRecordAt(directory, name)
}

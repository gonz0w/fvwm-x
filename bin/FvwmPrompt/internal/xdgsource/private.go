// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"errors"
	"io"
	"os"
	"path/filepath"
	"syscall"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

func ownedPrivate(info os.FileInfo) bool {
	stat, ok := info.Sys().(*syscall.Stat_t)
	return ok && info.Mode().IsRegular() && stat.Uid == uint32(os.Geteuid()) && stat.Nlink == 1 && info.Mode().Perm()&0077 == 0
}

func ownedWritableRecord(info os.FileInfo) bool {
	stat, ok := info.Sys().(*syscall.Stat_t)
	return ok && info.Mode().IsRegular() && stat.Uid == uint32(os.Geteuid()) &&
		stat.Nlink == 1 && info.Mode().Perm()&0022 == 0
}

// Existing preference files can be readable by other users or managed through
// an owned symlink. Their contents remain data; foreign/group-writable records
// are refused. Publication makes the new file private without changing format.
func ReadSettings(path string) ([]byte, error) {
	if !validPath(path) {
		return nil, errors.New("invalid settings path")
	}
	file, err := openFile(path, false)
	if err != nil {
		return nil, err
	}
	defer file.Close()
	info, err := file.Stat()
	if err != nil {
		return nil, err
	}
	stat, ok := info.Sys().(*syscall.Stat_t)
	if !ok || stat.Uid != uint32(os.Geteuid()) || info.Mode().Perm()&0022 != 0 {
		return nil, errors.New("settings must be owned without group/world write access")
	}
	data, err := io.ReadAll(io.LimitReader(file, xdgmenu.MaxSourceBytes+1))
	if err != nil {
		return nil, err
	}
	if len(data) > xdgmenu.MaxSourceBytes {
		return nil, errors.New("settings grew beyond input limit")
	}
	return data, nil
}

// ReadPrivate reads only a private, owned regular record file. These are worker
// data files, never configuration programs or user-selected application files.
func ReadPrivate(path string) ([]byte, error) {
	if !validPath(path) {
		return nil, errors.New("invalid private record path")
	}
	fd, err := syscall.Open(path, syscall.O_RDONLY|syscall.O_NOFOLLOW|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0)
	if err != nil {
		return nil, err
	}
	file := os.NewFile(uintptr(fd), path)
	defer file.Close()
	info, err := file.Stat()
	if err != nil {
		return nil, err
	}
	if !ownedPrivate(info) || info.Size() > MaxJobBytes {
		return nil, errors.New("record file must be private, owned and bounded")
	}
	data, err := io.ReadAll(io.LimitReader(file, MaxJobBytes+1))
	if err != nil {
		return nil, err
	}
	if len(data) > MaxJobBytes {
		return nil, errors.New("record file grew beyond limit")
	}
	return data, nil
}

// PublishPrivate replaces an absent or private owned file atomically. Like the
// qualified converter's publisher, relative operations stay anchored to an open
// directory inode. Call only from the dedicated worker: process-wide cwd changes
// must not overlap other filesystem jobs. Existing files survive failed writes.
func PublishPrivate(path string, data []byte) (result error) {
	if !validPath(path) || len(data) > MaxJobBytes {
		return errors.New("invalid private output")
	}
	original, err := os.Open(".")
	if err != nil {
		return err
	}
	defer original.Close()
	fd, err := syscall.Open(filepath.Dir(path), syscall.O_RDONLY|syscall.O_DIRECTORY|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0)
	if err != nil {
		return err
	}
	parent := os.NewFile(uintptr(fd), filepath.Dir(path))
	defer parent.Close()
	info, err := parent.Stat()
	if err != nil {
		return err
	}
	stat, ok := info.Sys().(*syscall.Stat_t)
	if !ok || stat.Uid != uint32(os.Geteuid()) || info.Mode().Perm()&0022 != 0 {
		return errors.New("output directory must be owned without group/world write access")
	}
	if err = syscall.Fchdir(int(parent.Fd())); err != nil {
		return err
	}
	defer func() {
		if err := syscall.Fchdir(int(original.Fd())); result == nil {
			result = err
		}
	}()
	name := filepath.Base(path)
	if current, err := os.Lstat(name); err == nil {
		if !ownedWritableRecord(current) {
			return errors.New("refusing output that is not a private owned regular file")
		}
	} else if !os.IsNotExist(err) {
		return err
	}
	file, err := os.CreateTemp(".", ".fvwm-xdg-*")
	if err != nil {
		return err
	}
	defer os.Remove(file.Name())
	if _, err = file.Write(data); err != nil {
		file.Close()
		return err
	}
	if err = file.Close(); err != nil {
		return err
	}
	return os.Rename(file.Name(), name)
}

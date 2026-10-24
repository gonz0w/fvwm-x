// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"io"
	"os"
	"path/filepath"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"golang.org/x/sys/unix"
)

type values = desktopsettings.Values

func newID(bytes int) string {
	data := make([]byte, bytes)
	_, _ = rand.Read(data)
	return hex.EncodeToString(data)
}
func hash(data []byte) string { sum := sha256.Sum256(data); return hex.EncodeToString(sum[:]) }
func stateDirectory(path string) (*os.File, error) {
	parent := filepath.Dir(path)
	if err := os.MkdirAll(parent, 0700); err != nil {
		return nil, err
	}
	fd, err := unix.Open(parent, unix.O_RDONLY|unix.O_DIRECTORY|unix.O_NOFOLLOW|unix.O_CLOEXEC, 0)
	if err != nil {
		return nil, err
	}
	file := os.NewFile(uintptr(fd), parent)
	var info unix.Stat_t
	if err = unix.Fstat(fd, &info); err == nil && (info.Uid != uint32(os.Getuid()) || info.Mode&0022 != 0) {
		err = errors.New("Settings directory must be owned and writable only by you")
	}
	if err != nil {
		file.Close()
		return nil, err
	}
	return file, nil
}
func privateState(fd int) error {
	var info unix.Stat_t
	if err := unix.Fstat(fd, &info); err != nil {
		return err
	}
	if info.Mode&unix.S_IFMT != unix.S_IFREG || info.Uid != uint32(os.Getuid()) || info.Nlink != 1 || info.Mode&0077 != 0 {
		return errors.New("Settings file must be a private owned regular file")
	}
	if info.Size > 8192 {
		return errors.New("Settings file exceeds 8192 bytes")
	}
	return nil
}
func readState(dir *os.File, name string) ([]byte, *string, error) {
	fd, err := unix.Openat(int(dir.Fd()), name, unix.O_RDONLY|unix.O_NOFOLLOW|unix.O_NONBLOCK|unix.O_CLOEXEC, 0)
	if errors.Is(err, unix.ENOENT) {
		return nil, nil, nil
	}
	if err != nil {
		return nil, nil, err
	}
	file := os.NewFile(uintptr(fd), name)
	defer file.Close()
	if err = privateState(fd); err != nil {
		return nil, nil, err
	}
	data, err := io.ReadAll(io.LimitReader(file, 8193))
	if err != nil {
		return nil, nil, err
	}
	if len(data) > 8192 {
		return nil, nil, errors.New("Settings file exceeds 8192 bytes")
	}
	revision := hash(data)
	return data, &revision, nil
}
func loadSettings(path string) (values, *string, error) {
	dir, err := stateDirectory(path)
	if err != nil {
		return nil, nil, err
	}
	defer dir.Close()
	data, revision, err := readState(dir, filepath.Base(path))
	if err != nil {
		return nil, nil, err
	}
	if data == nil {
		return desktopsettings.Defaults(), revision, nil
	}
	result, err := desktopsettings.DecodeState(data)
	return result, revision, err
}
func sameRevision(a, b *string) bool {
	return (a == nil && b == nil) || (a != nil && b != nil && *a == *b)
}

// Replacement is the commit point. All validation, identifiers, encoding and
// fallible writes finish first; close/cleanup cannot report a later refusal.
// All operations stay relative to the same opened directory across renames.
func saveSettings(path string, settings values, expected *string) (*string, error) {
	checked, err := desktopsettings.Checked(settings)
	if err != nil {
		return nil, err
	}
	checked["schema_version"] = 2
	data, err := json.MarshalIndent(checked, "", "  ")
	if err != nil {
		return nil, err
	}
	data = append(data, '\n')
	if len(data) > 8192 {
		return nil, errors.New("Settings file exceeds 8192 bytes")
	}
	revision := hash(data)
	temporary := ".desktop-" + newID(16)
	dir, err := stateDirectory(path)
	if err != nil {
		return nil, err
	}
	defer dir.Close()
	fd, err := unix.Openat(int(dir.Fd()), ".desktop-state.lock", unix.O_CREAT|unix.O_RDWR|unix.O_NOFOLLOW|unix.O_NONBLOCK|unix.O_CLOEXEC, 0600)
	if err != nil {
		return nil, err
	}
	lock := os.NewFile(uintptr(fd), ".desktop-state.lock")
	defer lock.Close()
	if err = privateState(fd); err != nil {
		return nil, err
	}
	if err = unix.Flock(fd, unix.LOCK_EX|unix.LOCK_NB); err != nil {
		return nil, err
	}
	_, current, err := readState(dir, filepath.Base(path))
	if err != nil {
		return nil, err
	}
	if !sameRevision(current, expected) {
		return nil, errors.New("Settings changed externally; reload before saving")
	}
	fd, err = unix.Openat(int(dir.Fd()), temporary, unix.O_WRONLY|unix.O_CREAT|unix.O_EXCL|unix.O_NOFOLLOW|unix.O_CLOEXEC, 0600)
	if err != nil {
		return nil, err
	}
	defer unix.Unlinkat(int(dir.Fd()), temporary, 0)
	file := os.NewFile(uintptr(fd), temporary)
	if _, err = file.Write(data); err == nil {
		err = file.Sync()
	}
	if closeErr := file.Close(); err == nil {
		err = closeErr
	}
	if err != nil {
		return nil, err
	}
	if err = unix.Renameat(int(dir.Fd()), temporary, int(dir.Fd()), filepath.Base(path)); err != nil {
		return nil, err
	}
	return &revision, nil
}

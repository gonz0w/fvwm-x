// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
	"syscall"
)

const stateName = ".fvwm-menu-directory.dir"
const stateLimit = 65536

func ownedRegular(info os.FileInfo) bool {
	stat, ok := info.Sys().(*syscall.Stat_t)
	return ok && info.Mode().IsRegular() && stat.Uid == uint32(os.Getuid()) && info.Mode().Perm()&0022 == 0
}

func readRemembered(dir string) (string, error) {
	fd, err := syscall.Open(filepath.Join(dir, stateName), syscall.O_RDONLY|syscall.O_NOFOLLOW|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0)
	if err != nil {
		if os.IsNotExist(err) {
			return "", nil
		}
		return "", fmt.Errorf("open navigation state: %w", err)
	}
	file := os.NewFile(uintptr(fd), stateName)
	defer file.Close()
	info, err := file.Stat()
	if err != nil {
		return "", err
	}
	if !ownedRegular(info) {
		return "", errors.New("navigation state must be an owned regular file without group/world write access")
	}
	data, err := io.ReadAll(io.LimitReader(file, stateLimit+1))
	if err != nil {
		return "", err
	}
	if len(data) > stateLimit {
		return "", errors.New("navigation state exceeds 64 KiB")
	}
	return strings.TrimSuffix(string(data), "\n"), nil
}

func remember(dir, path string) (result error) {
	if !singleLine(path) || len(path) >= stateLimit {
		return errors.New("invalid navigation path")
	}
	original, err := os.Open(".")
	if err != nil {
		return err
	}
	defer original.Close()
	fd, err := syscall.Open(dir, syscall.O_RDONLY|syscall.O_DIRECTORY|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0)
	if err != nil {
		return err
	}
	parent := os.NewFile(uintptr(fd), dir)
	defer parent.Close()
	info, err := parent.Stat()
	if err != nil {
		return err
	}
	stat, ok := info.Sys().(*syscall.Stat_t)
	if !ok || !info.IsDir() || stat.Uid != uint32(os.Getuid()) || info.Mode().Perm()&0022 != 0 {
		return errors.New("navigation directory must be owned and not group/world writable")
	}
	// This private navigation process has no concurrent filesystem work.
	// Anchor relative operations to the checked directory inode, allowing
	// configured symlink directories without racing their parent pathname.
	if err := syscall.Fchdir(int(parent.Fd())); err != nil {
		return err
	}
	defer func() {
		if err := syscall.Fchdir(int(original.Fd())); result == nil {
			result = err
		}
	}()
	if info, err := os.Lstat(stateName); err == nil {
		if !ownedRegular(info) {
			return errors.New("refusing navigation state that is not an owned regular file")
		}
	} else if !os.IsNotExist(err) {
		return err
	}
	file, err := os.CreateTemp(".", ".fvwm-menu-directory-*")
	if err != nil {
		return err
	}
	name := file.Name()
	defer os.Remove(name)
	if _, err := io.WriteString(file, path+"\n"); err != nil {
		file.Close()
		return err
	}
	if err := file.Close(); err != nil {
		return err
	}
	// Publish a complete private file; never open the destination for writing.
	return os.Rename(name, stateName)
}

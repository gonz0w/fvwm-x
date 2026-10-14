// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"errors"
	"os"
	"syscall"
)

// The persistent inode is never unlinked on release: all concurrent workers
// must contend for the same lock. The dedicated worker closes it on every exit.
func LockRecord(path string) (*os.File, error) {
	if !validPath(path) {
		return nil, errors.New("invalid record lock path")
	}
	fd, err := syscall.Open(path, syscall.O_RDWR|syscall.O_CREAT|syscall.O_NOFOLLOW|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0600)
	if err != nil {
		return nil, err
	}
	file := os.NewFile(uintptr(fd), path)
	info, err := file.Stat()
	if err == nil && !ownedPrivate(info) {
		err = errors.New("record lock must be private and owned")
	}
	if err == nil {
		err = syscall.Flock(fd, syscall.LOCK_EX|syscall.LOCK_NB)
	}
	if err != nil {
		file.Close()
		return nil, err
	}
	return file, nil
}

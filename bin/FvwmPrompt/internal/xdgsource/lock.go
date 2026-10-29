// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"errors"
	"os"
	"strings"
	"syscall"

	"golang.org/x/sys/unix"
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

// LockRecordAt anchors session ownership to the caller's held, validated
// directory inode. It neither changes cwd nor unlinks the persistent lock.
// Directory ownership/mode policy belongs to the session owner.
func LockRecordAt(directory *os.File, name string) (*os.File, error) {
	if directory == nil || name == "" || name == "." || name == ".." ||
		len(name) > 255 || strings.ContainsAny(name, "/\x00") {
		return nil, errors.New("invalid anchored record lock")
	}
	fd, err := unix.Openat(int(directory.Fd()), name, unix.O_RDWR|unix.O_CREAT|unix.O_NOFOLLOW|unix.O_NONBLOCK|unix.O_CLOEXEC, 0600)
	if err != nil {
		return nil, err
	}
	file := os.NewFile(uintptr(fd), name)
	info, err := file.Stat()
	if err == nil && !ownedPrivate(info) {
		err = errors.New("record lock must be private and owned")
	}
	if err == nil {
		err = unix.Flock(fd, unix.LOCK_EX|unix.LOCK_NB)
	}
	if err != nil {
		file.Close()
		return nil, err
	}
	return file, nil
}

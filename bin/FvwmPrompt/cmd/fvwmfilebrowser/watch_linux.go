//go:build linux

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"encoding/binary"
	"errors"
	"fmt"
	"os"
	"syscall"
	"time"
)

type linuxDirectoryWatch struct{ fd int }

func newNativeDirectoryWatch(directory *os.File) (nativeDirectoryWatch, error) {
	fd, err := syscall.InotifyInit1(syscall.IN_CLOEXEC | syscall.IN_NONBLOCK)
	if err != nil {
		return nil, err
	}
	// Bind the already observed directory, rather than resolving the name again.
	// Missing procfs is an explicit manual-refresh fallback.
	_, err = syscall.InotifyAddWatch(fd, fmt.Sprintf("/proc/self/fd/%d", directory.Fd()),
		syscall.IN_ONLYDIR|syscall.IN_CREATE|syscall.IN_DELETE|syscall.IN_MOVED_FROM|
			syscall.IN_MOVED_TO|syscall.IN_MODIFY|syscall.IN_ATTRIB|
			syscall.IN_DELETE_SELF|syscall.IN_MOVE_SELF)
	if err != nil {
		syscall.Close(fd)
		return nil, err
	}
	return &linuxDirectoryWatch{fd}, nil
}

func (w *linuxDirectoryWatch) close() { syscall.Close(w.fd) }

func inotifyChanges(data []byte) (bool, bool, error) {
	changed, lost := false, false
	for len(data) != 0 {
		if len(data) < syscall.SizeofInotifyEvent {
			return false, false, errors.New("short native watch event")
		}
		mask := binary.NativeEndian.Uint32(data[4:8])
		size := uint64(binary.NativeEndian.Uint32(data[12:16]))
		if size > uint64(len(data)-syscall.SizeofInotifyEvent) {
			return false, false, errors.New("invalid native watch event length")
		}
		// Overflow requests a complete snapshot, never a partial event replay.
		changed = true
		lost = lost || mask&(syscall.IN_IGNORED|syscall.IN_UNMOUNT|
			syscall.IN_DELETE_SELF|syscall.IN_MOVE_SELF) != 0
		data = data[syscall.SizeofInotifyEvent+int(size):]
	}
	return changed, lost, nil
}

func (w *linuxDirectoryWatch) next() (bool, bool, error) {
	var buffer [16 * 1024]byte
	n, err := syscall.Read(w.fd, buffer[:])
	if err == syscall.EAGAIN || err == syscall.EINTR {
		time.Sleep(100 * time.Millisecond)
		return false, false, nil
	}
	if err != nil {
		return false, false, err
	}
	if n == 0 {
		return false, false, errors.New("native watch closed")
	}
	return inotifyChanges(buffer[:n])
}

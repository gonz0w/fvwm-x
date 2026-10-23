//go:build freebsd || openbsd || darwin

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"os"
	"syscall"
)

type bsdDirectoryWatch struct{ fd int }

func newNativeDirectoryWatch(directory *os.File) (nativeDirectoryWatch, error) {
	fd, err := syscall.Kqueue()
	if err != nil {
		return nil, err
	}
	syscall.CloseOnExec(fd)
	event := syscall.Kevent_t{Ident: uint64(directory.Fd()), Filter: syscall.EVFILT_VNODE,
		Flags: syscall.EV_ADD | syscall.EV_CLEAR,
		Fflags: syscall.NOTE_WRITE | syscall.NOTE_EXTEND | syscall.NOTE_ATTRIB |
			syscall.NOTE_LINK | syscall.NOTE_DELETE | syscall.NOTE_RENAME | syscall.NOTE_REVOKE}
	_, err = syscall.Kevent(fd, []syscall.Kevent_t{event}, nil, nil)
	if err != nil {
		syscall.Close(fd)
		return nil, err
	}
	return &bsdDirectoryWatch{fd}, nil
}

func (w *bsdDirectoryWatch) close() { syscall.Close(w.fd) }

func (w *bsdDirectoryWatch) next() (bool, bool, error) {
	var events [1]syscall.Kevent_t
	timeout := syscall.NsecToTimespec(100000000)
	n, err := syscall.Kevent(w.fd, nil, events[:], &timeout)
	if err == syscall.EINTR {
		return false, false, nil
	}
	if err != nil || n == 0 {
		return false, false, err
	}
	event := events[0]
	lost := event.Flags&syscall.EV_ERROR != 0 ||
		event.Fflags&(syscall.NOTE_DELETE|syscall.NOTE_RENAME|syscall.NOTE_REVOKE) != 0
	return true, lost, nil
}

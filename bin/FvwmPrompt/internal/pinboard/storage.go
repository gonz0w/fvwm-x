// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package pinboard

import (
	"errors"
	"io"
	"os"
	"path/filepath"
	"strings"
	"syscall"
)

var ErrConflict = errors.New("pinboard revision conflict; reload before editing")

func private(info os.FileInfo, directory bool) bool {
	stat, ok := info.Sys().(*syscall.Stat_t)
	return ok && stat.Uid == uint32(os.Geteuid()) && info.Mode().Perm()&0077 == 0 &&
		(directory && info.IsDir() || !directory && info.Mode().IsRegular() && stat.Nlink == 1)
}

// withStore anchors all checks and replacements to an owned private directory.
// Like xdgsource.PublishPrivate it changes cwd: use only in a dedicated worker,
// never concurrently with another filesystem operation in the same process.
func withStore(path string, operation func(string) error) (result error) {
	if !filepath.IsAbs(path) || filepath.Clean(path) != path || len(path) > 4096 || strings.ContainsRune(path, 0) {
		return errors.New("invalid absolute pinboard state path")
	}
	original, err := os.Open(".")
	if err != nil {
		return err
	}
	defer original.Close()
	fd, err := syscall.Open(filepath.Dir(path), syscall.O_RDONLY|syscall.O_DIRECTORY|syscall.O_NOFOLLOW|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0)
	if err != nil {
		return err
	}
	parent := os.NewFile(uintptr(fd), filepath.Dir(path))
	defer parent.Close()
	info, err := parent.Stat()
	if err != nil {
		return err
	}
	if !private(info, true) {
		return errors.New("pinboard directory must be private and owned")
	}
	if err = syscall.Fchdir(fd); err != nil {
		return err
	}
	defer func() {
		if err := syscall.Fchdir(int(original.Fd())); result == nil {
			result = err
		}
	}()
	return operation(filepath.Base(path))
}

func readState(name string) (State, error) {
	fd, err := syscall.Open(name, syscall.O_RDONLY|syscall.O_NOFOLLOW|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0)
	if os.IsNotExist(err) {
		return Empty(), nil
	}
	if err != nil {
		return State{}, err
	}
	file := os.NewFile(uintptr(fd), name)
	defer file.Close()
	info, err := file.Stat()
	if err != nil {
		return State{}, err
	}
	if !private(info, false) || info.Size() > MaxStateBytes {
		return State{}, errors.New("state must be private, owned, regular and bounded")
	}
	data, err := io.ReadAll(io.LimitReader(file, MaxStateBytes+1))
	if err != nil {
		return State{}, err
	}
	return Decode(data)
}

func Load(path string) (state State, err error) {
	err = withStore(path, func(name string) error { var e error; state, e = readState(name); return e })
	return
}

// Save serializes writers with a persistent nonblocking lock inode. Corruption
// and unknown revisions are errors, never permission to overwrite the record.
func Save(path string, expected uint64, candidate State) (State, error) {
	if err := candidate.Validate(); err != nil {
		return State{}, err
	}
	if expected == ^uint64(0) || candidate.Revision != expected {
		return State{}, ErrConflict
	}
	next := candidate.Clone()
	next.Revision++
	data, err := next.Encode()
	if err != nil {
		return State{}, err
	}
	err = withStore(path, func(name string) error {
		fd, err := syscall.Open(name+".lock", syscall.O_RDWR|syscall.O_CREAT|syscall.O_NOFOLLOW|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0600)
		if err != nil {
			return err
		}
		lock := os.NewFile(uintptr(fd), name+".lock")
		defer lock.Close()
		info, err := lock.Stat()
		if err != nil {
			return err
		}
		if !private(info, false) {
			return errors.New("pinboard lock must be private and owned")
		}
		if err = syscall.Flock(fd, syscall.LOCK_EX|syscall.LOCK_NB); err != nil {
			return errors.New("pinboard writer is busy")
		}
		current, err := readState(name)
		if err != nil {
			return err
		}
		if current.Revision != expected {
			return ErrConflict
		}
		file, err := os.CreateTemp(".", ".fvwm-pinboard-*")
		if err != nil {
			return err
		}
		defer os.Remove(file.Name())
		if _, err = file.Write(data); err != nil {
			file.Close()
			return err
		}
		if err = file.Sync(); err != nil {
			file.Close()
			return err
		}
		if err = file.Close(); err != nil {
			return err
		}
		// Verify the lock pathname still denotes the locked inode before the
		// atomic commit. The record's prior identity is checked again as well.
		lockNow, err := os.Lstat(name + ".lock")
		if err != nil || !os.SameFile(info, lockNow) {
			return errors.New("pinboard lock was replaced")
		}
		latest, err := readState(name)
		if err != nil {
			return err
		}
		if latest.Revision != expected {
			return ErrConflict
		}
		return os.Rename(file.Name(), name)
	})
	if err != nil {
		return State{}, err
	}
	return next, nil
}

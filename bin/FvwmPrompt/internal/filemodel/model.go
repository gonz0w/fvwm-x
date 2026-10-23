// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package filemodel owns literal filename bytes and file target observations.
// Blocking calls belong in a separate process, not a WM or UI event loop.
package filemodel

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"syscall"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
)

const MaxPath = 4096
const MaxWire = 4 * 1024 * 1024

type Identity struct {
	Device uint64      `json:"device"`
	Inode  uint64      `json:"inode"`
	Mode   os.FileMode `json:"mode"`
	Size   int64       `json:"size"`
	MTime  int64       `json:"mtime_ns"`
}

type Item struct {
	ID       string   `json:"id"`
	Name     []byte   `json:"name_bytes"`
	Identity Identity `json:"identity"`
	Kind     string   `json:"kind"`
}

type Snapshot struct {
	Schema int      `json:"schema_version"`
	Path   []byte   `json:"path_bytes"`
	Root   Identity `json:"root"`
	Hidden bool     `json:"hidden"`
	Items  []Item   `json:"items"`
}

func identity(info os.FileInfo) Identity {
	stat := info.Sys().(*syscall.Stat_t)
	return Identity{uint64(stat.Dev), uint64(stat.Ino), info.Mode(), info.Size(), info.ModTime().UnixNano()}
}

func sameRoot(a, b Identity) bool {
	return a.Device == b.Device && a.Inode == b.Inode && a.Mode == b.Mode
}

func validPath(path string) bool {
	return filepath.IsAbs(path) && len(path) <= MaxPath && !strings.ContainsRune(path, 0)
}

func itemID(name []byte, root, value Identity) string {
	// Names are part of identity: hard links are distinct directory entries.
	hash := sha256.Sum256(append([]byte(fmt.Sprintf("%d:%d:%d:%d:%d:", root.Device, root.Inode, value.Device, value.Inode, value.Mode)), name...))
	return hex.EncodeToString(hash[:16])
}

func kind(mode os.FileMode) string {
	switch {
	case mode.IsRegular():
		return "file"
	case mode.IsDir():
		return "directory"
	case mode&os.ModeSymlink != 0:
		return "symlink"
	default:
		return "special"
	}
}

// Scan refuses an over-limit directory as a whole. It never silently truncates
// before sorting. All names, including invalid UTF-8, remain original bytes.
func Scan(path string, hidden bool) (*Snapshot, error) {
	path = filepath.Clean(path)
	if !validPath(path) {
		return nil, errors.New("expected an absolute path of at most 4096 bytes")
	}
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	info, err := f.Stat()
	if err != nil || !info.IsDir() {
		return nil, errors.New("folder is unavailable")
	}
	snapshot := &Snapshot{Schema: 1, Path: []byte(path), Root: identity(info), Hidden: hidden}
	// Include hidden entries in the admission budget even when not displayed.
	entries, err := f.ReadDir(collection.MaxItems + 1)
	if err != nil && err != io.EOF {
		return nil, err
	}
	if len(entries) > collection.MaxItems {
		return nil, errors.New("folder exceeds the 4096-entry limit")
	}
	for _, entry := range entries {
		name := entry.Name()
		if !hidden && strings.HasPrefix(name, ".") {
			continue
		}
		if len(filepath.Join(path, name)) > MaxPath {
			return nil, errors.New("folder contains an over-limit path")
		}
		info, err := entry.Info()
		if err != nil {
			return nil, errors.New("folder changed or an entry is inaccessible; refresh")
		}
		value := identity(info)
		snapshot.Items = append(snapshot.Items, Item{itemID([]byte(name), snapshot.Root, value), []byte(name), value, kind(value.Mode)})
	}
	current, err := os.Stat(path)
	if err != nil || !sameRoot(snapshot.Root, identity(current)) {
		return nil, errors.New("folder was replaced during loading")
	}
	sort.Slice(snapshot.Items, func(i, j int) bool {
		a, b := snapshot.Items[i], snapshot.Items[j]
		if (a.Kind == "directory") != (b.Kind == "directory") {
			return a.Kind == "directory"
		}
		return bytes.Compare(a.Name, b.Name) < 0
	})
	return snapshot, nil
}

// Entries exposes display metadata only. A label is never a path or action.
func (s *Snapshot) Entries() []collection.Entry {
	return entries(s.Items)
}

func entries(items []Item) []collection.Entry {
	entries := make([]collection.Entry, 0, len(items))
	for _, item := range items {
		label := string(item.Name)
		if !utf8.Valid(item.Name) || strings.ContainsAny(label, "\x00\n\r\t") {
			label = strconv.QuoteToGraphic(label)
		}
		for len(label) > 480 {
			_, size := utf8.DecodeLastRuneInString(label)
			label = label[:len(label)-size]
		}
		revision := sha256.Sum256([]byte(fmt.Sprintf("%d:%d:%d",
			item.Identity.Mode, item.Identity.Size, item.Identity.MTime)))
		entries = append(entries, collection.Entry{ID: item.ID, Label: label,
			Kind: item.Kind, Revision: hex.EncodeToString(revision[:16])})
	}
	return entries
}

// Validate is an observation immediately before dispatch, not an atomic lease
// over a pathname passed to another application. Destructive jobs need their
// own descriptor-based contract. Symlinks/special files are unavailable here.
func (s *Snapshot) RootPath() (string, error) {
	if s.Schema != 1 || !validPath(string(s.Path)) || len(s.Items) > collection.MaxItems {
		return "", errors.New("invalid file snapshot")
	}
	root, err := os.Stat(string(s.Path))
	if err != nil || !sameRoot(s.Root, identity(root)) {
		return "", errors.New("folder changed; refresh before acting")
	}
	return string(s.Path), nil
}

func (s *Snapshot) Validate(id string) (string, string, error) {
	if _, err := s.RootPath(); err != nil {
		return "", "", err
	}

	for _, item := range s.Items {
		if item.ID != id {
			continue
		}
		name := string(item.Name)
		if name == "" || name == "." || name == ".." || strings.ContainsAny(name, "/\x00") {
			return "", "", errors.New("invalid file target")
		}
		path := filepath.Join(string(s.Path), name)
		info, err := os.Lstat(path)
		if err != nil || identity(info) != item.Identity {
			return "", "", errors.New("item changed; refresh before acting")
		}
		if item.Kind != "file" && item.Kind != "directory" {
			return "", "", errors.New("symlink and special-file actions are unavailable")
		}
		return path, item.Kind, nil
	}
	return "", "", errors.New("stale file item")
}

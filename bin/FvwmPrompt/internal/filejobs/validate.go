//go:build go1.25

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filejobs

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"syscall"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

func validPath(path []byte) bool {
	s := string(path)
	return len(path) > 0 && len(path) <= filemodel.MaxPath && filepath.IsAbs(s) && filepath.Clean(s) == s && !strings.ContainsRune(s, 0)
}

func validName(name []byte) bool {
	return len(name) > 0 && len(name) <= 255 && string(name) != "." && string(name) != ".." && !strings.ContainsAny(string(name), "/\x00")
}

func stamp(info os.FileInfo) filemodel.Identity {
	s := info.Sys().(*syscall.Stat_t)
	return filemodel.Identity{Device: uint64(s.Dev), Inode: uint64(s.Ino), Mode: info.Mode(), Size: info.Size(), MTime: info.ModTime().UnixNano()}
}

func sameObject(a, b filemodel.Identity) bool {
	return a.Device == b.Device && a.Inode == b.Inode && a.Mode == b.Mode
}

func selected(req Request) ([]filemodel.Item, error) {
	s := req.Snapshot
	if s == nil || s.Schema != 1 || !validPath(s.Path) || len(s.Items) > MaxEntries || !s.Root.Mode.IsDir() {
		return nil, errors.New("invalid observed folder")
	}
	known := make(map[string]filemodel.Item, len(s.Items))
	for _, item := range s.Items {
		if item.ID == "" || len(item.ID) > 64 || !validName(item.Name) || len(filepath.Join(string(s.Path), string(item.Name))) > filemodel.MaxPath || known[item.ID].ID != "" {
			return nil, errors.New("invalid or duplicate observed item")
		}
		known[item.ID] = item
	}
	items := make([]filemodel.Item, 0, len(req.IDs))
	for _, id := range req.IDs {
		item, ok := known[id]
		if !ok || len(id) > 64 {
			return nil, errors.New("unknown or duplicate selected identity")
		}
		if (item.Kind != "file" || !item.Identity.Mode.IsRegular()) &&
			(item.Kind != "directory" || !item.Identity.Mode.IsDir()) &&
			(item.Kind != "symlink" || item.Identity.Mode&os.ModeSymlink == 0) {
			return nil, errors.New("special or inconsistent file type is unavailable")
		}
		items = append(items, item)
		delete(known, id)
	}
	return items, nil
}

// ValidateRequest admits only typed metadata and performs no filesystem work.
// The worker revalidates opened roots and targets before any mutation.
func ValidateRequest(req Request) error {
	if req.Schema != Schema || req.ID == "" || len(req.ID) > 64 || req.Generation == 0 || len(req.IDs) > MaxTargets {
		return errors.New("invalid job envelope")
	}
	for _, c := range req.ID {
		if c < 33 || c > 126 {
			return errors.New("invalid job identifier")
		}
	}
	if _, err := selected(req); err != nil {
		return err
	}
	if req.Operation == "mkdir" {
		if len(req.IDs) != 0 || !validName(req.Name) || len(filepath.Join(string(req.Snapshot.Path), string(req.Name))) > filemodel.MaxPath {
			return errors.New("new folder requires one name and no selected items")
		}
	} else if len(req.IDs) == 0 {
		return errors.New("select one to 256 observed entries")
	}
	switch req.Operation {
	case "mkdir":
	case "rename":
		if len(req.IDs) != 1 || !validName(req.Name) {
			return errors.New("rename requires one item and one component name")
		}
	case "copy", "move", "link":
		if !validPath(req.Destination) {
			return errors.New("destination must be an absolute folder path")
		}
		if len(req.Name) != 0 && (len(req.IDs) != 1 || !validName(req.Name)) {
			return errors.New("a destination name requires one selected item")
		}
	case "trash", "restore", "delete":
		if len(req.Name) != 0 {
			return errors.New("this operation does not accept a name")
		}
	default:
		return errors.New("unsupported file operation")
	}
	if req.Operation != "copy" && req.Operation != "move" && req.Operation != "link" && len(req.Destination) != 0 {
		return errors.New("unexpected destination")
	}
	if req.Permanent != (req.Operation == "delete") {
		return errors.New("permanent deletion requires explicit confirmation")
	}
	return nil
}

func openObserved(req Request) (*os.Root, error) {
	r, err := os.OpenRoot(string(req.Snapshot.Path))
	if err != nil {
		return nil, err
	}
	f, err := r.Open(".")
	if err == nil {
		var info os.FileInfo
		info, err = f.Stat()
		f.Close()
		if err == nil && !sameObject(stamp(info), req.Snapshot.Root) {
			err = errors.New("observed folder changed; refresh before acting")
		}
	}
	if err != nil {
		r.Close()
		return nil, err
	}
	return r, nil
}

func checkNode(root *os.Root, node treeNode, changedDirectory bool) error {
	info, err := root.Lstat(node.path)
	if err != nil {
		return err
	}
	actual := stamp(info)
	if (!changedDirectory || !node.info.IsDir()) && actual != node.identity ||
		changedDirectory && node.info.IsDir() && !sameObject(actual, node.identity) {
		return fmt.Errorf("entry changed before mutation: %q", node.path)
	}
	return nil
}

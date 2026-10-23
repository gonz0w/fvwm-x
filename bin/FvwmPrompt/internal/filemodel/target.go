// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filemodel

import (
	"errors"
	"net/url"
	"path/filepath"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
)

const MaxTarget = 3*MaxPath + 32

// ParseTarget decodes a local URI exactly once. Filename bytes need not be
// UTF-8. URI syntax is never interpreted for ordinary absolute path arguments.
func ParseTarget(value string) ([]byte, error) {
	if len(value) > MaxTarget {
		return nil, errors.New("file target exceeds limit")
	}
	path := value
	if !filepath.IsAbs(value) {
		u, err := url.Parse(value)
		if err != nil || u.Scheme != "file" || u.Opaque != "" || u.User != nil ||
			(u.Host != "" && strings.ToLower(u.Host) != "localhost") ||
			strings.ContainsAny(value, "?#") {
			return nil, errors.New("expected an absolute local path or local file URI without query/fragment")
		}
		path = u.Path
	}
	if !validPath(path) {
		return nil, errors.New("expected an absolute local path of at most 4096 bytes")
	}
	return []byte(path), nil
}

// Locate loads one parent and resolves all requested names before publishing.
// It does not open a target or follow its final symlink. Absolute lexical parent
// names must agree; aliases are not resolved by extra blocking path lookups.
func Locate(targets [][]byte, hidden bool) (*Snapshot, []string, error) {
	if len(targets) == 0 || len(targets) > collection.MaxSelected {
		return nil, nil, errors.New("reveal requires one to 256 targets")
	}
	parent := ""
	names := make([]string, 0, len(targets))
	seen := make(map[string]bool, len(targets))
	for _, target := range targets {
		path := string(target)
		if !validPath(path) {
			return nil, nil, errors.New("invalid reveal target")
		}
		for _, component := range strings.Split(path, string(filepath.Separator)) {
			if component == "." || component == ".." {
				return nil, nil, errors.New("reveal targets cannot contain dot path components")
			}
		}
		path = filepath.Clean(path)
		if path == string(filepath.Separator) {
			return nil, nil, errors.New("the filesystem root has no parent entry; use --dir")
		}
		dir, name := filepath.Dir(path), filepath.Base(path)
		if parent != "" && parent != dir {
			return nil, nil, errors.New("reveal targets must share one parent folder")
		}
		if seen[name] {
			return nil, nil, errors.New("duplicate reveal target")
		}
		parent, seen[name] = dir, true
		names = append(names, name)
		hidden = hidden || strings.HasPrefix(name, ".")
	}
	snapshot, err := Scan(parent, hidden)
	if err != nil {
		return nil, nil, err
	}
	byName := make(map[string]string, len(snapshot.Items))
	for _, item := range snapshot.Items {
		byName[string(item.Name)] = item.ID
	}
	ids := make([]string, 0, len(names))
	for _, name := range names {
		id, exists := byName[name]
		if !exists {
			return nil, nil, errors.New("a requested target is missing; no targets selected")
		}
		ids = append(ids, id)
	}
	return snapshot, ids, nil
}

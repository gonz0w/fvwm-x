// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"errors"
	"path/filepath"
	"strings"
	"unicode"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

const (
	fileManagerMaxGroups   = 8
	fileManagerMaxURIs     = 8
	fileManagerMaxURIBytes = 16 * 1024
	fileManagerMaxFrame    = 64 * 1024
	fileManagerMaxReply    = 512
)

// Planning is lexical only. ParseTarget owns the sole URI decode, including
// non-UTF-8 filename bytes; filesystem observation remains in browser workers.
func planFileManager(frame fileManagerFrame) ([]options, error) {
	if frame.Method != "ShowFolders" && frame.Method != "ShowItems" && frame.Method != "ShowItemProperties" {
		return nil, errors.New("unsupported FileManager1 method")
	}
	if len(frame.URIs) == 0 || len(frame.URIs) > fileManagerMaxURIs {
		return nil, errors.New("expected one to eight local file URIs")
	}
	if !validFileManagerStartupID(frame.StartupID) {
		return nil, errors.New("invalid startup ID")
	}
	groups := make([]options, 0, fileManagerMaxGroups)
	byFolder := make(map[string]int)
	seen := make(map[string]bool)
	total := 0
	for _, uri := range frame.URIs {
		if len(uri) > fileManagerMaxURIBytes-total || !utf8.ValidString(uri) || !strings.HasPrefix(uri, "file://") {
			return nil, errors.New("expected bounded local file URIs")
		}
		total += len(uri)
		path, err := filemodel.ParseTarget(uri)
		if err != nil {
			return nil, err
		}
		for _, component := range strings.Split(string(path), "/") {
			if component == "." || component == ".." {
				return nil, errors.New("dot path components are not supported")
			}
		}
		clean := filepath.Clean(string(path))
		if frame.Method != "ShowFolders" && clean == "/" {
			return nil, errors.New("the filesystem root has no parent entry")
		}
		if seen[clean] {
			continue
		}
		seen[clean] = true
		folder := clean
		if frame.Method != "ShowFolders" {
			folder = filepath.Dir(clean)
		}
		index, found := byFolder[folder]
		if !found {
			if len(groups) == fileManagerMaxGroups {
				return nil, errors.New("too many folder groups")
			}
			index = len(groups)
			byFolder[folder] = index
			groups = append(groups, options{dir: folder, properties: frame.Method == "ShowItemProperties"})
		}
		if frame.Method != "ShowFolders" {
			groups[index].targets = append(groups[index].targets, []byte(clean))
		}
	}
	return groups, nil
}

func validFileManagerStartupID(value string) bool {
	if len(value) > 512 || !utf8.ValidString(value) {
		return false
	}
	for _, c := range value {
		if !unicode.IsPrint(c) {
			return false
		}
	}
	return true
}

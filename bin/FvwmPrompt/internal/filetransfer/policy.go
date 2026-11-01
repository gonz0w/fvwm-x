// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package filetransfer handles ordinary file references only. It never accepts
// clipboard text, images, protected content, executable subjects or labels.
package filetransfer

import (
	"errors"
	"path/filepath"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

const (
	MaxReferences = 256
	MaxURIBytes   = 256 * 1024
	MaxGroups     = 8
	MaxFrame      = 512 * 1024
	MaxQueue      = 8
)

type Group struct {
	Parent string
	Paths  [][]byte
}

func validHost(host string) bool {
	if host == "" || len(host) > 255 {
		return false
	}
	for _, c := range host {
		if !(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '-' || c == '.') {
			return false
		}
	}
	return true
}

// Parse validates authority without decoding the path, then delegates the sole
// percent decode to ParseTarget. Replacing only authority permits the captured
// real hostname without expanding filemodel's other local-target consumers.
func Parse(text, hostname string) ([]Group, error) {
	if len(text) == 0 || len(text) > MaxURIBytes {
		return nil, errors.New("invalid file reference size")
	}
	for _, c := range []byte(text) {
		if c >= 128 || c == 0 {
			return nil, errors.New("file references must be encoded ASCII")
		}
	}
	groups := []Group{}
	parents, seen := map[string]int{}, map[string]bool{}
	count := 0
	for _, line := range strings.Split(text, "\n") {
		line = strings.TrimSuffix(line, "\r")
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		count++
		if count > MaxReferences || !strings.HasPrefix(line, "file://") || strings.ContainsAny(line, "?#\r\t ") {
			return nil, errors.New("invalid file references")
		}
		suffix := strings.TrimPrefix(line, "file://")
		slash := strings.IndexByte(suffix, '/')
		if slash < 0 {
			return nil, errors.New("missing absolute file path")
		}
		host := suffix[:slash]
		if strings.ContainsAny(host, "@:%") || (host != "" && !strings.EqualFold(host, "localhost") && (!validHost(hostname) || !strings.EqualFold(host, hostname))) {
			return nil, errors.New("nonlocal file reference")
		}
		// Keep the escaped path untouched for ParseTarget's sole decode.
		path, err := filemodel.ParseTarget("file://localhost" + suffix[slash:])
		if err != nil {
			return nil, errors.New("invalid file path")
		}
		for _, component := range strings.Split(string(path), "/") {
			if component == "." || component == ".." {
				return nil, errors.New("dot path component")
			}
		}
		clean := filepath.Clean(string(path))
		if clean == "/" {
			return nil, errors.New("root has no source entry")
		}
		if seen[clean] {
			continue
		}
		seen[clean] = true
		parent := filepath.Dir(clean)
		index, ok := parents[parent]
		if !ok {
			if len(groups) == MaxGroups {
				return nil, errors.New("too many source folders")
			}
			index = len(groups)
			parents[parent] = index
			groups = append(groups, Group{Parent: parent})
		}
		groups[index].Paths = append(groups[index].Paths, []byte(clean))
	}
	if len(groups) == 0 {
		return nil, errors.New("no file references")
	}
	return groups, nil
}

// Encode preserves every filename byte. Slash is the only structural path byte;
// all non-unreserved bytes are escaped, including percent and invalid UTF-8.
func Encode(paths [][]byte, hostname string) (string, error) {
	if !validHost(hostname) || len(paths) == 0 || len(paths) > MaxReferences {
		return "", errors.New("invalid outgoing file references")
	}
	var out strings.Builder
	const hex = "0123456789ABCDEF"
	for _, path := range paths {
		if len(path) > filemodel.MaxPath || len(path) < 2 || path[0] != '/' || strings.IndexByte(string(path), 0) >= 0 {
			return "", errors.New("invalid outgoing path")
		}
		for _, component := range strings.Split(string(path), "/") {
			if component == "." || component == ".." {
				return "", errors.New("dot path component")
			}
		}
		out.WriteString("file://" + hostname)
		for _, c := range path {
			if c == '/' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '-' || c == '.' || c == '_' || c == '~' {
				out.WriteByte(c)
			} else {
				out.WriteByte('%')
				out.WriteByte(hex[c>>4])
				out.WriteByte(hex[c&15])
			}
		}
		out.WriteString("\r\n")
		if out.Len() > MaxURIBytes {
			return "", errors.New("outgoing reference budget exceeded")
		}
	}
	return out.String(), nil
}

// Lease is local bookkeeping; native clear always repeats the same conditional
// check against actual X ownership. Completion never authorizes source deletion.
type Lease struct{ ID, Action string }

func (l *Lease) Lost(id string) {
	if l.ID == id {
		*l = Lease{}
	}
}
func (l Lease) ClearAfter(id, action string, success bool) bool {
	return success && action == "move" && l.Action == "move" && id != "" && id == l.ID
}

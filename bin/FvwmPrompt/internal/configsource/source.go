// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
// Package configsource validates the shared version 1 configuration origin.
package configsource

import (
	"encoding/json"
	"errors"
	"path/filepath"
	"strconv"
	"unicode"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

const Limit = 1536

type Site struct {
	Path        string
	First, Last uint32
}

type Source struct {
	Kind, Reason string
	Site         Site
	Include      []Site
}

// encoding/json replaces unpaired UTF-16 escapes. Reject them before decoding
// so a displayed source path never silently names a different file.
func validSourceEscapes(data []byte) bool {
	for i := 0; i < len(data); i++ {
		if data[i] != '\\' {
			continue
		}
		i++
		if i >= len(data) {
			return false
		}
		if data[i] != 'u' {
			continue
		}
		if i+4 >= len(data) {
			return false
		}
		v, err := strconv.ParseUint(string(data[i+1:i+5]), 16, 16)
		if err != nil {
			return false
		}
		i += 4
		if v >= 0xdc00 && v <= 0xdfff {
			return false
		}
		if v >= 0xd800 && v <= 0xdbff {
			if i+6 >= len(data) || data[i+1] != '\\' || data[i+2] != 'u' {
				return false
			}
			low, err := strconv.ParseUint(string(data[i+3:i+7]), 16, 16)
			if err != nil || low < 0xdc00 || low > 0xdfff {
				return false
			}
			i += 6
		}
	}
	return true
}
func sourceString(value any) (string, bool) {
	text, ok := value.(string)
	if !ok {
		return "", false
	}
	for _, r := range text {
		if unicode.IsControl(r) {
			return "", false
		}
	}
	return text, true
}
func sourceLine(value any) (uint32, bool) {
	number, ok := value.(json.Number)
	if !ok {
		return 0, false
	}
	v, err := strconv.ParseUint(string(number), 10, 32)
	return uint32(v), err == nil && v != 0
}
func decodeSourceSite(fields map[string]any, required bool) (Site, error) {
	site := Site{}
	_, path := fields["path"]
	_, first := fields["first"]
	_, last := fields["last"]
	if !path && !first && !last && !required {
		return site, nil
	}
	var ok bool
	if site.Path, ok = sourceString(fields["path"]); !ok || !filepath.IsAbs(site.Path) {
		return site, errors.New("invalid source path")
	}
	if site.First, ok = sourceLine(fields["first"]); !ok {
		return site, errors.New("invalid source start")
	}
	if site.Last, ok = sourceLine(fields["last"]); !ok || site.Last < site.First {
		return site, errors.New("invalid source end")
	}
	return site, nil
}

// Decode rejects malformed or incomplete metadata without inventing a source.
func Decode(data []byte) (*Source, error) {
	invalid := errors.New("invalid configuration source")
	if len(data) == 0 || len(data) > Limit || !utf8.Valid(data) || !validSourceEscapes(data) {
		return nil, invalid
	}
	value, err := desktopsettings.DecodeJSON(data)
	if err != nil {
		return nil, invalid
	}
	fields, ok := value.(map[string]any)
	if !ok {
		return nil, invalid
	}
	for key := range fields {
		switch key {
		case "version", "kind", "path", "first", "last", "include", "reason":
		default:
			return nil, invalid
		}
	}
	version, ok := fields["version"].(json.Number)
	if !ok || version != "1" {
		return nil, invalid
	}
	kind, ok := fields["kind"].(string)
	if !ok || (kind != "file" && kind != "generated" && kind != "invocation" && kind != "unavailable") {
		return nil, invalid
	}
	source := &Source{Kind: kind}
	if source.Site, err = decodeSourceSite(fields, kind == "file"); err != nil {
		return nil, invalid
	}
	if value, exists := fields["reason"]; exists {
		if source.Reason, ok = sourceString(value); !ok {
			return nil, invalid
		}
	}
	if value, exists := fields["include"]; exists {
		parents, ok := value.([]any)
		if !ok || len(parents) > 16 || (len(parents) != 0 && source.Site.Path == "") {
			return nil, invalid
		}
		for _, value := range parents {
			parent, ok := value.(map[string]any)
			if !ok || len(parent) != 3 {
				return nil, invalid
			}
			site, err := decodeSourceSite(parent, true)
			if err != nil {
				return nil, invalid
			}
			source.Include = append(source.Include, site)
		}
	}
	return source, nil
}

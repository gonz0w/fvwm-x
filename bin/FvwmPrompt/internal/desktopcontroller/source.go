// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"path/filepath"
	"strconv"
	"strings"
	"unicode"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

const sourceLimit = 1536

type sourceSite struct {
	path        string
	first, last uint32
}
type configSource struct {
	kind, reason string
	site         sourceSite
	include      []sourceSite
}

// Only the immediately following complete packet may consume this record.
// Fragmentation leaves the record alone; any intervening packet retires it.
type sourcePair struct {
	key    uint64
	source *configSource
}

func (p *sourcePair) consume(packet frame) *configSource {
	previous := *p
	*p = sourcePair{}
	if packet.kind == configSourceMessage {
		if len(packet.body) < 25 || binary.NativeEndian.Uint64(packet.body) == 0 ||
			binary.NativeEndian.Uint64(packet.body[8:]) != 0 || binary.NativeEndian.Uint64(packet.body[16:]) != 0 {
			return nil
		}
		data := packet.body[24:]
		end := bytes.IndexByte(data, 0)
		if end < 0 || end > sourceLimit {
			return nil
		}
		source, err := decodeConfigSource(data[:end])
		if err == nil {
			*p = sourcePair{binary.NativeEndian.Uint64(packet.body), source}
		}
		return nil
	}
	if packet.kind == configInfo && len(packet.body) >= 24 && previous.key != 0 &&
		binary.NativeEndian.Uint64(packet.body) == previous.key {
		return previous.source
	}
	return nil
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
func decodeSourceSite(fields map[string]any, required bool) (sourceSite, error) {
	site := sourceSite{}
	_, path := fields["path"]
	_, first := fields["first"]
	_, last := fields["last"]
	if !path && !first && !last && !required {
		return site, nil
	}
	var ok bool
	if site.path, ok = sourceString(fields["path"]); !ok || !filepath.IsAbs(site.path) {
		return site, errors.New("invalid source path")
	}
	if site.first, ok = sourceLine(fields["first"]); !ok {
		return site, errors.New("invalid source start")
	}
	if site.last, ok = sourceLine(fields["last"]); !ok || site.last < site.first {
		return site, errors.New("invalid source end")
	}
	return site, nil
}
func decodeConfigSource(data []byte) (*configSource, error) {
	invalid := errors.New("invalid configuration source")
	if len(data) == 0 || len(data) > sourceLimit || !utf8.Valid(data) || !validSourceEscapes(data) {
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
	source := &configSource{kind: kind}
	if source.site, err = decodeSourceSite(fields, kind == "file"); err != nil {
		return nil, invalid
	}
	if value, exists := fields["reason"]; exists {
		if source.reason, ok = sourceString(value); !ok {
			return nil, invalid
		}
	}
	if value, exists := fields["include"]; exists {
		parents, ok := value.([]any)
		if !ok || len(parents) > 16 || (len(parents) != 0 && source.site.path == "") {
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
			source.include = append(source.include, site)
		}
	}
	return source, nil
}
func (s sourceSite) text() string {
	return fmt.Sprintf("%s:%d-%d", s.path, s.first, s.last)
}
func (s *configSource) text() string {
	if s == nil {
		return "Source context unavailable"
	}
	var lines []string
	if s.kind == "file" {
		lines = append(lines, "Source: "+s.site.text())
	} else {
		lines = append(lines, "Source: "+s.kind+"; definition context unavailable")
		if s.site.path != "" {
			lines = append(lines, "Invocation: "+s.site.text())
		}
	}
	for _, parent := range s.include {
		lines = append(lines, "Read from "+parent.text())
	}
	if s.reason != "" {
		lines = append(lines, "Context: "+s.reason)
	}
	return strings.Join(lines, "\n")
}

type configurationMode bool

const (
	validateConfiguration configurationMode = false
	applyConfiguration    configurationMode = true
)

// Startup applies retained options before finishConfiguration. Subsequent
// delivery is diagnostic-only; valid configured changes still require restart.
func (d *Controller) configureMessage(text string, source *configSource, mode configurationMode) {
	key, value, ok := desktopConfiguration(text)
	if !ok {
		return
	}
	var err error
	if _, known := desktopsettings.Registry()[key]; known {
		if mode == applyConfiguration {
			err = d.configure(key, value)
		} else {
			_, err = desktopsettings.Value(key, value, true)
		}
	} else if key == "terminal" {
		if mode == applyConfiguration {
			d.terminal = detectTerminal(value)
		}
	} else {
		err = errors.New("Unknown FvwmDesktop option")
	}
	if err != nil {
		d.configurationError(err, source)
	}
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/configsource"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

const sourceLimit = configsource.Limit

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

// Keep packet pairing and diagnostic presentation local to the controller.
// Schema validation belongs to the same decoder used by read-only inspection.
func decodeConfigSource(data []byte) (*configSource, error) {
	source, err := configsource.Decode(data)
	if err != nil {
		return nil, err
	}
	site := func(value configsource.Site) sourceSite {
		return sourceSite{value.Path, value.First, value.Last}
	}
	result := &configSource{kind: source.Kind, reason: source.Reason, site: site(source.Site)}
	for _, parent := range source.Include {
		result.include = append(result.include, site(parent))
	}
	return result, nil
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
	} else if key == "scratchpad" {
		if mode == applyConfiguration {
			err = d.configureScratchpad(value)
		} else {
			_, err = parseScratchpad(value)
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

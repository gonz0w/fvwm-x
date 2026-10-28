//go:build go1.25

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package filejobs executes bounded, descriptor-anchored file operations in an
// isolated worker. Paths are literal bytes; presentation belongs to the caller.
package filejobs

import "github.com/fvwmorg/FvwmPrompt/internal/filemodel"

const (
	Schema     = 1
	MaxTargets = 256
	MaxEntries = 4096
	MaxDepth   = 64
	ChunkBytes = 64 * 1024
	MaxRecord  = 32 * 1024
	MaxRequest = filemodel.MaxWire
)

type Request struct {
	Schema      int                 `json:"schema_version"`
	ID          string              `json:"id"`
	Generation  uint64              `json:"generation"`
	Operation   string              `json:"operation"`
	Snapshot    *filemodel.Snapshot `json:"snapshot"`
	IDs         []string            `json:"ids,omitempty"`
	Destination []byte              `json:"destination_bytes,omitempty"`
	Name        []byte              `json:"name_bytes,omitempty"`
	Permanent   bool                `json:"permanent,omitempty"`
}

type Control struct {
	Schema   int    `json:"schema_version"`
	ID       string `json:"id"`
	Kind     string `json:"kind"`
	Decision uint64 `json:"decision,omitempty"`
	Choice   string `json:"choice,omitempty"`
	Name     []byte `json:"name_bytes,omitempty"`
}

type Outcome struct {
	State              string `json:"state"`
	Detail             string `json:"detail,omitempty"`
	SourceRemaining    bool   `json:"source_remaining"`
	DestinationPresent bool   `json:"destination_present"`
	ContentComplete    bool   `json:"content_complete"`
	MetadataComplete   bool   `json:"metadata_complete"`
}

type Event struct {
	Schema      int      `json:"schema_version"`
	ID          string   `json:"id"`
	Generation  uint64   `json:"generation"`
	Sequence    uint64   `json:"sequence"`
	Kind        string   `json:"kind"`
	State       string   `json:"state"`
	Item        int      `json:"item,omitempty"`
	Path        []byte   `json:"path_bytes,omitempty"`
	Destination []byte   `json:"destination_bytes,omitempty"`
	Done        uint64   `json:"done"`
	Total       uint64   `json:"total"`
	TotalKnown  bool     `json:"total_known"`
	Bytes       uint64   `json:"bytes"`
	Decision    uint64   `json:"decision,omitempty"`
	Detail      string   `json:"detail,omitempty"`
	Outcome     *Outcome `json:"outcome,omitempty"`
}

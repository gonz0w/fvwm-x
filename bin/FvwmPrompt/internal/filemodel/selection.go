// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filemodel

import (
	"errors"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
)

const MaxActionBytes = 32 * 1024

// selectedItems performs bounded metadata-only admission before any filesystem
// calls. Order comes from the caller's admitted view, never a display label.
func (s *Snapshot) selectedItems(ids []string) ([]Item, error) {
	if s == nil || s.Schema != 1 ||
		len(s.Items) > collection.MaxItems || len(ids) == 0 || len(ids) > collection.MaxSelected {
		return nil, errors.New("select between 1 and 256 current entries")
	}
	known := make(map[string]Item, len(s.Items))
	for _, item := range s.Items {
		if item.ID == "" || len(item.ID) > 64 {
			return nil, errors.New("invalid snapshot identity")
		}
		if _, exists := known[item.ID]; exists {
			return nil, errors.New("duplicate snapshot identity")
		}
		known[item.ID] = item
	}
	items := make([]Item, 0, len(ids))
	for _, id := range ids {
		if id == "" || len(id) > 64 {
			return nil, errors.New("invalid selected identity")
		}
		item, exists := known[id]
		if !exists {
			return nil, errors.New("unknown or duplicate selected identity")
		}
		items = append(items, item)
		delete(known, id)
	}
	return items, nil
}

// ValidateSelection is worker-only. Admit all arguments and revalidate every
// observation before returning any paths to the launcher. Path observations
// are not descriptor leases; file-operation jobs retain their own contract.
func (s *Snapshot) ValidateSelection(ids []string) ([]string, error) {
	items, err := s.selectedItems(ids)
	if err != nil {
		return nil, err
	}
	if len(s.Path) > MaxPath || !validPath(string(s.Path)) {
		return nil, errors.New("invalid file snapshot path")
	}
	paths := make([]string, 0, len(items))
	remaining := MaxActionBytes
	for _, item := range items {
		if len(item.Name) > MaxPath {
			return nil, errors.New("file target exceeds path limit")
		}
		name := string(item.Name)
		if name == "" || name == "." || name == ".." || strings.ContainsAny(name, "/\x00") {
			return nil, errors.New("invalid file target")
		}
		if item.Kind != "file" && item.Kind != "directory" {
			return nil, errors.New("symlink and special-file actions are unavailable")
		}
		if kind(item.Identity.Mode) != item.Kind {
			return nil, errors.New("inconsistent file type observation")
		}
		path := filepath.Join(string(s.Path), name)
		if len(path) > MaxPath || len(path)+1 > remaining {
			return nil, errors.New("selected paths exceed the 32 KiB argument budget")
		}
		remaining -= len(path) + 1
		paths = append(paths, path)
	}
	if _, err := s.RootPath(); err != nil {
		return nil, err
	}
	for i, item := range items {
		info, err := os.Lstat(paths[i])
		if err != nil || identity(info) != item.Identity {
			return nil, errors.New("selected entry changed; refresh before acting")
		}
	}
	return paths, nil
}

type SelectionSummary struct {
	Count, Files, Directories, Links, Special int
	FileBytes                                 int64
}

func summarizeItems(items []Item) (*SelectionSummary, error) {
	summary := &SelectionSummary{Count: len(items)}
	for _, item := range items {
		switch item.Kind {
		case "file":
			if item.Identity.Size < 0 || item.Identity.Size > math.MaxInt64-summary.FileBytes {
				return nil, errors.New("observed file-size total exceeds limit")
			}
			summary.Files++
			summary.FileBytes += item.Identity.Size
		case "directory":
			summary.Directories++
		case "symlink":
			summary.Links++
		case "special":
			summary.Special++
		default:
			return nil, errors.New("unknown observed file type")
		}
	}
	return summary, nil
}

// SummarizeSelection exposes typed admitted observations without filesystem or
// content work. Wire consumers choose exact integer encoding independently.
func (s *Snapshot) SummarizeSelection(ids []string) (*SelectionSummary, error) {
	items, err := s.selectedItems(ids)
	if err != nil {
		return nil, err
	}
	return summarizeItems(items)
}

// ObservedSelection is metadata-only, including links and special entries. It
// reads neither files nor recursive directory contents and never claims freshness.
func (s *Snapshot) ObservedSelection(ids []string) ([]string, error) {
	items, err := s.selectedItems(ids)
	if err != nil {
		return nil, err
	}
	if len(items) == 1 {
		return s.ObservedDetails(items[0].ID)
	}
	summary, err := summarizeItems(items)
	if err != nil {
		return nil, err
	}
	return []string{
		fmt.Sprintf("Observed selection: %d entries; %d files; %d folders", summary.Count, summary.Files, summary.Directories),
		fmt.Sprintf("Regular-file bytes: %d; %d links; %d special entries", summary.FileBytes, summary.Links, summary.Special),
		"Folder contents and link targets are not counted. No file content is read.",
	}, nil
}

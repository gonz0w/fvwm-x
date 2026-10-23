// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filemodel

import (
	"bytes"
	"cmp"
	"errors"
	"fmt"
	"sort"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
)

// OrderedEntries projects an admitted snapshot without filesystem work or
// altering target observations. Directories stay first in either direction.
func (s *Snapshot) OrderedEntries(key string, reverse bool) ([]collection.Entry, error) {
	if key != "name" && key != "kind" && key != "size" && key != "modified" {
		return nil, errors.New("unsupported file ordering")
	}
	if len(s.Items) > collection.MaxItems {
		return nil, errors.New("file collection exceeds limit")
	}
	items := append([]Item(nil), s.Items...)
	sort.Slice(items, func(i, j int) bool {
		a, b := items[i], items[j]
		if (a.Kind == "directory") != (b.Kind == "directory") {
			return a.Kind == "directory"
		}
		order := 0
		switch key {
		case "kind":
			order = cmp.Compare(a.Kind, b.Kind)
		case "size":
			order = cmp.Compare(a.Identity.Size, b.Identity.Size)
		case "modified":
			order = cmp.Compare(a.Identity.MTime, b.Identity.MTime)
		}
		if order == 0 {
			order = bytes.Compare(a.Name, b.Name)
		}
		if reverse {
			return order > 0
		}
		return order < 0
	})
	return entries(items), nil
}

// ObservedDetails describes the last scan, including symlinks/special files.
// It neither follows a link nor claims that the observation is still current.
func (s *Snapshot) ObservedDetails(id string) ([]string, error) {
	for _, item := range s.Items {
		if item.ID == id {
			return []string{
				"Observed name: " + entries([]Item{item})[0].Label,
				fmt.Sprintf("Kind: %s; mode: %s; size: %d bytes", item.Kind, item.Identity.Mode, item.Identity.Size),
				"Modified (UTC): " + time.Unix(0, item.Identity.MTime).UTC().Format(time.RFC3339Nano),
			}, nil
		}
	}
	return nil, errors.New("stale file item")
}

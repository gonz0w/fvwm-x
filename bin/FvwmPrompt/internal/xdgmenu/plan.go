// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"errors"
	"path/filepath"
	"strings"
)

// CompiledMenu joins allocation policy with the presentation metadata for the
// same consolidated menu. Definitions, layouts and desktop records are borrowed
// immutable inputs; Plan pools and directory slices belong to this result.
type CompiledMenu struct {
	Plan          *MenuPlan
	Definition    *Node
	DirectoryDirs []string
	DefaultLayout *Node
	Children      []*CompiledMenu
}

type planState struct {
	inventories        map[AppLocation]map[string]PoolEntry
	work, items, bytes int
}

// AppLocation distinguishes a legacy hierarchy's filename/prefix policy from
// a normal AppDir at the same filesystem path. It is operation-owned metadata,
// never an invented filesystem path or a desktop-file identity.
type AppLocation struct {
	Path   string
	Prefix string
	Legacy bool
}

func ApplicationLocation(node *Node) (AppLocation, error) {
	if node == nil || node.Tag != "AppDir" {
		return AppLocation{}, errors.New("expected AppDir")
	}
	location := AppLocation{Path: strings.Trim(node.Text, " \t\r\n"),
		Prefix: node.Attr["fvwm-legacy-prefix"], Legacy: node.Attr["fvwm-legacy"] == "true"}
	if !pathValue(location.Path) || !filepath.IsAbs(location.Path) || filepath.Clean(location.Path) != location.Path ||
		!pathValue(location.Prefix) || strings.Contains(location.Prefix, "/") || !location.Legacy && location.Prefix != "" {
		return AppLocation{}, errors.New("invalid application location")
	}
	return location, nil
}

func (s *planState) charge(items, bytes int) error {
	if items < 0 || bytes < 0 || items > s.items || bytes > s.bytes || items > s.work {
		return errors.New("menu plan construction exceeds total limits")
	}
	s.items -= items
	s.work -= items
	s.bytes -= bytes
	return nil
}

// BuildMenuPlans uses inventories already acquired by the worker. Missing
// directories supply no entries. Ancestor pools are copied before local AppDirs
// are applied, so a child's source overrides its parent without mutating it.
// Hidden entries remain in the pool until after allocation and visibility.
func BuildMenuPlans(root *Node, inventories map[AppLocation]map[string]PoolEntry) (*CompiledMenu, error) {
	if root == nil {
		return nil, nil
	}
	if len(inventories) > MaxItems {
		return nil, errors.New("too many application inventories")
	}
	s := &planState{inventories: inventories, work: MaxAllocationWork, items: MaxItems, bytes: MaxSourceBytes}
	for location, pool := range inventories {
		path := location.Path
		if !pathValue(path) || !filepath.IsAbs(path) || filepath.Clean(path) != path ||
			!pathValue(location.Prefix) || strings.Contains(location.Prefix, "/") || !location.Legacy && location.Prefix != "" {
			return nil, errors.New("invalid inventory directory")
		}
		if err := s.charge(1+len(pool), len(path)+len(location.Prefix)); err != nil {
			return nil, err
		}
		for id, entry := range pool {
			if !pathValue(id) || !strings.HasSuffix(id, ".desktop") || strings.Contains(id, "/") || entry.Entry == nil || !pathValue(entry.Path) || !filepath.IsAbs(entry.Path) || filepath.Clean(entry.Path) != entry.Path {
				return nil, errors.New("invalid inventory entry")
			}
			if err := s.charge(0, len(id)+len(entry.Path)); err != nil {
				return nil, err
			}
		}
	}
	return s.menu(root, nil, nil, nil, 0)
}

func (s *planState) menu(root *Node, inherited map[string]PoolEntry, dirs []string, layout *Node, depth int) (*CompiledMenu, error) {
	if depth >= MaxDepth {
		return nil, errors.New("menu plan exceeds depth")
	}
	name, err := menuName(root)
	if err != nil {
		return nil, err
	}
	if err = s.charge(1+len(root.Children)+len(inherited)+len(dirs), len(name)); err != nil {
		return nil, err
	}
	pool := make(map[string]PoolEntry, len(inherited))
	for id, entry := range inherited {
		if err = s.charge(0, len(id)+len(entry.Path)); err != nil {
			return nil, err
		}
		pool[id] = entry
	}
	plan := &MenuPlan{Name: name, Pool: pool}
	result := &CompiledMenu{Plan: plan, Definition: root, DirectoryDirs: append([]string(nil), dirs...), DefaultLayout: layout}
	for _, child := range root.Children {
		switch child.Tag {
		case "AppDir":
			location, err := ApplicationLocation(child)
			if err != nil {
				return nil, err
			}
			local := s.inventories[location]
			if err = s.charge(len(local), 0); err != nil {
				return nil, err
			}
			for id, entry := range local {
				if err = s.charge(0, len(id)+len(entry.Path)); err != nil {
					return nil, err
				}
				pool[id] = entry
			}
		case "DirectoryDir":
			path := strings.Trim(child.Text, " \t\r\n")
			if !pathValue(path) || !filepath.IsAbs(path) || filepath.Clean(path) != path {
				return nil, errors.New("unresolved directory metadata path")
			}
			if err = s.charge(1, len(path)); err != nil {
				return nil, err
			}
			result.DirectoryDirs = append(result.DirectoryDirs, path)
		case "Include", "Exclude":
			plan.Rules = append(plan.Rules, child)
		case "OnlyUnallocated":
			plan.OnlyUnallocated = true
		case "NotOnlyUnallocated":
			plan.OnlyUnallocated = false
		case "DefaultLayout":
			result.DefaultLayout = child
		case "MergeFile", "MergeDir", "LegacyDir", "DefaultMergeDirs", "DefaultAppDirs", "DefaultDirectoryDirs", "Move":
			// Move nodes are retained by the merger as metadata, but are already
			// applied. All acquisition directives must have been resolved.
			if child.Tag != "Move" {
				return nil, errors.New("unresolved menu acquisition directive")
			}
		case "Deleted":
			// A later NotDeleted may override this; pruning belongs to MergeMenus.
		}
	}
	last := make(map[string]int)
	for i, path := range result.DirectoryDirs {
		last[path] = i
	}
	unique := make([]string, 0, len(last))
	for i, path := range result.DirectoryDirs {
		if last[path] == i {
			unique = append(unique, path)
		}
	}
	result.DirectoryDirs = unique
	seen := make(map[string]bool)
	for _, child := range root.Children {
		if child.Tag != "Menu" {
			continue
		}
		compiled, err := s.menu(child, pool, result.DirectoryDirs, result.DefaultLayout, depth+1)
		if err != nil {
			return nil, err
		}
		if seen[compiled.Plan.Name] {
			return nil, errors.New("unconsolidated child menu names")
		}
		seen[compiled.Plan.Name] = true
		plan.Children = append(plan.Children, compiled.Plan)
		result.Children = append(result.Children, compiled)
	}
	return result, nil
}

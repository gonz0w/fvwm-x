// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"errors"
	"fmt"
	"io/fs"
	"path/filepath"
	"sort"
	"strings"
)

// MenuSource supplies bounded acquisition outside the policy engine. Returned
// trees and inventories remain caller-owned. Missing optional merges are
// ignored; other acquisition errors are returned. A work count cannot interrupt
// a blocked callback: the production worker must own its deadline separately.
type MenuSource interface {
	ReadMenu(string) ([]byte, error)
	MenuFiles(string) ([]string, error)
	LegacyMenu(directory, prefix string) (*Node, error)
}

type kdeLegacySource interface {
	KDELegacyDirectories() ([]string, error)
}

type mergeState struct {
	source             MenuSource
	paths              SearchPaths
	base               string
	active             map[string]bool
	files              map[string]*Node
	bytes, nodes, work int
}

func (s *mergeState) charge(count int) error {
	if count < 0 || count > s.work {
		return errors.New("menu merging exceeds work limit")
	}
	s.work -= count
	return nil
}

func (s *mergeState) node(tag, text string, attr map[string]string) (*Node, error) {
	if s.nodes <= 0 || !pathValue(tag) || !pathValue(text) || len(attr) > MaxItems {
		return nil, errors.New("menu merging exceeds node/value limits")
	}
	s.nodes--
	if err := s.charge(1 + len(attr)); err != nil {
		return nil, err
	}
	count := len(tag) + len(text)
	for key, value := range attr {
		if !pathValue(key) || !pathValue(value) || len(key)+len(value) > s.bytes-count {
			return nil, errors.New("menu merging exceeds attribute limits")
		}
		count += len(key) + len(value)
	}
	if count > s.bytes {
		return nil, errors.New("menu merging exceeds text limit")
	}
	s.bytes -= count
	copy := &Node{Tag: tag, Text: text}
	if len(attr) != 0 {
		copy.Attr = make(map[string]string, len(attr))
		for key, value := range attr {
			copy.Attr[key] = value
		}
	}
	return copy, nil
}

// MergeMenus acquires, expands and consolidates one menu job. File-relative
// paths are resolved before their elements are inserted into another file.
// Inputs, parsed-source caches and other jobs are never mutated. A deleted root
// returns nil without error. The tree retains layout and unknown extensions.
func MergeMenus(filename string, paths SearchPaths, source MenuSource) (*Node, error) {
	if source == nil || !pathValue(filename) || !filepath.IsAbs(filename) ||
		!strings.HasSuffix(filename, ".menu") || len(paths.Config)+len(paths.Data) > MaxItems {
		return nil, errors.New("invalid menu merge inputs")
	}
	s := &mergeState{source: source, paths: paths, active: make(map[string]bool),
		files: make(map[string]*Node), bytes: MaxSourceBytes, nodes: MaxItems, work: MaxAllocationWork}
	for _, dirs := range [][]string{paths.Config, paths.Data} {
		for _, dir := range dirs {
			if !pathValue(dir) || !filepath.IsAbs(dir) || len(dir) > s.bytes {
				return nil, errors.New("invalid menu search directory")
			}
			s.bytes -= len(dir)
		}
	}
	s.base = strings.TrimSuffix(filepath.Base(filename), ".menu")
	if s.base == "applications" || strings.HasSuffix(s.base, "-applications") {
		s.base = "applications"
	}
	root, err := s.file(filepath.Clean(filename), 0)
	if err != nil {
		return nil, err
	}
	if err = s.consolidate(root, 0); err != nil {
		return nil, err
	}
	if err = s.moves(root, 0); err != nil {
		return nil, err
	}
	return s.prune(root, 0)
}

func menuName(node *Node) (string, error) {
	if node == nil || node.Tag != "Menu" {
		return "", errors.New("expected Menu element")
	}
	name := ""
	found := false
	for _, child := range node.Children {
		if child == nil {
			return "", errors.New("nil menu element")
		}
		if child.Tag == "Name" {
			if found || len(child.Children) != 0 {
				return "", errors.New("duplicate or nested menu name")
			}
			found = true
			name = strings.Trim(child.Text, " \t\r\n")
		}
	}
	if !found || name == "" || !pathValue(name) || strings.Contains(name, "/") {
		return "", errors.New("missing or invalid menu name")
	}
	return name, nil
}

func (s *mergeState) read(filename string) (*Node, error) {
	if err := s.charge(1); err != nil {
		return nil, err
	}
	if tree, ok := s.files[filename]; ok {
		return tree, nil
	}
	data, err := s.source.ReadMenu(filename)
	if err != nil {
		return nil, err
	}
	if len(data) > s.bytes {
		return nil, errors.New("menu sources exceed total byte limit")
	}
	s.bytes -= len(data)
	tree, err := ParseMenuXML(data)
	if err == nil {
		s.files[filename] = tree
	}
	return tree, err
}

func (s *mergeState) file(filename string, depth int) (*Node, error) {
	if depth >= MaxDepth {
		return nil, errors.New("menu merging exceeds file depth")
	}
	if s.active[filename] {
		return nil, nil
	}
	tree, err := s.read(filename)
	if err != nil {
		return nil, fmt.Errorf("menu %s: %w", filename, err)
	}
	s.active[filename] = true
	defer delete(s.active, filename)
	return s.expand(tree, filename, depth)
}

func absoluteMenuPath(origin, value string) (string, error) {
	value = strings.Trim(value, " \t\r\n")
	if value == "" || !pathValue(value) {
		return "", errors.New("invalid menu path")
	}
	if !filepath.IsAbs(value) {
		value = filepath.Join(filepath.Dir(origin), value)
	}
	value = filepath.Clean(value)
	if !pathValue(value) {
		return "", errors.New("resolved menu path exceeds limit")
	}
	return value, nil
}

func (s *mergeState) parent(origin string) (string, error) {
	for i, dir := range s.paths.Config {
		if err := s.charge(1); err != nil {
			return "", err
		}
		relative, err := filepath.Rel(dir, origin)
		if err != nil || relative == ".." || strings.HasPrefix(relative, ".."+string(filepath.Separator)) {
			continue
		}
		for _, next := range s.paths.Config[i+1:] {
			candidate := filepath.Join(next, relative)
			if !pathValue(candidate) {
				return "", errors.New("parent menu path exceeds limit")
			}
			if _, err := s.read(candidate); err == nil {
				return candidate, nil
			} else if !errors.Is(err, fs.ErrNotExist) {
				return "", err
			}
		}
		break
	}
	return "", nil
}

func (s *mergeState) defaults(tag, suffix string, dirs []string) ([]*Node, error) {
	var result []*Node
	for i := len(dirs) - 1; i >= 0; i-- {
		n, err := s.node(tag, filepath.Join(dirs[i], suffix), nil)
		if err != nil {
			return nil, err
		}
		result = append(result, n)
	}
	return result, nil
}

func (s *mergeState) expand(input *Node, origin string, depth int) (*Node, error) {
	if input == nil || depth >= MaxDepth {
		return nil, errors.New("invalid or deep menu tree")
	}
	output, err := s.node(input.Tag, input.Text, input.Attr)
	if err != nil {
		return nil, err
	}
	if input.Tag != "Menu" {
		for _, child := range input.Children {
			copy, err := s.expand(child, origin, depth+1)
			if err != nil {
				return nil, err
			}
			output.Children = append(output.Children, copy)
		}
		return output, nil
	}
	if _, err := menuName(input); err != nil {
		return nil, err
	}
	// Normalize directives before expansion so the last repeated merge wins,
	// even when the first occurrence would insert overriding rules.
	var children []*Node
	last := make(map[string]int)
	for _, child := range input.Children {
		if child == nil {
			return nil, errors.New("nil menu child")
		}
		var added []*Node
		switch child.Tag {
		case "DefaultAppDirs":
			added, err = s.defaults("AppDir", "applications", s.paths.Data)
		case "DefaultDirectoryDirs":
			added, err = s.defaults("DirectoryDir", "desktop-directories", s.paths.Data)
		case "DefaultMergeDirs":
			added, err = s.defaults("MergeDir", filepath.Join("menus", s.base+"-merged"), s.paths.Config)
		case "KDELegacyDirs":
			if provider, ok := s.source.(kdeLegacySource); ok {
				var dirs []string
				dirs, err = provider.KDELegacyDirectories()
				if err == nil {
					err = s.charge(len(dirs))
				}
				for i := len(dirs) - 1; err == nil && i >= 0; i-- {
					if !filepath.IsAbs(dirs[i]) {
						return nil, errors.New("relative KDE legacy directory")
					}
					var n *Node
					n, err = s.node("LegacyDir", filepath.Clean(dirs[i]), map[string]string{"prefix": "kde-"})
					if n != nil {
						added = append(added, n)
					}
				}
			} else {
				return nil, errors.New("KDE legacy inventory is unavailable")
			}
		case "AppDir", "DirectoryDir", "MergeDir", "LegacyDir", "MergeFile":
			var value string
			if child.Tag == "MergeFile" && child.Attr["type"] == "parent" {
				value, err = s.parent(origin)
			} else {
				value, err = absoluteMenuPath(origin, child.Text)
			}
			if err == nil && value != "" {
				var copy *Node
				copy, err = s.node(child.Tag, value, child.Attr)
				added = []*Node{copy}
			}
		default:
			added = []*Node{child}
		}
		if err != nil {
			return nil, err
		}
		for _, n := range added {
			if err = s.charge(1); err != nil {
				return nil, err
			}
			children = append(children, n)
			switch n.Tag {
			case "MergeFile", "MergeDir", "LegacyDir":
				last[n.Tag+"\x00"+n.Text] = len(children) - 1
			}
		}
	}
	for i, child := range children {
		if index, ok := last[child.Tag+"\x00"+child.Text]; ok && index != i {
			continue
		}
		var merged []*Node
		switch child.Tag {
		case "MergeFile":
			if kind := child.Attr["type"]; kind != "" && kind != "path" && kind != "parent" {
				return nil, errors.New("unknown MergeFile type")
			}
			var tree *Node
			tree, err = s.file(child.Text, depth+1)
			if errors.Is(err, fs.ErrNotExist) {
				err = nil
			}
			if tree != nil {
				merged = tree.Children
			}
		case "MergeDir":
			var names []string
			names, err = s.source.MenuFiles(child.Text)
			if errors.Is(err, fs.ErrNotExist) {
				err = nil
			}
			if err == nil {
				err = s.charge(len(names))
			}
			if err == nil {
				names = append([]string(nil), names...)
				sort.Strings(names)
				for _, name := range names {
					if !pathValue(name) || name == "" || filepath.Base(name) != name {
						return nil, errors.New("invalid merge inventory")
					}
					if !strings.HasSuffix(name, ".menu") {
						continue
					}
					filename, pathErr := absoluteMenuPath(filepath.Join(child.Text, "placeholder"), name)
					if pathErr != nil {
						return nil, pathErr
					}
					var tree *Node
					tree, err = s.file(filename, depth+1)
					if errors.Is(err, fs.ErrNotExist) {
						err = nil
					}
					if err != nil {
						break
					}
					if tree != nil {
						for _, n := range tree.Children {
							if n.Tag != "Name" {
								merged = append(merged, n)
							}
						}
					}
				}
			}
		case "LegacyDir":
			prefix := child.Attr["prefix"]
			if !pathValue(prefix) || strings.Contains(prefix, "/") {
				return nil, errors.New("invalid legacy prefix")
			}
			var tree *Node
			tree, err = s.source.LegacyMenu(child.Text, prefix)
			if errors.Is(err, fs.ErrNotExist) {
				err = nil
			}
			if err == nil && tree != nil {
				tree, err = s.expand(tree, filepath.Join(child.Text, "legacy.menu"), depth+1)
				if tree != nil {
					merged = tree.Children
				}
			}
		default:
			var copy *Node
			copy, err = s.expand(child, origin, depth+1)
			if copy != nil {
				output.Children = append(output.Children, copy)
			}
		}
		if err != nil {
			return nil, err
		}
		for _, n := range merged {
			if err = s.charge(1); err != nil {
				return nil, err
			}
			if n.Tag != "Name" {
				output.Children = append(output.Children, n)
			}
		}
	}
	return output, nil
}

func withoutName(children []*Node) []*Node {
	var result []*Node
	for _, n := range children {
		if n.Tag != "Name" {
			result = append(result, n)
		}
	}
	return result
}

func (s *mergeState) consolidate(menu *Node, depth int) error {
	if depth >= MaxDepth {
		return errors.New("consolidated menu exceeds depth")
	}
	if _, err := menuName(menu); err != nil {
		return err
	}
	if err := s.charge(len(menu.Children)); err != nil {
		return err
	}
	last := make(map[string]int)
	groups := make(map[string][]*Node)
	for i, child := range menu.Children {
		key := ""
		switch child.Tag {
		case "Menu":
			name, err := menuName(child)
			if err != nil {
				return err
			}
			key = "Menu\x00" + name
			if err := s.charge(len(child.Children)); err != nil {
				return err
			}
			groups[key] = append(groups[key], withoutName(child.Children)...)
		case "AppDir", "DirectoryDir", "Directory":
			key = child.Tag + "\x00" + strings.Trim(child.Text, " \t\r\n")
		}
		if key != "" {
			last[key] = i
		}
	}
	var result []*Node
	for i, child := range menu.Children {
		key := ""
		if child.Tag == "Menu" {
			name, _ := menuName(child)
			key = "Menu\x00" + name
		} else if child.Tag == "AppDir" || child.Tag == "DirectoryDir" || child.Tag == "Directory" {
			key = child.Tag + "\x00" + strings.Trim(child.Text, " \t\r\n")
		}
		if key != "" && last[key] != i {
			continue
		}
		if child.Tag == "Menu" {
			var nameNode *Node
			for _, n := range child.Children {
				if n.Tag == "Name" {
					nameNode = n
					break
				}
			}
			child.Children = append([]*Node{nameNode}, groups[key]...)
			if err := s.consolidate(child, depth+1); err != nil {
				return err
			}
		}
		result = append(result, child)
	}
	menu.Children = result
	return nil
}

func menuPath(value string) ([]string, error) {
	value = strings.Trim(value, " \t\r\n")
	if !pathValue(value) || value == "" {
		return nil, errors.New("empty or invalid Move path")
	}
	parts := strings.Split(value, "/")
	if len(parts) >= MaxDepth {
		return nil, errors.New("deep Move path")
	}
	for _, part := range parts {
		if part == "" {
			return nil, errors.New("empty Move component")
		}
	}
	return parts, nil
}

func (s *mergeState) find(menu *Node, parts []string, create bool) (parent, found *Node, err error) {
	for _, name := range parts {
		parent, found = menu, nil
		if err = s.charge(len(menu.Children)); err != nil {
			return nil, nil, err
		}
		for _, child := range menu.Children {
			if child.Tag == "Menu" {
				value, _ := menuName(child)
				if value == name {
					found = child
					break
				}
			}
		}
		if found == nil && create {
			found, err = s.node("Menu", "", nil)
			if err != nil {
				return nil, nil, err
			}
			n, err := s.node("Name", name, nil)
			if err != nil {
				return nil, nil, err
			}
			found.Children = []*Node{n}
			parent.Children = append(parent.Children, found)
		}
		if found == nil {
			return parent, nil, nil
		}
		menu = found
	}
	return parent, found, nil
}

func (s *mergeState) moves(menu *Node, depth int) error {
	if depth >= MaxDepth {
		return errors.New("moved menu exceeds depth")
	}
	if err := s.charge(len(menu.Children)); err != nil {
		return err
	}
	for _, child := range menu.Children {
		if child.Tag == "Menu" {
			if err := s.moves(child, depth+1); err != nil {
				return err
			}
		}
	}
	var operations []*Node
	for _, n := range menu.Children {
		if n.Tag == "Move" {
			operations = append(operations, n)
		}
	}
	for _, operation := range operations {
		if len(operation.Children)%2 != 0 {
			return errors.New("unpaired Move path")
		}
		for i := 0; i < len(operation.Children); i += 2 {
			old, new := operation.Children[i], operation.Children[i+1]
			if old.Tag != "Old" || new.Tag != "New" || len(old.Children)+len(new.Children) != 0 {
				return errors.New("invalid Move pair")
			}
			from, err := menuPath(old.Text)
			if err != nil {
				return err
			}
			to, err := menuPath(new.Text)
			if err != nil {
				return err
			}
			if strings.Join(from, "/") == strings.Join(to, "/") {
				continue
			}
			// A descendant destination would retain its own ancestor. Reject the
			// operation rather than creating a recursive output tree.
			if len(to) > len(from) && strings.Join(to[:len(from)], "/") == strings.Join(from, "/") {
				return errors.New("Move destination is inside origin")
			}
			parent, source, err := s.find(menu, from, false)
			if err != nil {
				return err
			}
			if source == nil {
				continue
			}
			_, target, err := s.find(menu, to, true)
			if err != nil {
				return err
			}
			if err = s.charge(len(source.Children) + len(parent.Children)); err != nil {
				return err
			}
			target.Children = append(withoutName(source.Children), target.Children...)
			for j, n := range parent.Children {
				if n == source {
					parent.Children = append(parent.Children[:j], parent.Children[j+1:]...)
					break
				}
			}
			if err = s.consolidate(menu, depth); err != nil {
				return err
			}
		}
	}
	return nil
}

func (s *mergeState) prune(menu *Node, depth int) (*Node, error) {
	if depth >= MaxDepth {
		return nil, errors.New("menu exceeds final depth")
	}
	if err := s.charge(len(menu.Children)); err != nil {
		return nil, err
	}
	deleted := false
	for _, child := range menu.Children {
		if child.Tag == "Deleted" {
			deleted = true
		}
		if child.Tag == "NotDeleted" {
			deleted = false
		}
	}
	if deleted {
		return nil, nil
	}
	var result []*Node
	for _, child := range menu.Children {
		if child.Tag == "Menu" {
			var err error
			child, err = s.prune(child, depth+1)
			if err != nil {
				return nil, err
			}
		}
		if child != nil {
			result = append(result, child)
		}
	}
	menu.Children = result
	return menu, nil
}

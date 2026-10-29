// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"errors"
	"fmt"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

type ItemKind uint8

const (
	ApplicationItem ItemKind = iota
	SubmenuItem
	SeparatorItem
	HeaderItem
)

// PresentedItem contains literal application arguments, never a shell command.
// Menu rendering and process launch each own their encoding boundary.
type PresentedItem struct {
	Kind        ItemKind
	Label, Icon string
	Entry       *AllocatedEntry
	Menu        *PresentedMenu
	Argv        []string
	Terminal    bool
	WorkingDir  string
	Startup     *StartupMetadata
}
type PresentedMenu struct {
	Name, Label, Icon string
	Items             []PresentedItem
}
type PresentationOptions struct {
	Locales, Desktops []string
	Executable        func(string) bool
	Directories       map[string]*Desktop
}
type presentationState struct {
	options          PresentationOptions
	remaining, bytes int
}
type layoutOptions struct {
	showEmpty, inline, header, alias bool
	limit                            int
}

func (s *presentationState) charge(count, bytes int) error {
	if count < 0 || bytes < 0 || count > s.remaining || bytes > s.bytes {
		return errors.New("menu presentation exceeds total limits")
	}
	s.remaining -= count
	s.bytes -= bytes
	return nil
}

// PresentMenus applies visibility after allocation, resolves localized directory
// labels, then follows the last Layout or inherited DefaultLayout. Inputs remain
// immutable. Nil is a deleted/hidden menu. Unknown extension nodes are retained
// in the definition but do not turn into actions.
func PresentMenus(compiled *CompiledMenu, allocated *AllocatedMenu, options PresentationOptions) (*PresentedMenu, error) {
	if compiled == nil && allocated == nil {
		return nil, nil
	}
	if len(options.Locales)+len(options.Desktops)+len(options.Directories) > MaxItems {
		return nil, errors.New("excessive presentation inputs")
	}
	s := &presentationState{options: options, remaining: MaxItems, bytes: MaxSourceBytes}
	for _, values := range [][]string{options.Locales, options.Desktops} {
		for _, value := range values {
			if !pathValue(value) {
				return nil, errors.New("invalid presentation environment")
			}
			if err := s.charge(1, len(value)); err != nil {
				return nil, err
			}
		}
	}
	return s.menu(compiled, allocated, 0)
}

func (s *presentationState) metadata(compiled *CompiledMenu) (label, icon string, hidden bool, err error) {
	label = compiled.Plan.Name
	names := childTextsPolicy(compiled.Definition, "Directory")
	for i := len(names) - 1; i >= 0; i-- {
		relative := strings.Trim(names[i], " \t\r\n")
		if !pathValue(relative) || filepath.IsAbs(relative) || filepath.Clean(relative) != relative || relative == ".." || strings.HasPrefix(relative, "../") || !strings.HasSuffix(relative, ".directory") {
			return "", "", false, errors.New("invalid directory entry reference")
		}
		for j := len(compiled.DirectoryDirs) - 1; j >= 0; j-- {
			if err = s.charge(1, 0); err != nil {
				return
			}
			path := filepath.Join(compiled.DirectoryDirs[j], relative)
			if !pathValue(path) {
				return "", "", false, errors.New("directory entry path exceeds limit")
			}
			d := s.options.Directories[path]
			if d == nil {
				continue
			}
			kind, e := d.String("Desktop Entry", "Type")
			if e != nil {
				return "", "", false, e
			}
			if kind != "Directory" {
				return "", "", false, errors.New("directory metadata has wrong Type")
			}
			label, err = d.Localized("Desktop Entry", "Name", s.options.Locales)
			if err != nil {
				return
			}
			icon, err = LocalizedIcon(d, "Desktop Entry", s.options.Locales)
			if err != nil {
				return
			}
			for _, key := range []string{"Hidden", "NoDisplay"} {
				var value bool
				value, err = d.Boolean("Desktop Entry", key)
				if err != nil {
					return
				}
				hidden = hidden || value
			}
			return
		}
	}
	return
}

func childTextsPolicy(node *Node, tag string) []string {
	var values []string
	for _, child := range node.Children {
		if child.Tag == tag {
			values = append(values, child.Text)
		}
	}
	return values
}

func parseLayoutOptions(attr map[string]string, base layoutOptions) (layoutOptions, error) {
	for key, value := range attr {
		switch key {
		case "show_empty", "inline", "inline_header", "inline_alias":
			if value != "true" && value != "false" {
				return base, fmt.Errorf("invalid layout boolean %s", key)
			}
			v := value == "true"
			switch key {
			case "show_empty":
				base.showEmpty = v
			case "inline":
				base.inline = v
			case "inline_header":
				base.header = v
			case "inline_alias":
				base.alias = v
			}
		case "inline_limit":
			limit, err := strconv.Atoi(value)
			if err != nil || limit < 0 || limit > MaxItems {
				return base, errors.New("invalid layout inline limit")
			}
			base.limit = limit
		}
	}
	return base, nil
}

func (s *presentationState) chargeApplication(item PresentedItem, action bool) error {
	count := len(item.Argv)
	if action {
		count++
	}
	if err := s.charge(count, len(item.Label)+len(item.Icon)+len(item.WorkingDir)+StartupMetadataBytes(item.Startup)); err != nil {
		return err
	}
	for _, arg := range item.Argv {
		if err := s.charge(0, len(arg)); err != nil {
			return err
		}
	}
	return nil
}

func (s *presentationState) menu(compiled *CompiledMenu, allocated *AllocatedMenu, depth int) (*PresentedMenu, error) {
	if compiled == nil || allocated == nil || compiled.Plan == nil || compiled.Definition == nil || depth >= MaxDepth || compiled.Plan.Name != allocated.Name || len(compiled.Children) != len(allocated.Children) {
		return nil, errors.New("mismatched or deep presentation tree")
	}
	if err := s.charge(1+len(allocated.Entries)+len(compiled.Children), len(allocated.Name)); err != nil {
		return nil, err
	}
	label, icon, hidden, err := s.metadata(compiled)
	if err != nil {
		return nil, err
	}
	if hidden {
		return nil, nil
	}
	if err = s.charge(0, len(label)+len(icon)); err != nil {
		return nil, err
	}
	result := &PresentedMenu{Name: allocated.Name, Label: label, Icon: icon}
	menus := make(map[string]PresentedItem)
	files := make(map[string][]PresentedItem)
	for i, child := range compiled.Children {
		menu, err := s.menu(child, allocated.Children[i], depth+1)
		if err != nil {
			return nil, err
		}
		if menu != nil {
			menus[menu.Name] = PresentedItem{Kind: SubmenuItem, Label: menu.Label, Icon: menu.Icon, Menu: menu}
		}
	}
	for i := range allocated.Entries {
		entry := &allocated.Entries[i]
		if entry.Entry == nil {
			return nil, errors.New("missing allocated desktop entry")
		}
		d := entry.Entry
		visible, err := d.Visible(s.options.Desktops, s.options.Executable)
		if err != nil {
			return nil, fmt.Errorf("desktop %s: %w", entry.Path, err)
		}
		if !visible {
			continue
		}
		label, err := d.Localized("Desktop Entry", "Name", s.options.Locales)
		if err != nil {
			return nil, err
		}
		icon, err := LocalizedIcon(d, "Desktop Entry", s.options.Locales)
		if err != nil {
			return nil, err
		}
		argv, err := ExpandExec(d.Groups["Desktop Entry"]["Exec"], ExecContext{Name: label, Icon: icon, Location: entry.Path})
		if err != nil {
			return nil, fmt.Errorf("desktop %s: %w", entry.Path, err)
		}
		terminal, err := d.Boolean("Desktop Entry", "Terminal")
		if err != nil {
			return nil, err
		}
		directory, err := d.String("Desktop Entry", "Path")
		if err != nil {
			return nil, err
		}
		if directory != "" && (!pathValue(directory) || !filepath.IsAbs(directory)) {
			return nil, errors.New("desktop working directory must be absolute")
		}
		startup, err := ParseStartupMetadata(d, label, icon, entry.ID)
		if err != nil {
			return nil, err
		}
		primary := PresentedItem{Kind: ApplicationItem, Label: label, Icon: icon, Entry: entry, Argv: argv, Terminal: terminal, WorkingDir: directory, Startup: startup}
		if err = s.chargeApplication(primary, false); err != nil {
			return nil, err
		}
		items := []PresentedItem{primary}
		for _, action := range DeclaredActions(d, s.options.Locales, label, icon) {
			argv, err := ExpandExec(action.Exec, ExecContext{Name: label, Icon: action.Icon, Location: entry.Path})
			if err != nil {
				continue
			}
			startup, err := ParseStartupMetadata(d, action.Label, action.Icon, entry.ID)
			if err != nil {
				return nil, err
			}
			item := PresentedItem{Kind: ApplicationItem, Label: action.Label, Icon: action.Icon, Entry: entry,
				Argv: argv, Terminal: terminal, WorkingDir: directory, Startup: startup}
			if err = s.chargeApplication(item, true); err != nil {
				return nil, err
			}
			items = append(items, item)
		}
		// Layout reserves and places the original desktop ID once. Action rows
		// stay with their primary, in declared order, without synthetic IDs.
		files[entry.ID] = items
	}
	base := layoutOptions{limit: 4, header: true}
	if compiled.DefaultLayout != nil {
		base, err = parseLayoutOptions(compiled.DefaultLayout.Attr, base)
		if err != nil {
			return nil, err
		}
	}
	var layout *Node
	for _, node := range compiled.Definition.Children {
		if node.Tag == "Layout" {
			layout = node
		}
	}
	if layout == nil || len(layout.Children) == 0 {
		layout = compiled.DefaultLayout
	}
	if layout == nil || len(layout.Children) == 0 {
		layout = &Node{Tag: "Layout", Children: []*Node{{Tag: "Merge", Attr: map[string]string{"type": "menus"}}, {Tag: "Merge", Attr: map[string]string{"type": "files"}}}}
	}
	reservedFiles, reservedMenus := make(map[string]bool), make(map[string]bool)
	if err = s.charge(len(layout.Children), 0); err != nil {
		return nil, err
	}
	for _, node := range layout.Children {
		if node == nil {
			return nil, errors.New("nil layout node")
		}
		name := strings.Trim(node.Text, " \t\r\n")
		if node.Tag == "Filename" {
			reservedFiles[name] = true
		}
		if node.Tag == "Menuname" {
			reservedMenus[name] = true
		}
	}
	usedFiles, usedMenus := make(map[string]bool), make(map[string]bool)
	appendItem := func(item PresentedItem, options layoutOptions) error {
		if item.Kind == SubmenuItem {
			if len(item.Menu.Items) == 0 && !options.showEmpty {
				return nil
			}
			if options.inline && (options.limit == 0 || len(item.Menu.Items) <= options.limit) {
				if len(item.Menu.Items) == 1 && options.alias {
					copy := item.Menu.Items[0]
					copy.Label = item.Label
					copy.Icon = item.Icon
					if err := s.charge(1, len(copy.Label)+len(copy.Icon)); err != nil {
						return err
					}
					result.Items = append(result.Items, copy)
					return nil
				}
				if options.header && len(item.Menu.Items) != 0 {
					if err := s.charge(1, len(item.Label)+len(item.Icon)); err != nil {
						return err
					}
					result.Items = append(result.Items, PresentedItem{Kind: HeaderItem, Label: item.Label, Icon: item.Icon})
				}
				if err := s.charge(len(item.Menu.Items), 0); err != nil {
					return err
				}
				result.Items = append(result.Items, item.Menu.Items...)
				return nil
			}
		}
		if err := s.charge(1, 0); err != nil {
			return err
		}
		result.Items = append(result.Items, item)
		return nil
	}
	for _, node := range layout.Children {
		name := strings.Trim(node.Text, " \t\r\n")
		switch node.Tag {
		case "Filename":
			if items, ok := files[name]; ok && !usedFiles[name] {
				for _, item := range items {
					if err = appendItem(item, base); err != nil {
						return nil, err
					}
				}
				usedFiles[name] = true
			}
		case "Menuname":
			options, err := parseLayoutOptions(node.Attr, base)
			if err != nil {
				return nil, err
			}
			if item, ok := menus[name]; ok && !usedMenus[name] {
				if err = appendItem(item, options); err != nil {
					return nil, err
				}
				usedMenus[name] = true
			}
		case "Separator":
			if len(result.Items) != 0 && result.Items[len(result.Items)-1].Kind != SeparatorItem {
				if err = appendItem(PresentedItem{Kind: SeparatorItem}, base); err != nil {
					return nil, err
				}
			}
		case "Merge":
			kind := node.Attr["type"]
			if kind != "menus" && kind != "files" && kind != "all" {
				return nil, errors.New("unknown layout merge type")
			}
			type candidate struct {
				id   string
				item PresentedItem
			}
			var candidates []candidate
			if kind != "files" {
				for id, item := range menus {
					if !reservedMenus[id] && !usedMenus[id] {
						candidates = append(candidates, candidate{id, item})
					}
				}
			}
			if kind != "menus" {
				for id, items := range files {
					if !reservedFiles[id] && !usedFiles[id] {
						candidates = append(candidates, candidate{id, items[0]})
					}
				}
			}
			if err = s.charge(len(candidates), 0); err != nil {
				return nil, err
			}
			sort.Slice(candidates, func(i, j int) bool {
				a, b := candidates[i], candidates[j]
				if a.item.Label != b.item.Label {
					return a.item.Label < b.item.Label
				}
				if a.item.Kind != b.item.Kind {
					return a.item.Kind < b.item.Kind
				}
				return a.id < b.id
			})
			for _, candidate := range candidates {
				if candidate.item.Kind == SubmenuItem {
					if err = appendItem(candidate.item, base); err != nil {
						return nil, err
					}
					usedMenus[candidate.id] = true
				} else {
					for _, item := range files[candidate.id] {
						if err = appendItem(item, base); err != nil {
							return nil, err
						}
					}
					usedFiles[candidate.id] = true
				}
			}
		}
	}
	if len(result.Items) != 0 && result.Items[len(result.Items)-1].Kind == SeparatorItem {
		result.Items = result.Items[:len(result.Items)-1]
	}
	return result, nil
}

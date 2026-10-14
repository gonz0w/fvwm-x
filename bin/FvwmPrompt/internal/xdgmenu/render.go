// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"errors"
	"fmt"
	"io"
	"strings"
)

const MaxRenderBytes = 128 * 1024 * 1024
const MaxCommandBytes = 1024 * 1024

type RenderOptions struct {
	Title, InsertInto, IncludeItems, Regenerate, Configure string
	Titles, Dynamic, Icons                                 bool
	Icon                                                   func(name string, directory bool) (string, error)
	Action                                                 func(PresentedItem) (string, error)
}
type MenuDocument struct {
	Menu  *PresentedMenu
	Label string
}
type renderState struct {
	out          io.Writer
	options      RenderOptions
	bytes, items int
	names        map[string]bool
	nameBytes    int
	serial       int
}

// Preserve ordinary theme-visible menu names. Disambiguate only collisions,
// including document roots and captions that contain path separators.
func (s *renderState) menuName(preferred string) (string, error) {
	name := preferred
	for s.names[name] {
		s.serial++
		if s.serial > MaxItems {
			return "", errors.New("menu name work limit exceeded")
		}
		name = fmt.Sprintf("%s#xdg-%d", preferred, s.serial)
	}
	if !commandValue(name) || len(s.names) >= MaxItems || len(name) > s.nameBytes {
		return "", errors.New("menu name storage limit exceeded")
	}
	s.names[name] = true
	s.nameBytes -= len(name)
	return name, nil
}

// FVWMQuote protects one data token from configuration expansion. Action
// callbacks must encode their own execution data; data never becomes a command
// merely because it appeared in a caption or menu name.
func FVWMQuote(value string) string {
	return `"` + strings.NewReplacer(`\`, `\\`, `"`, `\"`, "$", "$$").Replace(value) + `"`
}

func menuCaption(value string) string {
	value = strings.NewReplacer("&", "&&", "%", "%%", "*", "**", "^", "^^", "@", "@@", "\n", " ", "\r", " ", "\t", " ").Replace(value)
	return strings.NewReplacer(`\`, `\\`, `"`, `\"`, "$", "$$").Replace(value)
}

func commandValue(value string) bool { return pathValue(value) && !strings.ContainsAny(value, "\r\n") }

func (s *renderState) line(format string, args ...any) error {
	line := fmt.Sprintf(format, args...)
	if len(line) > MaxCommandBytes || len(line) > s.bytes || s.items <= 0 {
		return errors.New("generated menu exceeds command/output limits")
	}
	s.bytes -= len(line)
	s.items--
	_, err := io.WriteString(s.out, line)
	return err
}

func (s *renderState) item(label, icon, action string, directory bool) error {
	if !pathValue(label) || !pathValue(icon) || !commandValue(action) {
		return errors.New("invalid menu item data/action")
	}
	image := ""
	if s.options.Icons && s.options.Icon != nil {
		resolved, err := s.options.Icon(icon, directory)
		if err != nil {
			return err
		}
		if resolved != "" {
			if !commandValue(resolved) || strings.ContainsAny(resolved, "%*") {
				return errors.New("icon path cannot be represented in a menu")
			}
			image = "%" + strings.NewReplacer(`\`, `\\`, `"`, `\"`, "$", "$$").Replace(resolved) + "%"
		}
	}
	return s.line("+ \"%s%s\" %s\n", menuCaption(label), image, action)
}

func (s *renderState) extra() error {
	if s.options.IncludeItems == "none" {
		return nil
	}
	if err := s.line("+ \"\" Nop\n"); err != nil {
		return err
	}
	if s.options.IncludeItems == "both" || s.options.IncludeItems == "regenerate" {
		if err := s.line("+ \"$[gt.Regenerate]\" %s\n", s.options.Regenerate); err != nil {
			return err
		}
	}
	if s.options.IncludeItems == "both" || s.options.IncludeItems == "config" {
		return s.line("+ \"$[gt.Configure]\" %s\n", s.options.Configure)
	}
	return nil
}

func (s *renderState) menu(menu *PresentedMenu, name, label, path string, top bool, depth int) error {
	if menu == nil || depth >= MaxDepth || !commandValue(name) || !commandValue(path) {
		return errors.New("invalid rendered menu tree")
	}
	if !(top && s.options.InsertInto != "") {
		mode := ""
		if top && s.options.Dynamic {
			mode = " recreate"
		}
		if err := s.line("DestroyMenu%s %s\n", mode, FVWMQuote(name)); err != nil {
			return err
		}
	}
	if s.options.Titles && !(top && s.options.InsertInto != "") {
		if err := s.line("AddToMenu %s \"%s\" Title\n", FVWMQuote(name), menuCaption(label)); err != nil {
			return err
		}
	} else {
		if err := s.line("AddToMenu %s\n", FVWMQuote(name)); err != nil {
			return err
		}
	}
	children := make(map[int]string)
	for index, item := range menu.Items {
		switch item.Kind {
		case ApplicationItem:
			if s.options.Action == nil {
				return errors.New("application action encoder is unavailable")
			}
			action, err := s.options.Action(item)
			if err != nil {
				return err
			}
			if err = s.item(item.Label, item.Icon, action, false); err != nil {
				return err
			}
		case SubmenuItem:
			if item.Menu == nil {
				return errors.New("nil rendered submenu")
			}
			childPath, err := s.menuName(submenuPath(path, item.Label))
			if err != nil {
				return err
			}
			children[index] = childPath
			if err := s.item(item.Label, item.Icon, "Popup "+FVWMQuote(childPath), true); err != nil {
				return err
			}
		case SeparatorItem:
			if err := s.line("+ \"\" Nop\n"); err != nil {
				return err
			}
		case HeaderItem:
			if err := s.item(item.Label, item.Icon, "Title", true); err != nil {
				return err
			}
		default:
			return errors.New("unknown rendered item kind")
		}
	}
	if top {
		if err := s.extra(); err != nil {
			return err
		}
	}
	if err := s.line("\n"); err != nil {
		return err
	}
	for index, item := range menu.Items {
		if item.Kind == SubmenuItem {
			childPath := children[index]
			if err := s.menu(item.Menu, childPath, item.Menu.Label, childPath, false, depth+1); err != nil {
				return err
			}
		}
	}
	return nil
}

func submenuPath(parent, label string) string {
	if parent == "" {
		return label
	}
	return parent + "/" + label
}

// RenderMenus writes deterministic native commands. Callers buffer or publish
// the complete successful result before exposing it to the core. Partial output
// on an encoder/writer error is owned by the caller and must not be executed.
func RenderMenus(out io.Writer, documents []MenuDocument, options RenderOptions) error {
	if out == nil || len(documents) > MaxItems || !commandValue(options.Title) || options.Title == "" || !commandValue(options.InsertInto) || !commandValue(options.Regenerate) || !commandValue(options.Configure) {
		return errors.New("invalid menu renderer options")
	}
	if options.IncludeItems != "none" && options.IncludeItems != "both" && options.IncludeItems != "config" && options.IncludeItems != "regenerate" {
		return errors.New("invalid additional menu items")
	}
	s := &renderState{out: out, options: options, bytes: MaxRenderBytes, items: MaxItems, names: make(map[string]bool), nameBytes: MaxRenderBytes}
	top := options.Title
	if options.InsertInto != "" {
		top = options.InsertInto
	}
	if _, err := s.menuName(top); err != nil {
		return err
	}
	if len(documents) == 1 {
		menu := documents[0].Menu
		if menu == nil {
			menu = &PresentedMenu{Name: "Applications"}
		}
		return s.menu(menu, top, top, "", true, 0)
	}
	var items []PresentedItem
	roots := make(map[int]string)
	for index, document := range documents {
		if document.Menu == nil || len(document.Menu.Items) == 0 {
			continue
		}
		name, err := s.menuName("Fvwm" + document.Label)
		if err != nil {
			return err
		}
		roots[index] = name
	}
	for index, document := range documents {
		if document.Menu == nil || len(document.Menu.Items) == 0 {
			continue
		}
		name := roots[index]
		if err := s.menu(document.Menu, name, document.Label, "", false, 0); err != nil {
			return err
		}
		items = append(items, PresentedItem{Kind: SubmenuItem, Label: document.Label, Menu: &PresentedMenu{Name: name}})
	}
	root := &PresentedMenu{Name: "", Items: items}
	// The combined menu references already-rendered document roots rather than
	// recursing through their application menus a second time.
	mode := ""
	if options.Dynamic {
		mode = " recreate"
	}
	if options.InsertInto == "" {
		if err := s.line("DestroyMenu%s %s\n", mode, FVWMQuote(top)); err != nil {
			return err
		}
	}
	if options.Titles && options.InsertInto == "" {
		if err := s.line("AddToMenu %s \"%s\" Title\n", FVWMQuote(top), menuCaption(top)); err != nil {
			return err
		}
	} else {
		if err := s.line("AddToMenu %s\n", FVWMQuote(top)); err != nil {
			return err
		}
	}
	for _, item := range root.Items {
		if err := s.item(item.Label, "", "Popup "+FVWMQuote(item.Menu.Name), true); err != nil {
			return err
		}
	}
	if err := s.extra(); err != nil {
		return err
	}
	return s.line("\n")
}

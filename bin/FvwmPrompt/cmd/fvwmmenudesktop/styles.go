// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"errors"
	"fmt"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

func iconStyles(out *bytes.Buffer, documents []xdgmenu.MenuDocument, icons *iconRenderer) error {
	seen := make(map[string]bool)
	remaining := xdgmenu.MaxItems
	var visit func(*xdgmenu.PresentedMenu, int) error
	visit = func(menu *xdgmenu.PresentedMenu, depth int) error {
		if menu == nil {
			return nil
		}
		if depth >= xdgmenu.MaxDepth {
			return errors.New("style menu nesting exceeds bound")
		}
		for _, item := range menu.Items {
			remaining--
			if remaining < 0 {
				return errors.New("too many menu icon styles")
			}
			if item.Kind == xdgmenu.SubmenuItem {
				if err := visit(item.Menu, depth+1); err != nil {
					return err
				}
			}
			if item.Kind != xdgmenu.ApplicationItem || item.Entry == nil || item.Icon == "" {
				continue
			}
			class, err := item.Entry.Entry.String("Desktop Entry", "StartupWMClass")
			if err != nil {
				return err
			}
			if class == "" {
				class = item.Icon
			}
			if seen[class] || strings.ContainsAny(class, "\x00\r\n") {
				continue
			}
			seen[class] = true
			icon, err := icons.resolve(item.Icon, false)
			if err != nil {
				return err
			}
			if icon == "" {
				continue
			}
			line := fmt.Sprintf("Style %s MiniIcon %s\n", xdgmenu.FVWMQuote(class), xdgmenu.FVWMQuote(icon))
			if len(line) > xdgmenu.MaxCommandBytes || len(line) > xdgmenu.MaxRenderBytes-out.Len() {
				return errors.New("icon styles exceed output limit")
			}
			out.WriteString(line)
		}
		return nil
	}
	for _, document := range documents {
		if err := visit(document.Menu, 0); err != nil {
			return err
		}
	}
	return nil
}

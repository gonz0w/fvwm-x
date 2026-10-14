// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"io"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

// Failure output is constructed separately after incomplete worker output was
// discarded. Diagnostic filenames/metadata never become configuration syntax.
func failureMenu(args []string, out io.Writer) {
	defaults, _ := xdgmenu.DefaultsFromPreferences(nil)
	o, err := parseOptions(args, defaults)
	if err != nil || !o.menuError {
		return
	}
	menu := &xdgmenu.PresentedMenu{Name: "Error", Label: "Error", Items: []xdgmenu.PresentedItem{{Kind: xdgmenu.HeaderItem, Label: "Desktop menu generation failed; see terminal diagnostics."}}}
	var complete bytes.Buffer
	err = xdgmenu.RenderMenus(&complete, []xdgmenu.MenuDocument{{Menu: menu, Label: "Error"}}, xdgmenu.RenderOptions{Title: o.Title, Titles: o.Titles, IncludeItems: "none"})
	if err == nil {
		_, _ = out.Write(complete.Bytes())
	}
}

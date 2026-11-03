// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
package xdgsource

import (
	"io/fs"
	"os"
	"path/filepath"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

func TestOptionalLucideTheme(t *testing.T) {
	// Copy the actual shipped theme into a nonstandard XDG data prefix, rather
	// than synthesizing a second index that could hide packaging mistakes.
	origin := filepath.Join("..", "..", "..", "..", "default-config", "icons", "FVWM-X-Lucide")
	prefix := filepath.Join(t.TempDir(), "custom", "share")
	theme := filepath.Join(prefix, "icons", "FVWM-X-Lucide")
	err := filepath.WalkDir(origin, func(path string, entry fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		relative, err := filepath.Rel(origin, path)
		if err != nil {
			return err
		}
		destination := filepath.Join(theme, relative)
		if entry.IsDir() {
			return os.MkdirAll(destination, 0700)
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		return os.WriteFile(destination, data, 0600)
	})
	if err != nil {
		t.Fatal(err)
	}
	names := []string{"folder", "folder-open", "file", "file-text", "image", "clipboard", "copy", "scissors", "clipboard-paste", "search", "refresh-cw", "arrow-left", "arrow-right", "arrow-up", "settings", "trash"}
	finder := NewIconFinder(New(nil), t.TempDir(), xdgmenu.SearchPaths{Data: []string{prefix}}, "", 24)
	for quarters := 4; quarters <= 16; quarters++ {
		for _, state := range []string{"normal", "selected", "disabled"} {
			for _, name := range names {
				request := xdgmenu.IconRequest{Name: name, Theme: "FVWM-X-Lucide", Size: 24, ScaleQuarters: quarters, State: state}
				got, err := finder.Resolve(request)
				want := filepath.Join(theme, "scalable", "actions", name+".svg")
				if err != nil || got != want {
					t.Fatalf("%+v: %q %v, want %q", request, got, err, want)
				}
			}
		}
	}
	// Exclude system fallback artwork when asserting the private theme has no
	// enabled format. A system folder.png would be a valid normal fallback.
	finder.Roots = []string{filepath.Join(prefix, "icons")}
	finder.Extensions = []string{"png", "xpm"}
	request := xdgmenu.IconRequest{Name: "folder", Theme: "FVWM-X-Lucide", Size: 24, ScaleQuarters: 4}
	if got, err := finder.Resolve(request); err != nil || got != "" {
		t.Fatal("disabled SVG lookup", got, err)
	}
	finder.Roots = []string{filepath.Join(t.TempDir(), "absent")}
	finder.Extensions = []string{"svg"}
	absent := finder.Roots
	finder = NewIconFinder(New(nil), t.TempDir(), xdgmenu.SearchPaths{}, "", 24)
	finder.Roots = absent
	finder.Extensions = []string{"svg"}
	if got, err := finder.Resolve(request); err != nil || got != "" {
		t.Fatal("uninstalled theme", got, err)
	}
}

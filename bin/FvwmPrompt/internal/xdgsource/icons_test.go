// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"os"
	"path/filepath"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

func TestIconThemeSearch(t *testing.T) {
	root := t.TempDir()
	write := func(relative, text string) string {
		path := filepath.Join(root, relative)
		if err := os.MkdirAll(filepath.Dir(path), 0700); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(path, []byte(text), 0600); err != nil {
			t.Fatal(err)
		}
		return path
	}
	write("child/index.theme", "[Icon Theme]\nInherits=parent\nDirectories=small\n[small]\nSize=16\nType=Fixed\n")
	write("parent/index.theme", "[Icon Theme]\nInherits=child\nDirectories=exact\n[exact]\nSize=24\nType=Fixed\n")
	write("hicolor/index.theme", "[Icon Theme]\nDirectories=exact\n[exact]\nSize=24\nType=Fixed\n")
	child := write("child/small/app.png", "fixture")
	write("parent/exact/app.png", "fixture")
	parent := write("parent/exact/parent.svg", "fixture")
	standard := write("hicolor/exact/standard.xpm", "fixture")
	plain := write("unthemed.png", "fixture")
	finder := NewIconFinder(New(nil), root, xdgmenu.SearchPaths{}, "child", 24)
	finder.Roots = []string{root}
	for _, test := range []struct{ name, want string }{{"app", child}, {"parent", parent}, {"standard", standard}, {"unthemed", plain}, {"missing", ""}, {child, child}} {
		got, err := finder.Find(test.name)
		if err != nil || got != test.want {
			t.Fatal(test, got, err)
		}
	}
	finder.Extensions = []string{"xpm"}
	if got, err := finder.Find(parent); err != nil || got != "" {
		t.Fatal("disabled SVG path", got, err)
	}
	if got, err := finder.Find("../app"); err == nil || got != "" {
		t.Fatal("accepted traversal", got, err)
	}
}

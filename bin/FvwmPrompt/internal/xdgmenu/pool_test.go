// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"strings"
	"testing"
)

func TestDesktopFileIDs(t *testing.T) {
	for relative, want := range map[string]string{"a.desktop": "a.desktop", "bo/oz/Hello.desktop": "bo-oz-Hello.desktop", "Café Tool.desktop": "Café Tool.desktop", ".hidden/a.desktop": ".hidden-a.desktop"} {
		t.Run(relative, func(t *testing.T) {
			if got, err := DesktopID(relative); err != nil || got != want {
				t.Fatal(got, err)
			}
		})
	}
	for name, relative := range map[string]string{"empty": "", "absolute": "/a.desktop", "escape": "../a.desktop", "escape-nested": "a/../b.desktop", "double-slash": "a//b.desktop", "dot": "./a.desktop", "wrong-extension": "a.txt", "case": "a.DESKTOP", "nul": "a\x00.desktop", "invalid-utf8": "\xff.desktop", "long": strings.Repeat("a", MaxValueBytes) + ".desktop"} {
		t.Run(name, func(t *testing.T) {
			if got, err := DesktopID(relative); err == nil || got != "" {
				t.Fatal(got, err)
			}
		})
	}
}

func TestApplicationPoolPrecedence(t *testing.T) {
	base := parsedDesktop(t, "Name=System\n")
	hidden := parsedDesktop(t, "Name=User\nHidden=true\n")
	cases := []struct {
		name string
		dirs []AppDirectory
		want map[string]PoolEntry
	}{
		{"empty", nil, map[string]PoolEntry{}},
		{"last-appdir", []AppDirectory{{"/system", []DesktopFile{{"a.desktop", base}, {"sub/b.desktop", base}}}, {"/user", []DesktopFile{{"a.desktop", hidden}}}}, map[string]PoolEntry{"a.desktop": {"/user/a.desktop", hidden}, "sub-b.desktop": {"/system/sub/b.desktop", base}}},
		{"duplicate-directory-last", []AppDirectory{{"/same/", []DesktopFile{{"a.desktop", base}}}, {"/other", []DesktopFile{{"a.desktop", hidden}}}, {"/same", []DesktopFile{{"b.desktop", base}}}}, map[string]PoolEntry{"a.desktop": {"/other/a.desktop", hidden}, "b.desktop": {"/same/b.desktop", base}}},
		{"flattened-collision", []AppDirectory{{"/apps", []DesktopFile{{"foo/bar.desktop", hidden}, {"foo-bar.desktop", base}}}}, map[string]PoolEntry{"foo-bar.desktop": {"/apps/foo-bar.desktop", base}}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			original := make([][]DesktopFile, len(c.dirs))
			for i, dir := range c.dirs {
				original[i] = append([]DesktopFile(nil), dir.Entries...)
			}
			got, err := BuildPool(c.dirs)
			if err != nil || !reflect.DeepEqual(got, c.want) {
				t.Fatal(got, err)
			}
			for i, dir := range c.dirs {
				if !reflect.DeepEqual(dir.Entries, original[i]) {
					t.Fatal("mutated application inventory")
				}
			}
		})
	}
	pool, err := BuildPool([]AppDirectory{{"/system", []DesktopFile{{"a.desktop", base}}}, {"/user", []DesktopFile{{"a.desktop", hidden}}}})
	if err != nil {
		t.Fatal(err)
	}
	if visible, err := pool["a.desktop"].Entry.Visible([]string{"FVWM"}, nil); err != nil || visible {
		t.Fatal("hidden override revealed system application", visible, err)
	}
}

func TestApplicationPoolLimits(t *testing.T) {
	entry := parsedDesktop(t, "Name=Fixture\n")
	entries := make([]DesktopFile, 200)
	for i := range entries {
		entries[i] = DesktopFile{Relative: strings.Repeat("a", i+1) + ".desktop", Entry: entry}
	}
	cases := map[string][]AppDirectory{
		"relative-directory": {{"relative", nil}},
		"nul-directory":      {{"/bad\x00", nil}},
		"nil-entry":          {{"/apps", []DesktopFile{{"a.desktop", nil}}}},
		"escape":             {{"/apps", []DesktopFile{{"../a.desktop", entry}}}},
		"many-directories":   make([]AppDirectory, MaxItems+1),
		"many-entries":       {{"/apps", make([]DesktopFile, MaxItems+1)}},
		"aggregate-entries":  {{"/first", make([]DesktopFile, MaxItems)}, {"/second", []DesktopFile{{"a.desktop", entry}}}},
		"combined-path":      {{"/" + strings.Repeat("a", MaxValueBytes-2), []DesktopFile{{"a.desktop", entry}}}},
		"combined-output":    {{"/" + strings.Repeat("a", 50000), entries}},
	}
	for name, dirs := range cases {
		t.Run(name, func(t *testing.T) {
			if got, err := BuildPool(dirs); err == nil || got != nil {
				t.Fatal("accepted invalid pool", len(got), err)
			}
		})
	}
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import "testing"

func TestIconThemePolicy(t *testing.T) {
	for _, name := range []string{"Thème", "Theme With Spaces"} {
		if !ValidIconTheme(name) {
			t.Fatal("refused literal theme directory name", name)
		}
	}
	data := []byte("[Icon Theme]\nInherits= parent , hicolor\nDirectories=fixed,scalable,threshold\n[fixed]\nSize=24\nType=Fixed\n[scalable]\nSize=32\nType=Scalable\nMinSize=16\nMaxSize=64\nScale=2\n[threshold]\nSize=24\nThreshold=2\n")
	theme, err := ParseIconTheme(data)
	if err != nil {
		t.Fatal(err)
	}
	if len(theme.Directories) != 3 || len(theme.Inherits) != 2 {
		t.Fatal(theme)
	}
	for _, test := range []struct {
		i                 int
		size, scale, want int64
	}{{0, 24, 1, 0}, {0, 20, 1, 4}, {1, 24, 2, 0}, {1, 16, 1, 16}, {2, 26, 1, 0}, {2, 28, 1, 2}} {
		got, err := theme.Directories[test.i].Distance(test.size, test.scale)
		if err != nil || got != test.want {
			t.Fatal(test, got, err)
		}
	}
	for _, bad := range []string{"[Icon Theme]\nDirectories=../outside\n", "[Icon Theme]\nDirectories=missing\n", "[Icon Theme]\nInherits=../parent\n", "[Icon Theme]\nDirectories=a\n[a]\nSize=999999999\n", "[Icon Theme]\nDirectories=a\n[a]\nSize=24\nScale=0\n"} {
		if _, err := ParseIconTheme([]byte(bad)); err == nil {
			t.Fatal(bad)
		}
	}
	if _, err := (IconDirectory{Type: "Scalable", Size: 24, Scale: 1, Minimum: 0, Maximum: 24}).Distance(24, 1); err == nil {
		t.Fatal("accepted zero scalable endpoint")
	}
}

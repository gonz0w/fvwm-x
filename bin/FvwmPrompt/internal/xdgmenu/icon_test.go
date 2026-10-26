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

func TestIconRequestGeometryAndScale(t *testing.T) {
	for _, test := range []struct {
		text string
		want int
	}{
		{"1", 4}, {"1.0", 4}, {"1.00", 4}, {"1.25", 5}, {"1.5", 6},
		{"1.50", 6}, {"1.75", 7}, {"2", 8}, {"3.75", 15}, {"4.00", 16},
	} {
		got, err := ParseIconScale(test.text)
		if err != nil || got != test.want {
			t.Fatal(test, got, err)
		}
	}
	for _, text := range []string{"", "0", "4.25", "1.1", "1.250", "1.", ".25", "+1", "01", "1e0", " 1", "NaN", "999999999999999999999"} {
		if _, err := ParseIconScale(text); err == nil {
			t.Fatal("accepted scale", text)
		}
	}
	request := IconRequest{Name: "folder", Size: 17, ScaleQuarters: 5}
	if got, err := request.PhysicalSize(); err != nil || got != 22 {
		t.Fatal(got, err)
	}
	for _, state := range []string{"", "normal", "selected", "disabled"} {
		request.State = state
		if _, err := request.PhysicalSize(); err != nil {
			t.Fatal(state, err)
		}
	}
	bad := []IconRequest{
		{Name: "folder", Size: 0, ScaleQuarters: 4},
		{Name: "folder", Size: 4097, ScaleQuarters: 4},
		{Name: "folder", Size: 1025, ScaleQuarters: 16},
		{Name: "folder", Size: int(^uint(0) >> 1), ScaleQuarters: 16},
		{Name: "folder", Size: 16, ScaleQuarters: 3},
		{Name: "folder", Size: 16, ScaleQuarters: 17},
		{Name: "folder", Size: 16, ScaleQuarters: int(^uint(0) >> 1)},
		{Name: "folder", Size: 16, ScaleQuarters: 4, State: "hover"},
		{Name: "folder", Size: 16, ScaleQuarters: 4, Theme: "../theme"},
		{Name: "../folder", Size: 16, ScaleQuarters: 4},
		{Name: `folder\child`, Size: 16, ScaleQuarters: 4},
		{Name: "folder", Theme: `theme\child`, Size: 16, ScaleQuarters: 4},
		{Name: "folder", Size: 16, ScaleQuarters: 4, Override: "bad\nname"},
		{Name: "folder", Size: 16, ScaleQuarters: 4, Fallback: string([]byte{0xff})},
	}
	for _, request := range bad {
		if _, err := request.PhysicalSize(); err == nil {
			t.Fatal("accepted request", request)
		}
	}
	if got, err := (IconRequest{Size: 1024, ScaleQuarters: 16}).PhysicalSize(); err != nil || got != 4096 {
		t.Fatal(got, err)
	}
}

func TestIconQuarterDirectoryPolicy(t *testing.T) {
	one := IconDirectory{Type: "Fixed", Size: 48, Scale: 1}
	two := IconDirectory{Type: "Fixed", Size: 24, Scale: 2}
	for _, directory := range []IconDirectory{one, two} {
		if got, err := directory.DistanceQuarters(24, 8); err != nil || got != 0 {
			t.Fatal(directory, got, err)
		}
	}
	if exact, err := one.MatchQuarters(24, 8); err != nil || exact {
		t.Fatal("same physical extent is not nominal match", exact, err)
	}
	if exact, err := two.MatchQuarters(24, 8); err != nil || !exact {
		t.Fatal("missed nominal scale match", exact, err)
	}
	if got, err := one.DistanceQuarters(17, 5); err != nil || got != 107 {
		t.Fatal("fractional numerator", got, err)
	}
	if _, err := (IconDirectory{Type: "Fixed", Size: int64(^uint64(0) >> 1), Scale: 1}).DistanceQuarters(24, 8); err == nil {
		t.Fatal("accepted overflowing directory")
	}
}

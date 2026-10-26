// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"context"
	"errors"
	"fmt"
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
	for _, test := range []struct{ name, want string }{{"parent", parent}, {"standard", standard}, {"unthemed", plain}, {"missing", ""}} {
		request := xdgmenu.IconRequest{Name: test.name, Theme: "child", Size: 24, ScaleQuarters: 5}
		if got, err := finder.Resolve(request); err != nil || got != test.want {
			t.Fatal("typed inheritance/cycle/hicolor/root fallback", test, got, err)
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

func writeIconFixture(t *testing.T, root, relative, data string) string {
	t.Helper()
	filename := filepath.Join(root, relative)
	if err := os.MkdirAll(filepath.Dir(filename), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filename, []byte(data), 0600); err != nil {
		t.Fatal(err)
	}
	return filename
}

func TestIconRequestScaleAndIsolation(t *testing.T) {
	root := t.TempDir()
	writeIconFixture(t, root, "theme/index.theme", "[Icon Theme]\nDirectories=physical,nominal,fractional\n[physical]\nSize=48\nType=Fixed\n[nominal]\nSize=24\nScale=2\nType=Fixed\n[fractional]\nSize=22\nType=Fixed\n")
	physical := writeIconFixture(t, root, "theme/physical/app.png", "1x")
	nominal := writeIconFixture(t, root, "theme/nominal/app.png", "2x")
	fractional := writeIconFixture(t, root, "theme/fractional/app.png", "quarter")
	override := writeIconFixture(t, root, "theme/nominal/choice.png", "selected")
	writeIconFixture(t, root, "theme/nominal/app-selected.png", "not a state convention")
	finder := NewIconFinder(New(nil), root, xdgmenu.SearchPaths{}, "unchanged", 13)
	finder.Roots = []string{root}
	for _, test := range []struct {
		request xdgmenu.IconRequest
		want    string
	}{
		{xdgmenu.IconRequest{Name: "app", Theme: "theme", Size: 24, ScaleQuarters: 8}, nominal},
		{xdgmenu.IconRequest{Name: "app", Theme: "theme", Size: 48, ScaleQuarters: 4}, physical},
		{xdgmenu.IconRequest{Name: "app", Theme: "theme", Size: 17, ScaleQuarters: 5}, fractional},
		{xdgmenu.IconRequest{Name: "app", Theme: "theme", Size: 24, ScaleQuarters: 8, State: "selected", Override: "choice"}, override},
		{xdgmenu.IconRequest{Name: "app", Theme: "theme", Size: 24, ScaleQuarters: 8, State: "disabled", Override: "missing"}, nominal},
		{xdgmenu.IconRequest{Name: "app", Theme: "theme", Size: 24, ScaleQuarters: 8, State: "selected"}, nominal},
		{xdgmenu.IconRequest{Name: "missing", Theme: "theme", Size: 24, ScaleQuarters: 8, Fallback: "app"}, nominal},
	} {
		got, err := finder.Resolve(test.request)
		if err != nil || got != test.want {
			t.Fatal(test, got, err)
		}
		if finder.Size != 13 || finder.Theme != "unchanged" {
			t.Fatal("request mutated defaults", finder)
		}
	}
}

func TestIconRequestBoundsAndCancellation(t *testing.T) {
	root := t.TempDir()
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	finder := NewIconFinder(New(ctx), root, xdgmenu.SearchPaths{}, "", 24)
	request := xdgmenu.IconRequest{Name: "app", Size: 24, ScaleQuarters: 4}
	if _, err := finder.Resolve(request); !errors.Is(err, context.Canceled) {
		t.Fatal("ignored deadline", err)
	}
	finder = NewIconFinder(New(nil), root, xdgmenu.SearchPaths{}, "", 24)
	finder.remaining = 0
	if _, err := finder.Resolve(request); err == nil {
		t.Fatal("ignored work bound")
	}
	finder = NewIconFinder(New(nil), root, xdgmenu.SearchPaths{}, "", 24)
	finder.Extensions = nil
	request.Theme = "unread"
	if got, err := finder.Resolve(request); err != nil || got != "" || len(finder.themes) != 0 {
		t.Fatal("disabled formats did lookup", got, err)
	}
	request.Size = 4097
	if _, err := finder.Resolve(request); err == nil {
		t.Fatal("disabled formats accepted invalid request")
	}
	filename := writeIconFixture(t, root, "large.png", "")
	if err := os.Truncate(filename, MaxJobBytes+1); err != nil {
		t.Fatal(err)
	}
	finder.Extensions = []string{"png"}
	request = xdgmenu.IconRequest{Name: filename, Size: 24, ScaleQuarters: 4}
	if got, err := finder.Resolve(request); err != nil || got != "" {
		t.Fatal("accepted oversized input", got, err)
	}
}

func TestIconInheritanceDepthAndFreshGeneration(t *testing.T) {
	root := t.TempDir()
	for i := 0; i < xdgmenu.MaxDepth; i++ {
		writeIconFixture(t, root, fmt.Sprintf("theme%d/index.theme", i), fmt.Sprintf("[Icon Theme]\nInherits=theme%d\n", i+1))
	}
	finder := NewIconFinder(New(nil), root, xdgmenu.SearchPaths{}, "", 24)
	finder.Roots = []string{root}
	request := xdgmenu.IconRequest{Name: "app", Theme: "theme0", Size: 24, ScaleQuarters: 4}
	if _, err := finder.Resolve(request); err == nil {
		t.Fatal("ignored inheritance depth")
	}
	writeIconFixture(t, root, "fresh/index.theme", "[Icon Theme]\nDirectories=small\n[small]\nSize=24\nType=Fixed\n")
	old := writeIconFixture(t, root, "fresh/small/app.png", "old")
	request.Theme = "fresh"
	finder = NewIconFinder(New(nil), root, xdgmenu.SearchPaths{}, "", 24)
	finder.Roots = []string{root}
	if got, err := finder.Resolve(request); err != nil || got != old {
		t.Fatal(got, err)
	}
	writeIconFixture(t, root, "fresh/index.theme", "[Icon Theme]\nDirectories=new\n[new]\nSize=24\nType=Fixed\n")
	fresh := writeIconFixture(t, root, "fresh/new/app.png", "new")
	// The existing job retains its snapshot. A replacement Source/finder owns
	// the next generation rather than mutating cached theme metadata in place.
	if got, err := finder.Resolve(request); err != nil || got != old {
		t.Fatal(got, err)
	}
	finder = NewIconFinder(New(nil), root, xdgmenu.SearchPaths{}, "", 24)
	finder.Roots = []string{root}
	if got, err := finder.Resolve(request); err != nil || got != fresh {
		t.Fatal(got, err)
	}
}

func TestIconRequestAbsoluteBackslashPaths(t *testing.T) {
	root := t.TempDir()
	filename := writeIconFixture(t, root, `literal\name.png`, "fixture")
	finder := NewIconFinder(New(nil), root, xdgmenu.SearchPaths{}, "", 24)
	finder.Roots = []string{root}
	if got, err := finder.Find(filename); err != nil || got != filename {
		t.Fatal("legacy absolute path", got, err)
	}
	for _, request := range []xdgmenu.IconRequest{
		{Name: filename, Size: 24, ScaleQuarters: 4},
		{Name: "missing", Fallback: filename, Size: 24, ScaleQuarters: 4},
		{Name: "missing", Override: filename, State: "selected", Size: 24, ScaleQuarters: 4},
	} {
		if got, err := finder.Resolve(request); err != nil || got != filename {
			t.Fatal("absolute Unix backslash filename", request, got, err)
		}
	}
}

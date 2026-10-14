// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"context"
	"errors"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"syscall"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

func writeFixture(t *testing.T, path, text string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, []byte(text), 0600); err != nil {
		t.Fatal(err)
	}
}
func application(t *testing.T, path, name string) {
	t.Helper()
	writeFixture(t, path, "[Desktop Entry]\nType=Application\nName="+name+"\nExec=fixture-app\n")
}
func TestFilesystemMenuPipeline(t *testing.T) {
	dir := t.TempDir()
	apps, override := filepath.Join(dir, "apps"), filepath.Join(dir, "override")
	application(t, filepath.Join(apps, "a.desktop"), "Alpha")
	application(t, filepath.Join(apps, "nested", "b.desktop"), "Beta")
	writeFixture(t, filepath.Join(override, "a.desktop"), "[Desktop Entry]\nType=Application\nName=Hidden override\nHidden=true\n")
	writeFixture(t, filepath.Join(dir, "labels", "tools.directory"), "[Desktop Entry]\nType=Directory\nName=Tools\nName[fr]=Outils\n")
	rootPath := filepath.Join(dir, "menus", "applications.menu")
	writeFixture(t, rootPath, `<Menu><Name>Applications</Name><AppDir>../apps</AppDir><DirectoryDir>../labels</DirectoryDir><MergeFile>fragment.menu</MergeFile><Include><Filename>a.desktop</Filename></Include></Menu>`)
	writeFixture(t, filepath.Join(dir, "menus", "fragment.menu"), `<Menu><Name>Ignored</Name><Menu><Name>Tools</Name><Directory>tools.directory</Directory><AppDir>../override</AppDir><Include><All/></Include></Menu></Menu>`)
	source := New(context.Background())
	root, err := xdgmenu.MergeMenus(rootPath, xdgmenu.SearchPaths{}, source)
	if err != nil {
		t.Fatal(err)
	}
	pools, err := source.Prepare(root)
	if err != nil {
		t.Fatal(err)
	}
	compiled, err := xdgmenu.BuildMenuPlans(root, pools)
	if err != nil {
		t.Fatal(err)
	}
	allocated, err := xdgmenu.AllocateMenus(compiled.Plan)
	if err != nil {
		t.Fatal(err)
	}
	presented, err := xdgmenu.PresentMenus(compiled, allocated, xdgmenu.PresentationOptions{Directories: source.Directories, Locales: []string{"fr"}})
	if err != nil {
		t.Fatal(err)
	}
	if len(presented.Items) != 2 || presented.Items[0].Label != "Outils" || presented.Items[1].Label != "Alpha" || len(presented.Items[0].Menu.Items) != 1 || presented.Items[0].Menu.Items[0].Label != "Beta" {
		t.Fatal(presented)
	}
	if pools[xdgmenu.AppLocation{Path: apps}]["nested-b.desktop"].Path != filepath.Join(apps, "nested", "b.desktop") {
		t.Fatal(pools)
	}
	if len(source.files) != 6 {
		t.Fatal("unexpected acquired inputs", len(source.files))
	}
}

func TestFilesystemAcquisition(t *testing.T) {
	t.Run("menu-inventory", func(t *testing.T) {
		dir := t.TempDir()
		for _, name := range []string{"z.menu", "a.menu", "ignore.txt"} {
			writeFixture(t, filepath.Join(dir, name), "data")
		}
		if err := os.Mkdir(filepath.Join(dir, "directory.menu"), 0700); err != nil {
			t.Fatal(err)
		}
		got, err := New(nil).MenuFiles(dir)
		if err != nil || !reflect.DeepEqual(got, []string{"a.menu", "z.menu"}) {
			t.Fatal(got, err)
		}
	})
	t.Run("cached-input-identity", func(t *testing.T) {
		path := filepath.Join(t.TempDir(), "a.menu")
		writeFixture(t, path, "first")
		s := New(nil)
		a, err := s.ReadMenu(path)
		if err != nil {
			t.Fatal(err)
		}
		writeFixture(t, path, "second")
		b, err := s.ReadMenu(path)
		if err != nil || string(a) != "first" || string(b) != "first" {
			t.Fatal(a, b, err)
		}
	})
	t.Run("cancellation", func(t *testing.T) {
		ctx, cancel := context.WithCancel(context.Background())
		cancel()
		s := New(ctx)
		if _, err := s.ReadMenu("/unused.menu"); !errors.Is(err, context.Canceled) {
			t.Fatal(err)
		}
		if _, err := s.MenuFiles("/unused"); !errors.Is(err, context.Canceled) {
			t.Fatal(err)
		}
	})
	t.Run("nonregular-input", func(t *testing.T) {
		dir := t.TempDir()
		path := filepath.Join(dir, "stream.menu")
		if err := syscall.Mkfifo(path, 0600); err != nil {
			t.Fatal(err)
		}
		start := time.Now()
		if _, err := New(nil).ReadMenu(path); err == nil {
			t.Fatal("accepted FIFO")
		}
		if time.Since(start) > time.Second {
			t.Fatal("blocked on FIFO")
		}
		if _, err := New(nil).ReadMenu(dir); err == nil {
			t.Fatal("accepted directory")
		}
	})
	t.Run("symlink-file", func(t *testing.T) {
		dir := t.TempDir()
		path := filepath.Join(dir, "real.menu")
		writeFixture(t, path, "content")
		link := filepath.Join(dir, "link.menu")
		if err := os.Symlink(path, link); err != nil {
			t.Fatal(err)
		}
		got, err := New(nil).ReadMenu(link)
		if err != nil || string(got) != "content" {
			t.Fatal(got, err)
		}
	})
	t.Run("file-size-limit", func(t *testing.T) {
		path := filepath.Join(t.TempDir(), "large.menu")
		f, err := os.Create(path)
		if err != nil {
			t.Fatal(err)
		}
		err = f.Truncate(xdgmenu.MaxSourceBytes + 1)
		f.Close()
		if err != nil {
			t.Fatal(err)
		}
		if _, err := New(nil).ReadMenu(path); err == nil {
			t.Fatal("accepted oversized file")
		}
	})
	t.Run("job-budget", func(t *testing.T) {
		path := filepath.Join(t.TempDir(), "a.menu")
		writeFixture(t, path, "content")
		s := New(nil)
		s.bytes = len(path) + 3
		if _, err := s.ReadMenu(path); err == nil {
			t.Fatal("exceeded job bytes")
		}
		s = New(nil)
		s.items = 0
		if _, err := s.ReadMenu(path); err == nil {
			t.Fatal("exceeded job item count")
		}
	})
	for _, path := range []string{"relative.menu", "/a/../b.menu", "/bad\x00.menu", "/" + strings.Repeat("a", xdgmenu.MaxValueBytes)} {
		t.Run(path[:min(len(path), 40)], func(t *testing.T) {
			if _, err := New(nil).ReadMenu(path); err == nil {
				t.Fatal("accepted invalid path")
			}
		})
	}
}

func TestApplicationDirectoryLinks(t *testing.T) {
	dir := t.TempDir()
	apps := filepath.Join(dir, "apps")
	application(t, filepath.Join(apps, "sub", "a.desktop"), "Alpha")
	if err := os.Symlink(apps, filepath.Join(apps, "sub", "back")); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(filepath.Join(apps, "sub"), filepath.Join(apps, "alias")); err != nil {
		t.Fatal(err)
	}
	s := New(nil)
	pool, err := s.pool(apps)
	if err != nil {
		t.Fatal(err)
	}
	if len(pool) != 2 || pool["sub-a.desktop"].Entry == nil ||
		pool["alias-a.desktop"].Entry == nil {
		t.Fatal(pool)
	}
	if _, ok := pool["sub-back-sub-a.desktop"]; ok {
		t.Fatal("followed directory loop")
	}
	if _, err := s.pool(filepath.Join(dir, "absent")); err != nil {
		t.Fatal("missing optional application directory", err)
	}
}

func TestLegacyMenuPipeline(t *testing.T) {
	dir := t.TempDir()
	legacy := filepath.Join(dir, "legacy")
	application(t, filepath.Join(legacy, "a.desktop"), "Root App")
	application(t, filepath.Join(legacy, "Tools", "b.desktop"), "Tool App")
	writeFixture(t, filepath.Join(legacy, "categorized.desktop"), "[Desktop Entry]\nType=Application\nName=Categorized\nExec=fixture-app\nCategories=Utility;\n")
	writeFixture(t, filepath.Join(legacy, ".directory"), "[Desktop Entry]\nType=Directory\nName=Legacy Caption\n")
	path := filepath.Join(dir, "applications.menu")
	writeFixture(t, path, `<Menu><Name>Root</Name><LegacyDir prefix="vendor-">legacy</LegacyDir></Menu>`)
	s := New(nil)
	root, err := xdgmenu.MergeMenus(path, xdgmenu.SearchPaths{}, s)
	if err != nil {
		t.Fatal(err)
	}
	pools, err := s.Prepare(root)
	if err != nil {
		t.Fatal(err)
	}
	compiled, err := xdgmenu.BuildMenuPlans(root, pools)
	if err != nil {
		t.Fatal(err)
	}
	allocated, err := xdgmenu.AllocateMenus(compiled.Plan)
	if err != nil {
		t.Fatal(err)
	}
	got, err := xdgmenu.PresentMenus(compiled, allocated, xdgmenu.PresentationOptions{Directories: s.Directories})
	if err != nil {
		t.Fatal(err)
	}
	if got.Label != "Legacy Caption" || len(got.Items) != 2 || got.Items[0].Label != "Tools" || got.Items[1].Label != "Root App" || got.Items[0].Menu.Items[0].Label != "Tool App" {
		t.Fatal(got)
	}
	if len(compiled.Plan.Pool) != 3 || compiled.Plan.Pool["vendor-a.desktop"].Entry.Groups["Desktop Entry"]["Categories"] != "Legacy;" {
		t.Fatal(compiled.Plan.Pool)
	}
	normal, err := s.pool(legacy)
	if err != nil {
		t.Fatal(err)
	}
	if _, present := normal["a.desktop"].Entry.Groups["Desktop Entry"]["Categories"]; present {
		t.Fatal("legacy policy mutated cached normal desktop")
	}
	if normal["categorized.desktop"].Entry.Groups["Desktop Entry"]["Categories"] != "Utility;" {
		t.Fatal("changed categories")
	}
}

func TestLegacyAndNormalDirectoryOrder(t *testing.T) {
	for _, legacyLast := range []bool{false, true} {
		t.Run(map[bool]string{true: "legacy-last", false: "normal-last"}[legacyLast], func(t *testing.T) {
			dir := t.TempDir()
			apps := filepath.Join(dir, "apps")
			application(t, filepath.Join(apps, "a.desktop"), "Alpha")
			body := `<LegacyDir>apps</LegacyDir><AppDir>apps</AppDir>`
			if legacyLast {
				body = `<AppDir>apps</AppDir><LegacyDir>apps</LegacyDir>`
			}
			path := filepath.Join(dir, "applications.menu")
			writeFixture(t, path, `<Menu><Name>Root</Name>`+body+`<Include><All/></Include></Menu>`)
			s := New(nil)
			root, err := xdgmenu.MergeMenus(path, xdgmenu.SearchPaths{}, s)
			if err != nil {
				t.Fatal(err)
			}
			pools, err := s.Prepare(root)
			if err != nil {
				t.Fatal(err)
			}
			compiled, err := xdgmenu.BuildMenuPlans(root, pools)
			if err != nil {
				t.Fatal(err)
			}
			categories, err := compiled.Plan.Pool["a.desktop"].Entry.List("Desktop Entry", "Categories")
			if err != nil || (len(categories) != 0) != legacyLast {
				t.Fatal(categories, err)
			}
		})
	}
}

func TestKDELegacyInventory(t *testing.T) {
	dir := t.TempDir()
	apps := filepath.Join(dir, "apps")
	application(t, filepath.Join(apps, "a.desktop"), "Alpha")
	path := filepath.Join(dir, "applications.menu")
	writeFixture(t, path, `<Menu><Name>Root</Name><KDELegacyDirs/></Menu>`)
	s := New(nil)
	s.KDEDirs = []string{apps}
	root, err := xdgmenu.MergeMenus(path, xdgmenu.SearchPaths{}, s)
	if err != nil {
		t.Fatal(err)
	}
	pools, err := s.Prepare(root)
	if err != nil {
		t.Fatal(err)
	}
	compiled, err := xdgmenu.BuildMenuPlans(root, pools)
	if err != nil || compiled.Plan.Pool["kde-a.desktop"].Entry == nil {
		t.Fatal(compiled, err)
	}
}

func TestLegacyCategoryEncoding(t *testing.T) {
	for _, raw := range []string{`Utility;`, `Literal\;`, `Cat\\;`, `\sPadded\s;`, ""} {
		t.Run(raw, func(t *testing.T) {
			d, err := xdgmenu.ParseDesktop([]byte("[Desktop Entry]\nType=Application\nCategories=" + raw + "\n"))
			if err != nil {
				t.Fatal(err)
			}
			before, err := d.List("Desktop Entry", "Categories")
			if err != nil {
				t.Fatal(err)
			}
			copy, err := legacyDesktop(d)
			if err != nil {
				t.Fatal(err)
			}
			after, err := copy.List("Desktop Entry", "Categories")
			if err != nil || !reflect.DeepEqual(after, append(before, "Legacy")) {
				t.Fatal(before, after, err)
			}
			if d.Groups["Desktop Entry"]["Categories"] != raw {
				t.Fatal("mutated original categories")
			}
		})
	}
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"strings"
	"testing"
)

func TestMenuPlanInheritance(t *testing.T) {
	root := mergeFixture(t, `<AppDir>/parent</AppDir><DirectoryDir>/parent-dirs</DirectoryDir><DefaultLayout inline="true"><Merge type="files"/></DefaultLayout><Include><All/></Include><Menu><Name>Child</Name><AppDir>/local-first</AppDir><AppDir>/local-last</AppDir><DirectoryDir>/child-dirs</DirectoryDir><Include><All/></Include><Menu><Name>Grandchild</Name><DirectoryDir>/parent-dirs</DirectoryDir></Menu></Menu><Menu><Name>Sibling</Name><DefaultLayout inline="false"/><OnlyUnallocated/><NotOnlyUnallocated/><OnlyUnallocated/><Include><All/></Include></Menu>`, nil)
	d := parsedDesktop(t, "Name=Ordinary\n")
	hidden := parsedDesktop(t, "Name=Hidden\nHidden=true\n")
	inventories := map[AppLocation]map[string]PoolEntry{
		{Path: "/parent"}:      {"a.desktop": {"/parent/a.desktop", d}, "b.desktop": {"/parent/b.desktop", d}},
		{Path: "/local-first"}: {"a.desktop": {"/local-first/a.desktop", d}},
		{Path: "/local-last"}:  {"a.desktop": {"/local-last/a.desktop", hidden}},
	}
	got, err := BuildMenuPlans(root, inventories)
	if err != nil {
		t.Fatal(err)
	}
	t.Run("local-last-overrides-parent", func(t *testing.T) {
		if got.Plan.Pool["a.desktop"].Path != "/parent/a.desktop" || got.Children[0].Plan.Pool["a.desktop"].Entry != hidden || got.Children[0].Plan.Pool["b.desktop"].Path != "/parent/b.desktop" {
			t.Fatal(got)
		}
	})
	t.Run("ancestor-directory-priority", func(t *testing.T) {
		if !reflect.DeepEqual(got.Children[0].DirectoryDirs, []string{"/parent-dirs", "/child-dirs"}) || !reflect.DeepEqual(got.Children[0].Children[0].DirectoryDirs, []string{"/child-dirs", "/parent-dirs"}) {
			t.Fatal(got)
		}
	})
	t.Run("default-layout-inheritance", func(t *testing.T) {
		if got.Children[0].DefaultLayout != got.DefaultLayout || got.Children[0].Children[0].DefaultLayout != got.DefaultLayout || got.Children[1].DefaultLayout.Attr["inline"] != "false" {
			t.Fatal(got)
		}
	})
	t.Run("last-unallocated-flag", func(t *testing.T) {
		if !got.Children[1].Plan.OnlyUnallocated || got.Children[0].Plan.OnlyUnallocated {
			t.Fatal(got)
		}
	})
	t.Run("aligned-allocation-tree", func(t *testing.T) {
		allocated, err := AllocateMenus(got.Plan)
		if err != nil || len(allocated.Children) != 2 || allocated.Children[0].Entries[0].Entry != hidden || len(allocated.Children[1].Entries) != 0 {
			t.Fatal(allocated, err)
		}
	})
	t.Run("owned-pools-and-slices", func(t *testing.T) {
		got.Children[0].Plan.Pool["a.desktop"] = PoolEntry{}
		got.Children[0].DirectoryDirs[0] = "changed"
		if inventories[AppLocation{Path: "/local-last"}]["a.desktop"].Entry != hidden || got.Plan.Pool["a.desktop"].Entry != d || got.DirectoryDirs[0] != "/parent-dirs" {
			t.Fatal("mutated inherited input")
		}
	})
}

func TestMenuPlanValidation(t *testing.T) {
	t.Run("deleted-root-empty", func(t *testing.T) {
		got, err := BuildMenuPlans(nil, nil)
		if got != nil || err != nil {
			t.Fatal(got, err)
		}
	})
	t.Run("missing-directory-empty", func(t *testing.T) {
		got, err := BuildMenuPlans(mergeFixture(t, `<AppDir>/absent</AppDir><Include><All/></Include>`, nil), nil)
		if err != nil || len(got.Plan.Pool) != 0 {
			t.Fatal(got, err)
		}
	})
	for _, xml := range []string{`<AppDir>relative</AppDir>`, `<DirectoryDir>/non/../clean</DirectoryDir>`, `<MergeFile>/pending.menu</MergeFile>`, `<DefaultAppDirs/>`, `<Menu><Name>A</Name></Menu><Menu><Name>A</Name></Menu>`} {
		t.Run(xml, func(t *testing.T) {
			root, err := ParseMenuXML([]byte("<Menu><Name>Root</Name>" + xml + "</Menu>"))
			if err != nil {
				t.Fatal(err)
			}
			got, err := BuildMenuPlans(root, nil)
			if err == nil || got != nil {
				t.Fatal(got, err)
			}
		})
	}
	t.Run("cyclic-definition-refused", func(t *testing.T) {
		root := &Node{Tag: "Menu", Children: []*Node{{Tag: "Name", Text: "Root"}}}
		root.Children = append(root.Children, root)
		got, err := BuildMenuPlans(root, nil)
		if got != nil || err == nil {
			t.Fatal(got, err)
		}
	})
	t.Run("nil-child-refused", func(t *testing.T) {
		got, err := BuildMenuPlans(&Node{Tag: "Menu", Children: []*Node{{Tag: "Name", Text: "Root"}, nil}}, nil)
		if got != nil || err == nil {
			t.Fatal(got, err)
		}
	})
	t.Run("inventory-validation", func(t *testing.T) {
		root := mergeFixture(t, "", nil)
		d := parsedDesktop(t, "Name=Fixture\n")
		for _, input := range []map[AppLocation]map[string]PoolEntry{{{Path: "relative"}: {}}, {{Path: "/apps"}: {"wrong.txt": {"/apps/a.desktop", d}}}, {{Path: "/apps"}: {"a.desktop": {"relative", d}}}, {{Path: "/apps"}: {"a.desktop": {"/apps/a.desktop", nil}}}} {
			got, err := BuildMenuPlans(root, input)
			if got != nil || err == nil {
				t.Fatal(got, err)
			}
		}
	})
	t.Run("combined-pool-budget", func(t *testing.T) {
		root := mergeFixture(t, `<AppDir>/apps</AppDir>`, nil)
		d := parsedDesktop(t, "Name=Fixture\n")
		pool := map[string]PoolEntry{}
		for i := 0; i < 1000; i++ {
			id := strings.Repeat("a", i+1) + ".desktop"
			pool[id] = PoolEntry{"/apps/" + id, d}
		}
		for i := 0; i < 20; i++ {
			root.Children = append(root.Children, &Node{Tag: "Menu", Children: []*Node{{Tag: "Name", Text: strings.Repeat("b", i+1)}}})
		}
		got, err := BuildMenuPlans(root, map[AppLocation]map[string]PoolEntry{{Path: "/apps"}: pool})
		if got != nil || err == nil {
			t.Fatal(got, err)
		}
	})
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"strings"
	"testing"
)

func allocationRule(tag string, children ...*Node) *Node {
	return &Node{Tag: tag, Children: children}
}

func allocatedIDs(menu *AllocatedMenu) []string {
	var ids []string
	for _, entry := range menu.Entries {
		ids = append(ids, entry.ID)
	}
	return ids
}

func TestOrderedMenuAllocation(t *testing.T) {
	d := parsedDesktop(t, "Name=Fixture\nCategories=Utility;\n")
	pool := map[string]PoolEntry{"z.desktop": {"/apps/z.desktop", d}, "a.desktop": {"/apps/a.desktop", d}}
	all := &Node{Tag: "All"}
	a := &Node{Tag: "Filename", Text: "a.desktop"}
	cases := []struct {
		name  string
		rules []*Node
		want  []string
	}{
		{"no-rules", nil, nil},
		{"all-stable-order", []*Node{allocationRule("Include", all)}, []string{"a.desktop", "z.desktop"}},
		{"exclude-first", []*Node{allocationRule("Exclude", all), allocationRule("Include", all)}, []string{"a.desktop", "z.desktop"}},
		{"exclude-later", []*Node{allocationRule("Include", all), allocationRule("Exclude", a)}, []string{"z.desktop"}},
		{"reinclude", []*Node{allocationRule("Include", all), allocationRule("Exclude", a), allocationRule("Include", a)}, []string{"a.desktop", "z.desktop"}},
		{"category", []*Node{allocationRule("Include", &Node{Tag: "Category", Text: "Utility"})}, []string{"a.desktop", "z.desktop"}},
		{"boolean", []*Node{allocationRule("Include", allocationRule("And", &Node{Tag: "Category", Text: "Utility"}, allocationRule("Not", a)))}, []string{"z.desktop"}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			plan := &MenuPlan{Name: "Applications", Pool: pool, Rules: c.rules}
			got, err := AllocateMenus(plan)
			if err != nil || got == nil || !reflect.DeepEqual(allocatedIDs(got), c.want) {
				t.Fatal(got, err)
			}
			for _, item := range got.Entries {
				if item.PoolEntry != pool[item.ID] {
					t.Fatal("lost source identity")
				}
			}
			if len(pool) != 2 || pool["a.desktop"].Entry != d || len(d.Groups["Desktop Entry"]) != 3 {
				t.Fatal("mutated caller-owned pool/desktop")
			}
		})
	}
}

func TestOnlyUnallocatedPasses(t *testing.T) {
	d := parsedDesktop(t, "Name=Fixture\n")
	pool := map[string]PoolEntry{"a.desktop": {"/apps/a.desktop", d}, "b.desktop": {"/apps/b.desktop", d}}
	all := &Node{Tag: "All"}
	a := &Node{Tag: "Filename", Text: "a.desktop"}
	cases := []struct {
		name      string
		rootRules []*Node
		first     []string
		second    []string
	}{
		{"normal-first-despite-tree-order", []*Node{allocationRule("Include", a)}, []string{"b.desktop"}, nil},
		{"excluded-still-allocated", []*Node{allocationRule("Include", a), allocationRule("Exclude", a)}, []string{"b.desktop"}, nil},
		{"exclude-only-does-not-allocate", []*Node{allocationRule("Exclude", a)}, []string{"a.desktop", "b.desktop"}, nil},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			plan := &MenuPlan{Name: "Root", Children: []*MenuPlan{
				{Name: "First", Pool: pool, OnlyUnallocated: true, Rules: []*Node{allocationRule("Include", all)}},
				{Name: "Normal", Pool: pool, Rules: c.rootRules},
				{Name: "Second", Pool: pool, OnlyUnallocated: true, Rules: []*Node{allocationRule("Include", all)}},
			}}
			got, err := AllocateMenus(plan)
			if err != nil || !reflect.DeepEqual(allocatedIDs(got.Children[0]), c.first) || !reflect.DeepEqual(allocatedIDs(got.Children[2]), c.second) {
				t.Fatal(got, err)
			}
			if got.Children[0].Name != "First" || got.Children[1].Name != "Normal" || got.Children[2].Name != "Second" {
				t.Fatal("reordered output tree")
			}
		})
	}
	t.Run("ordered-unallocated-rules", func(t *testing.T) {
		got, err := AllocateMenus(&MenuPlan{Name: "Root", Pool: pool, OnlyUnallocated: true,
			Rules: []*Node{allocationRule("Include", all), allocationRule("Exclude", a), allocationRule("Include", a)}})
		if err != nil || !reflect.DeepEqual(allocatedIDs(got), []string{"a.desktop", "b.desktop"}) {
			t.Fatal(got, err)
		}
	})
	t.Run("unallocated-child-before-parent", func(t *testing.T) {
		got, err := AllocateMenus(&MenuPlan{Name: "Root", Pool: pool, OnlyUnallocated: true,
			Rules: []*Node{allocationRule("Include", all)}, Children: []*MenuPlan{{Name: "Child", Pool: pool,
				OnlyUnallocated: true, Rules: []*Node{allocationRule("Include", a)}}}})
		if err != nil || !reflect.DeepEqual(allocatedIDs(got), []string{"b.desktop"}) || !reflect.DeepEqual(allocatedIDs(got.Children[0]), []string{"a.desktop"}) {
			t.Fatal("parent fallback stole child entries", got, err)
		}
	})
	t.Run("different-sources-same-id", func(t *testing.T) {
		got, err := AllocateMenus(&MenuPlan{Name: "Root", Pool: map[string]PoolEntry{"a.desktop": {"/system/a.desktop", d}},
			Rules: []*Node{allocationRule("Include", all)}, Children: []*MenuPlan{{Name: "Override", OnlyUnallocated: true,
				Pool: map[string]PoolEntry{"a.desktop": {"/user/a.desktop", d}}, Rules: []*Node{allocationRule("Include", all)}}}})
		if err != nil || len(got.Entries) != 1 || len(got.Children[0].Entries) != 1 {
			t.Fatal("flattened ID incorrectly shared allocation across sources", got, err)
		}
	})
	t.Run("same-source-different-id", func(t *testing.T) {
		got, err := AllocateMenus(&MenuPlan{Name: "Root", Pool: map[string]PoolEntry{"sub-a.desktop": {"/apps/sub/a.desktop", d}},
			Rules: []*Node{allocationRule("Include", all)}, Children: []*MenuPlan{{Name: "Shared", OnlyUnallocated: true,
				Pool: map[string]PoolEntry{"a.desktop": {"/apps/sub/a.desktop", d}}, Rules: []*Node{allocationRule("Include", all)}}}})
		if err != nil || len(got.Entries) != 1 || len(got.Children[0].Entries) != 0 {
			t.Fatal("same source incorrectly allocated twice", got, err)
		}
	})
}

func TestAllocationRejectsInvalidPlans(t *testing.T) {
	d := parsedDesktop(t, "Name=Fixture\n")
	cycle := &MenuPlan{Name: "Cycle"}
	cycle.Children = []*MenuPlan{cycle}
	ruleCycle := allocationRule("Include")
	ruleCycle.Children = []*Node{ruleCycle}
	cases := map[string]*MenuPlan{
		"nil":                     nil,
		"empty-name":              {},
		"slash-name":              {Name: "bad/name"},
		"cycle":                   cycle,
		"nil-child":               {Name: "Root", Children: []*MenuPlan{nil}},
		"duplicate-child":         {Name: "Root", Children: []*MenuPlan{{Name: "Child"}, {Name: "Child"}}},
		"nil-rule":                {Name: "Root", Rules: []*Node{nil}},
		"wrong-rule":              {Name: "Root", Rules: []*Node{{Tag: "All"}}},
		"unknown-empty-pool-rule": {Name: "Root", Rules: []*Node{allocationRule("Include", &Node{Tag: "Unknown"})}},
		"rule-cycle":              {Name: "Root", Rules: []*Node{ruleCycle}},
		"nil-entry":               {Name: "Root", Pool: map[string]PoolEntry{"a.desktop": {"/apps/a.desktop", nil}}},
		"invalid-id":              {Name: "Root", Pool: map[string]PoolEntry{"sub/a.desktop": {"/apps/a.desktop", d}}},
		"wrong-id-extension":      {Name: "Root", Pool: map[string]PoolEntry{"a.txt": {"/apps/a.desktop", d}}},
		"relative-source":         {Name: "Root", Pool: map[string]PoolEntry{"a.desktop": {"a.desktop", d}}},
		"unclean-source":          {Name: "Root", Pool: map[string]PoolEntry{"a.desktop": {"/apps/../a.desktop", d}}},
		"invalid-categories":      {Name: "Root", Pool: map[string]PoolEntry{"a.desktop": {"/apps/a.desktop", parsedDesktop(t, "Name=Fixture\nCategories=bad\\q;\n")}}},
	}
	for name, plan := range cases {
		t.Run(name, func(t *testing.T) {
			if got, err := AllocateMenus(plan); err == nil || got != nil {
				t.Fatal("accepted malformed allocation plan", got, err)
			}
		})
	}
}

func TestAllocationBudgets(t *testing.T) {
	t.Run("global-rule-work", func(t *testing.T) {
		d := parsedDesktop(t, "Name=Fixture\nCategories="+strings.Repeat("Other;", 200)+"\n")
		rules := make([]*Node, 50000)
		for i := range rules {
			rules[i] = allocationRule("Include", &Node{Tag: "Category", Text: "Absent"})
		}
		if got, err := AllocateMenus(&MenuPlan{Name: "Root", Pool: map[string]PoolEntry{"a.desktop": {"/apps/a.desktop", d}}, Rules: rules}); err == nil || got != nil {
			t.Fatal("per-rule limits failed to bound the complete job", got, err)
		}
	})
	t.Run("category-slot-storage", func(t *testing.T) {
		pool := make(map[string]PoolEntry)
		for i := 0; i < 9; i++ {
			id := strings.Repeat("a", i+1) + ".desktop"
			pool[id] = PoolEntry{"/apps/" + id, parsedDesktop(t, "Categories="+strings.Repeat(";", MaxValueBytes-20)+"\n")}
		}
		if got, err := AllocateMenus(&MenuPlan{Name: "Root", Pool: pool}); err == nil || got != nil {
			t.Fatal("empty categories bypassed storage accounting", got, err)
		}
	})
	t.Run("category-cache-sharing", func(t *testing.T) {
		d := parsedDesktop(t, "Name=Fixture\nCategories="+strings.Repeat(";", 10000)+"\n")
		children := make([]*MenuPlan, 60)
		for i := range children {
			children[i] = &MenuPlan{Name: strings.Repeat("a", i+1), Pool: map[string]PoolEntry{"a.desktop": {"/apps/a.desktop", d}}}
		}
		got, err := AllocateMenus(&MenuPlan{Name: "Root", Children: children})
		if err != nil || len(got.Children) != 60 {
			t.Fatal("shared desktop categories were decoded repeatedly", got, err)
		}
	})
	t.Run("global-items", func(t *testing.T) {
		rules := make([]*Node, MaxItems/2)
		for i := range rules {
			rules[i] = allocationRule("Include", &Node{Tag: "All"})
		}
		if got, err := AllocateMenus(&MenuPlan{Name: "Root", Rules: rules, Children: []*MenuPlan{{Name: "Child", Rules: rules}}}); err == nil || got != nil {
			t.Fatal("per-menu limits failed to bound the complete tree", got, err)
		}
	})
	t.Run("depth", func(t *testing.T) {
		root := &MenuPlan{Name: "Root"}
		current := root
		for i := 0; i < MaxDepth; i++ {
			current.Children = []*MenuPlan{{Name: "Child"}}
			current = current.Children[0]
		}
		if got, err := AllocateMenus(root); err == nil || got != nil {
			t.Fatal("excessive depth", got, err)
		}
	})
}

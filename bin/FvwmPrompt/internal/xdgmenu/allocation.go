// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"errors"
	"path/filepath"
	"sort"
	"strings"
)

// MenuPlan is the merger's owned, consolidated menu description. Pools
// already include ancestor AppDirs followed by local AppDirs. Rules contain
// only ordered Include/Exclude nodes. Visibility/layout are later operations.
type MenuPlan struct {
	Name            string
	Pool            map[string]PoolEntry
	Rules           []*Node
	OnlyUnallocated bool
	Children        []*MenuPlan
}

type AllocatedEntry struct {
	ID string
	PoolEntry
}

type AllocatedMenu struct {
	Name     string
	Entries  []AllocatedEntry
	Children []*AllocatedMenu
}

// A compilation allowance covers all menus/entries/rules, unlike the public
// MatchRule per-evaluation limit. The entry-by-rule Cartesian product cannot
// grow without bound. Acquisition and rendering need their own job budgets;
// this count does not preempt a blocking call or promise a time deadline.
const MaxAllocationWork = 100 * MaxItems

type allocationState struct {
	remaining  int
	bytes      int
	items      int
	allocated  map[string]bool
	categories map[*Desktop][]string
	plans      []*allocationPlan
}

type allocationPlan struct {
	input      *MenuPlan
	output     *AllocatedMenu
	ids        []string
	categories map[string][]string
}

func (s *allocationState) charge(count int) error {
	if count < 0 || count > s.remaining {
		return errors.New("menu allocation exceeds total work limit")
	}
	s.remaining -= count
	return nil
}

// AllocateMenus runs normal menus before OnlyUnallocated menus, with children
// before parents and siblings in input order. Inclusion marks the source file
// allocated even if a later Exclude removes it. File paths, not flattened IDs,
// identify shared sources:
// a child override is a different entry from the same ID in its parent pool.
// Callers must provide consistent source paths across their pools.
func AllocateMenus(root *MenuPlan) (*AllocatedMenu, error) {
	s := &allocationState{remaining: MaxAllocationWork, bytes: MaxSourceBytes,
		items: MaxItems, allocated: make(map[string]bool),
		categories: make(map[*Desktop][]string)}
	output, err := s.prepare(root, 0)
	if err != nil {
		return nil, err
	}
	for _, unallocated := range []bool{false, true} {
		for _, plan := range s.plans {
			if plan.input.OnlyUnallocated != unallocated {
				continue
			}
			if err := s.apply(plan); err != nil {
				return nil, err
			}
		}
	}
	return output, nil
}

func (s *allocationState) prepare(input *MenuPlan, depth int) (*AllocatedMenu, error) {
	if input == nil || depth >= MaxDepth || len(s.plans) >= MaxItems ||
		len(input.Pool) > MaxItems || len(input.Rules) > MaxItems || len(input.Children) > MaxItems {
		return nil, errors.New("invalid or excessive menu allocation tree")
	}
	if !pathValue(input.Name) || input.Name == "" || strings.Contains(input.Name, "/") || len(input.Name) > s.bytes {
		return nil, errors.New("invalid menu allocation name")
	}
	s.bytes -= len(input.Name)
	count := 1 + len(input.Pool) + len(input.Rules)
	if count > s.items {
		return nil, errors.New("menu allocation exceeds total item limit")
	}
	s.items -= count
	if err := s.charge(count + len(input.Children)); err != nil {
		return nil, err
	}
	output := &AllocatedMenu{Name: input.Name}
	plan := &allocationPlan{input: input, output: output, categories: make(map[string][]string)}
	for id, entry := range input.Pool {
		if !pathValue(id) || !strings.HasSuffix(id, ".desktop") || strings.Contains(id, "/") || entry.Entry == nil ||
			!pathValue(entry.Path) || !filepath.IsAbs(entry.Path) || filepath.Clean(entry.Path) != entry.Path || len(id)+len(entry.Path) > s.bytes {
			return nil, errors.New("invalid or excessive menu allocation pool")
		}
		s.bytes -= len(id) + len(entry.Path)
		categories, cached := s.categories[entry.Entry]
		if !cached {
			var err error
			categories, err = entry.Entry.List("Desktop Entry", "Categories")
			if err != nil {
				return nil, err
			}
			// Include string-slot storage, not just nonempty category text.
			if len(categories) > s.bytes/16 {
				return nil, errors.New("menu allocation category storage exceeds byte limit")
			}
			s.bytes -= len(categories) * 16
			for _, category := range categories {
				if len(category) > s.bytes {
					return nil, errors.New("menu allocation categories exceed byte limit")
				}
				s.bytes -= len(category)
			}
			if err := s.charge(len(categories)); err != nil {
				return nil, err
			}
			s.categories[entry.Entry] = categories
		}
		plan.ids = append(plan.ids, id)
		plan.categories[id] = categories
	}
	sort.Strings(plan.ids)
	for _, rule := range input.Rules {
		if rule == nil || rule.Tag != "Include" && rule.Tag != "Exclude" {
			return nil, errors.New("menu allocation requires Include/Exclude rules")
		}
		// Validate even an empty pool's rules, including every branch, using
		// the same bounded evaluator as the later real-entry pass.
		if _, err := matchRule(rule, "", nil, 0, &s.remaining); err != nil {
			return nil, err
		}
	}
	seen := make(map[string]bool)
	for _, child := range input.Children {
		if child == nil || seen[child.Name] {
			return nil, errors.New("menu allocation requires consolidated child names")
		}
		seen[child.Name] = true
		result, err := s.prepare(child, depth+1)
		if err != nil {
			return nil, err
		}
		output.Children = append(output.Children, result)
	}
	s.plans = append(s.plans, plan)
	return output, nil
}

func (s *allocationState) apply(plan *allocationPlan) error {
	// Snapshot eligibility once for this menu so its ordered rules can remove
	// and re-add an entry. Later unallocated menus see its allocation marks.
	eligible := make(map[string]bool)
	included := make(map[string]bool)
	for _, id := range plan.ids {
		if err := s.charge(1); err != nil {
			return err
		}
		eligible[id] = !plan.input.OnlyUnallocated || !s.allocated[plan.input.Pool[id].Path]
	}
	for _, rule := range plan.input.Rules {
		for _, id := range plan.ids {
			if err := s.charge(1); err != nil {
				return err
			}
			if !eligible[id] || rule.Tag == "Exclude" && !included[id] {
				continue
			}
			matched, err := matchRule(rule, id, plan.categories[id], 0, &s.remaining)
			if err != nil {
				return err
			}
			if matched {
				included[id] = rule.Tag == "Include"
				if included[id] {
					s.allocated[plan.input.Pool[id].Path] = true
				}
			}
		}
	}
	for _, id := range plan.ids {
		if err := s.charge(1); err != nil {
			return err
		}
		if included[id] {
			plan.output.Entries = append(plan.output.Entries,
				AllocatedEntry{ID: id, PoolEntry: plan.input.Pool[id]})
		}
	}
	return nil
}

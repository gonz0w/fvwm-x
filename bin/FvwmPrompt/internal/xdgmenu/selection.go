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

type MenuSelection struct {
	Desktop, MenuType, Prefix, UserDirectory string
	Saved                                    []string
	All                                      bool
}

// SelectMenus retains saved choices, explicit filters and desktop weighting.
// Search precedence and basename shadowing have already been resolved by
// DiscoverMenus. Selection never tests the filesystem or changes its inventory.
func SelectMenus(inventory []MenuFile, options MenuSelection) ([]MenuFile, error) {
	if len(inventory) > MaxItems || len(options.Saved) > MaxItems {
		return nil, errors.New("excessive menu selection")
	}
	bytes := MaxSourceBytes
	for _, value := range []string{options.Desktop, options.MenuType, options.Prefix, options.UserDirectory} {
		if !pathValue(value) || len(value) > bytes {
			return nil, errors.New("invalid menu selection")
		}
		bytes -= len(value)
	}
	for _, value := range options.Saved {
		if !pathValue(value) || len(value) > bytes {
			return nil, errors.New("invalid saved menu selection")
		}
		bytes -= len(value)
	}
	for _, menu := range inventory {
		if !pathValue(menu.Path) || !filepath.IsAbs(menu.Path) || !pathValue(menu.Name) || filepath.Base(menu.Path) != menu.Name || !strings.HasSuffix(menu.Name, ".menu") || len(menu.Name)+len(menu.Path) > bytes {
			return nil, errors.New("invalid menu inventory")
		}
		bytes -= len(menu.Name) + len(menu.Path)
	}
	desktop := options.Desktop
	if desktop == "" && options.Prefix != "" {
		desktop = strings.TrimSuffix(options.Prefix, "-")
	}
	if options.All && desktop == "" && options.MenuType == "" {
		return append([]MenuFile(nil), inventory...), nil
	}
	remaining := MaxAllocationWork
	match := func(pattern, name string) (bool, error) {
		remaining--
		if remaining < 0 {
			return false, errors.New("menu selection exceeds work limit")
		}
		return filepath.Match(pattern, name)
	}
	if len(options.Saved) != 0 && desktop == "" && options.MenuType == "" {
		var result []MenuFile
		seen := make(map[string]bool)
		for _, name := range options.Saved {
			for _, menu := range inventory {
				found, err := match("*"+name+".menu", menu.Name)
				if err != nil {
					return nil, err
				}
				if found && !seen[menu.Path] {
					result = append(result, menu)
					seen[menu.Path] = true
				}
			}
		}
		if len(result) != 0 {
			return result, nil
		}
	}
	var filtered []MenuFile
	for _, menu := range inventory {
		found, err := match("*"+desktop+"*"+options.MenuType+"*", menu.Name)
		if err != nil {
			return nil, err
		}
		if found {
			filtered = append(filtered, menu)
		}
	}
	if options.All {
		return filtered, nil
	}
	if desktop != "" {
		return filtered, nil
	}
	groups := make(map[string][]MenuFile)
	user := make(map[string]int)
	known := []string{"gnome", "kde", "xfce", "lxde", "cinnamon", "mate", "debian"}
	for _, menu := range filtered {
		group := "none"
		if strings.Contains(menu.Name, "-") {
			group = "others"
		}
		for _, name := range known {
			if strings.Contains(menu.Name, name) {
				group = name
				break
			}
		}
		groups[group] = append(groups[group], menu)
		if options.UserDirectory != "" && filepath.Dir(menu.Path) == options.UserDirectory {
			user[group]++
		}
	}
	var candidates []string
	for group := range groups {
		if group != "none" && group != "others" {
			candidates = append(candidates, group)
		}
	}
	sort.Strings(candidates)
	winner := ""
	for _, candidate := range candidates {
		if winner == "" || len(groups[candidate]) > len(groups[winner]) || len(groups[candidate]) == len(groups[winner]) && user[candidate] >= user[winner] {
			winner = candidate
		}
	}
	var result []MenuFile
	if winner != "" {
		result = append(result, groups[winner]...)
	}
	result = append(result, groups["others"]...)
	for _, menu := range groups["none"] {
		stem := strings.TrimSuffix(menu.Name, ".menu")
		duplicate := false
		for _, selected := range result {
			remaining--
			if remaining < 0 {
				return nil, errors.New("menu selection exceeds work limit")
			}
			if strings.Contains(strings.TrimSuffix(selected.Name, ".menu"), stem) {
				duplicate = true
				break
			}
		}
		if !duplicate {
			result = append(result, menu)
		}
	}
	return result, nil
}

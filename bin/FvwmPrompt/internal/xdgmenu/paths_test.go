// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"strings"
	"testing"
)

func TestSearchDirectories(t *testing.T) {
	cases := []struct {
		name        string
		environment map[string]string
		config      []string
		data        []string
	}{
		{"defaults", nil, []string{"/home/fixture/.config", "/etc/xdg"}, []string{"/home/fixture/.local/share", "/usr/local/share", "/usr/share"}},
		{"empty", map[string]string{"XDG_CONFIG_HOME": "", "XDG_CONFIG_DIRS": "", "XDG_DATA_HOME": "", "XDG_DATA_DIRS": ""}, []string{"/home/fixture/.config", "/etc/xdg"}, []string{"/home/fixture/.local/share", "/usr/local/share", "/usr/share"}},
		{"custom", map[string]string{"XDG_CONFIG_HOME": "/custom/config", "XDG_CONFIG_DIRS": "/vendor/first:/vendor/second", "XDG_DATA_HOME": "/custom/data", "XDG_DATA_DIRS": "/vendor/data"}, []string{"/custom/config", "/vendor/first", "/vendor/second"}, []string{"/custom/data", "/vendor/data"}},
		{"relative-empty-duplicates", map[string]string{"XDG_CONFIG_HOME": "relative", "XDG_CONFIG_DIRS": ":relative:/first/../first:/first:/second:", "XDG_DATA_HOME": "relative", "XDG_DATA_DIRS": "/home/fixture/.local/share:/data:/data/"}, []string{"/home/fixture/.config", "/first", "/second"}, []string{"/home/fixture/.local/share", "/data"}},
		{"no-valid-system-path", map[string]string{"XDG_CONFIG_DIRS": "relative::other", "XDG_DATA_DIRS": ":other"}, []string{"/home/fixture/.config"}, []string{"/home/fixture/.local/share"}},
		{"spaces-unicode", map[string]string{"XDG_CONFIG_HOME": "/café config", "XDG_DATA_HOME": "/data spaces"}, []string{"/café config", "/etc/xdg"}, []string{"/data spaces", "/usr/local/share", "/usr/share"}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			got, err := SearchDirectories("/home/fixture", c.environment)
			if err != nil || !reflect.DeepEqual(got.Config, c.config) || !reflect.DeepEqual(got.Data, c.data) {
				t.Fatal(got, err)
			}
		})
	}
	for name, home := range map[string]string{"empty-home": "", "relative-home": "relative", "nul-home": "/home\x00bad", "invalid-utf8-home": "/home\xff", "oversized-home": "/" + strings.Repeat("a", MaxValueBytes)} {
		t.Run(name, func(t *testing.T) {
			if got, err := SearchDirectories(home, nil); err == nil || got.Config != nil || got.Data != nil {
				t.Fatal(got, err)
			}
		})
	}
	for _, key := range []string{"XDG_CONFIG_HOME", "XDG_CONFIG_DIRS", "XDG_DATA_HOME", "XDG_DATA_DIRS"} {
		t.Run(key, func(t *testing.T) {
			if _, err := SearchDirectories("/home/fixture", map[string]string{key: "/bad\x00value"}); err == nil {
				t.Fatal("accepted NUL in environment")
			}
		})
	}
}

func TestMenuDiscoveryPrecedence(t *testing.T) {
	cases := []struct {
		name string
		dirs []MenuDirectory
		want []MenuFile
	}{
		{"empty", nil, nil},
		{"custom-no-etc", []MenuDirectory{{"/custom/menus", []string{"tools.menu", "applications.menu", "ignored.txt"}}}, []MenuFile{{"/custom/menus/applications.menu", "applications.menu"}, {"/custom/menus/tools.menu", "tools.menu"}}},
		{"override", []MenuDirectory{{"/user/menus", []string{"applications.menu"}}, {"/vendor/menus", []string{"applications.menu", "extra.menu"}}}, []MenuFile{{"/user/menus/applications.menu", "applications.menu"}, {"/vendor/menus/extra.menu", "extra.menu"}}},
		{"stable-order", []MenuDirectory{{"/first", []string{"z.menu", "a.menu", "a.menu"}}, {"/second", []string{"b.menu"}}}, []MenuFile{{"/first/a.menu", "a.menu"}, {"/first/z.menu", "z.menu"}, {"/second/b.menu", "b.menu"}}},
		{"case-and-spaces", []MenuDirectory{{"/path spaces", []string{"Café Menu.menu", "Upper.MENU", "a.menu"}}}, []MenuFile{{"/path spaces/Café Menu.menu", "Café Menu.menu"}, {"/path spaces/a.menu", "a.menu"}}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			original := make([][]string, len(c.dirs))
			for i, dir := range c.dirs {
				original[i] = append([]string(nil), dir.Names...)
			}
			got, err := DiscoverMenus(c.dirs)
			if err != nil || !reflect.DeepEqual(got, c.want) {
				t.Fatal(got, err)
			}
			for i, dir := range c.dirs {
				if !reflect.DeepEqual(dir.Names, original[i]) {
					t.Fatal("mutated supplied inventory")
				}
			}
		})
	}
}

func TestMenuDiscoveryLimits(t *testing.T) {
	longDir := "/" + strings.Repeat("a", MaxValueBytes-2)
	cases := map[string][]MenuDirectory{
		"relative-directory": {{"relative", nil}},
		"nul-directory":      {{"/bad\x00", nil}},
		"empty-name":         {{"/menus", []string{""}}},
		"path-name":          {{"/menus", []string{"../a.menu"}}},
		"dot":                {{"/menus", []string{"."}}},
		"nul-name":           {{"/menus", []string{"a\x00.menu"}}},
		"invalid-utf8":       {{"/menus", []string{"\xff.menu"}}},
		"many-directories":   make([]MenuDirectory, MaxItems+1),
		"many-names":         {{"/menus", make([]string, MaxItems+1)}},
		"combined-path":      {{longDir, []string{"a.menu"}}},
		"combined-output":    {{"/" + strings.Repeat("a", 50000), menuNames(200)}},
	}
	for name, dirs := range cases {
		t.Run(name, func(t *testing.T) {
			if got, err := DiscoverMenus(dirs); err == nil || got != nil {
				t.Fatal("accepted invalid inventory", len(got), err)
			}
		})
	}
}

func menuNames(count int) []string {
	result := make([]string, count)
	for i := range result {
		result[i] = strings.Repeat("x", i+1) + ".menu"
	}
	return result
}

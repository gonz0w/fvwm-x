// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"testing"
)

func TestMenuSelection(t *testing.T) {
	inventory := []MenuFile{{"/user/gnome-applications.menu", "gnome-applications.menu"}, {"/system/gnome-settings.menu", "gnome-settings.menu"}, {"/system/kde-applications.menu", "kde-applications.menu"}, {"/system/tool-extra.menu", "tool-extra.menu"}, {"/system/applications.menu", "applications.menu"}, {"/system/global.menu", "global.menu"}}
	cases := []struct {
		name    string
		options MenuSelection
		want    []int
	}{
		{"weighting", MenuSelection{UserDirectory: "/user"}, []int{0, 1, 3, 5}},
		{"all", MenuSelection{All: true}, []int{0, 1, 2, 3, 4, 5}},
		{"explicit-desktop", MenuSelection{Desktop: "kde"}, []int{2}},
		{"ordered-desktop-type", MenuSelection{Desktop: "gnome", MenuType: "settings"}, []int{1}},
		{"prefix", MenuSelection{Prefix: "gnome-", MenuType: "applications"}, []int{0}},
		{"saved-order", MenuSelection{Saved: []string{"kde-applications", "gnome-settings"}}, []int{2, 1}},
		{"saved-duplicates", MenuSelection{Saved: []string{"kde-applications", "kde-applications"}}, []int{2}},
		{"missing-saved-fallback", MenuSelection{Saved: []string{"absent"}}, []int{0, 1, 3, 5}},
		{"filter-overrides-saved", MenuSelection{Saved: []string{"kde-applications"}, Desktop: "gnome"}, []int{0, 1}},
		{"all-filtered", MenuSelection{All: true, MenuType: "applications"}, []int{0, 2, 4}},
		{"absent-filter", MenuSelection{Desktop: "absent"}, nil},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			got, err := SelectMenus(inventory, c.options)
			if err != nil {
				t.Fatal(err)
			}
			var want []MenuFile
			for _, i := range c.want {
				want = append(want, inventory[i])
			}
			if !reflect.DeepEqual(got, want) {
				t.Fatal(got, want)
			}
		})
	}
	t.Run("global-only", func(t *testing.T) {
		got, err := SelectMenus(inventory[4:], MenuSelection{})
		if err != nil || !reflect.DeepEqual(got, inventory[4:]) {
			t.Fatal(got, err)
		}
	})
	t.Run("literal-tie-last-and-user-weight", func(t *testing.T) {
		menus := []MenuFile{{"/system/gnome.menu", "gnome.menu"}, {"/user/kde.menu", "kde.menu"}}
		got, err := SelectMenus(menus, MenuSelection{UserDirectory: "/user"})
		if err != nil || !reflect.DeepEqual(got, menus[1:]) {
			t.Fatal(got, err)
		}
		got, err = SelectMenus(menus, MenuSelection{UserDirectory: "/system"})
		if err != nil || !reflect.DeepEqual(got, menus[:1]) {
			t.Fatal(got, err)
		}
	})
	t.Run("bad-pattern", func(t *testing.T) {
		if got, err := SelectMenus(inventory, MenuSelection{Desktop: "["}); got != nil || err == nil {
			t.Fatal(got, err)
		}
	})
}

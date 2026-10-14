// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"strings"
	"testing"
)

func TestPreferenceDataRoundTrip(t *testing.T) {
	cases := map[string][]Preference{
		"empty":                nil,
		"ordinary":             {{"Theme", "gnome"}, {"Title", "XDGMenu"}, {"MENapplications", "on"}},
		"whole-tail":           {{"Title", "Café Menu"}, {"TermCmd", "terminal --option  spaced"}},
		"literal-values":       {{"TermCmd", `terminal "quoted" '$literal' \data`}, {"Title", "$(literal) `$data`"}},
		"empty-and-unknown":    {{"Size", ""}, {"FutureField", "opaque data"}, {"MENold", ""}},
		"duplicate-records":    {{"TitlesOn", "on"}, {"TitlesOff", "on"}, {"TitlesOn", ""}, {"MENold", "on"}, {"MENold", ""}},
		"non-ascii-whitespace": {{"Title", "\u00a0Café\u00a0"}},
	}
	for name, records := range cases {
		t.Run(name, func(t *testing.T) {
			data, err := EncodePreferences(records)
			if err != nil {
				t.Fatal(err)
			}
			got, err := ParsePreferences(data)
			if err != nil || !reflect.DeepEqual(got, records) {
				t.Fatal(got, err)
			}
		})
	}
	t.Run("legacy-padding-comments-and-unrelated", func(t *testing.T) {
		data := "# old saved file\r\n  " + preferencePrefix + " Title        Café Menu   \r\n" +
			preferencePrefix + " TermCmd   terminal -e \\literal\t\n" +
			preferencePrefix + " Size\n" + "*OtherModule: Title Other\nEcho ignored\n\n"
		want := []Preference{{"Title", "Café Menu"}, {"TermCmd", "terminal -e \\literal"}, {"Size", ""}}
		got, err := ParsePreferences([]byte(data))
		if err != nil || !reflect.DeepEqual(got, want) {
			t.Fatal(got, err)
		}
	})
}

func TestPreferenceDefaults(t *testing.T) {
	t.Run("built-in", func(t *testing.T) {
		got, err := DefaultsFromPreferences(nil)
		want := ProviderDefaults{Size: 24, Titles: true, Theme: "gnome", Title: "XDGMenu", IconDir: "~/.fvwm/icons", DirIcon: "gnome-fs-directory", AppIcon: "gnome-applications", IncludeItems: "both", TermCmd: "xterm -e"}
		if err != nil || !reflect.DeepEqual(got, want) {
			t.Fatal(got, err)
		}
	})
	t.Run("all-legacy-fields", func(t *testing.T) {
		records := []Preference{{"Size", "32"}, {"IconsOn", "on"}, {"TitlesOff", "on"}, {"Theme", "custom theme"}, {"Title", "Café Menu"}, {"InsertInto", "Menu Root"}, {"Installprefix", "/custom/menus"}, {"IconDir", "/icon spaces"}, {"DirIcon", "dir icon"}, {"AppIcon", "app icon"}, {"IncludeNone", "on"}, {"TermCmd", `terminal --name "XDG Terminal" -e`}, {"Path", "/path spaces/menu"}, {"MENfirst", "on"}, {"MENsecond", "on"}, {"FutureField", "ignored"}}
		want := ProviderDefaults{Icons: true, Size: 32, Titles: false, Theme: "custom theme", Title: "Café Menu", InsertInto: "Menu Root", InstallPrefix: "/custom/menus", IconDir: "/icon spaces", DirIcon: "dir icon", AppIcon: "app icon", IncludeItems: "none", TermCmd: `terminal --name "XDG Terminal" -e`, OutputPath: "/path spaces/menu", Menus: []string{"first", "second"}}
		got, err := DefaultsFromPreferences(records)
		if err != nil || !reflect.DeepEqual(got, want) {
			t.Fatal(got, err)
		}
	})
	t.Run("ordered-toggles-and-duplicates", func(t *testing.T) {
		records := []Preference{{"IconsOn", "on"}, {"IconsOff", "on"}, {"TitlesOff", "on"}, {"TitlesOn", "on"}, {"TitlesOff", "off"}, {"Title", "First"}, {"Title", "Last"}, {"Title", ""}, {"MENa", "on"}, {"MENb", "on"}, {"MENa", ""}, {"MENc", "on"}, {"MENb", "on"}}
		got, err := DefaultsFromPreferences(records)
		if err != nil || got.Icons || !got.Titles || got.Title != "Last" || !reflect.DeepEqual(got.Menus, []string{"b", "c"}) {
			t.Fatal(got, err)
		}
	})
	for key, want := range map[string]string{"IncludeConfig": "config", "IncludeRegen": "regenerate", "IncludeBoth": "both", "IncludeNone": "none"} {
		t.Run(key, func(t *testing.T) {
			got, err := DefaultsFromPreferences([]Preference{{key, "on"}})
			if err != nil || got.IncludeItems != want {
				t.Fatal(got, err)
			}
		})
	}
	for name, value := range map[string]string{"zero-size": "0", "negative-size": "-1", "large-size": "65536"} {
		t.Run(name, func(t *testing.T) {
			got, err := DefaultsFromPreferences([]Preference{{"Size", value}})
			if err != nil {
				t.Fatal("size policy belongs to rendering, not saved data", got, err)
			}
		})
	}
}

func TestPreferenceLimits(t *testing.T) {
	for name, data := range map[string]string{
		"nul":          preferencePrefix + " Title bad\x00value\n",
		"invalid-utf8": preferencePrefix + " Title \xff\n",
		"missing-key":  preferencePrefix + "\n",
		"source":       strings.Repeat("a", MaxSourceBytes+1),
		"line":         preferencePrefix + " Title " + strings.Repeat("a", MaxValueBytes),
		"line-count":   strings.Repeat("\n", MaxItems+1),
		"embedded-cr":  preferencePrefix + " Title a\rb\n",
	} {
		t.Run("parse-"+name, func(t *testing.T) {
			if got, err := ParsePreferences([]byte(data)); err == nil || got != nil {
				t.Fatal("accepted malformed preferences", got, err)
			}
		})
	}
	cases := map[string][]Preference{
		"empty-key":          {{"", "value"}},
		"spaced-key":         {{"bad key", "value"}},
		"nul-value":          {{"Title", "bad\x00value"}},
		"newline-value":      {{"Title", "line1\nline2"}},
		"cr-value":           {{"Title", "line1\rline2"}},
		"invalid-key-utf8":   {{"\xff", "value"}},
		"invalid-value-utf8": {{"Title", "\xff"}},
		"large-value":        {{"Title", strings.Repeat("a", MaxValueBytes+1)}},
		"many-records":       make([]Preference, MaxItems+1),
	}
	for name, records := range cases {
		t.Run("encode-"+name, func(t *testing.T) {
			if got, err := EncodePreferences(records); err == nil || got != nil {
				t.Fatal(got, err)
			}
		})
		t.Run("defaults-"+name, func(t *testing.T) {
			if got, err := DefaultsFromPreferences(records); err == nil || !reflect.DeepEqual(got, ProviderDefaults{}) {
				t.Fatal(got, err)
			}
		})
	}
	for name, value := range map[string]string{"leading-space": " value", "trailing-space": "value ", "leading-tab": "\tvalue", "trailing-tab": "value\t"} {
		t.Run("encode-"+name, func(t *testing.T) {
			if got, err := EncodePreferences([]Preference{{"Title", value}}); err == nil || got != nil {
				t.Fatal("lossy legacy encoding", got, err)
			}
		})
	}
	for name, value := range map[string]string{"non-numeric": "bad", "overflow": "9999999999999999999999999999"} {
		t.Run("size-"+name, func(t *testing.T) {
			if got, err := DefaultsFromPreferences([]Preference{{"Size", value}}); err == nil || !reflect.DeepEqual(got, ProviderDefaults{}) {
				t.Fatal(got, err)
			}
		})
	}
	t.Run("aggregate-encoding", func(t *testing.T) {
		records := make([]Preference, 200)
		for i := range records {
			records[i] = Preference{"Title", strings.Repeat("a", 50000)}
		}
		if got, err := EncodePreferences(records); err == nil || got != nil {
			t.Fatal("excessive encoded preferences", len(got), err)
		}
		if got, err := DefaultsFromPreferences(records); err == nil || !reflect.DeepEqual(got, ProviderDefaults{}) {
			t.Fatal("excessive applied preferences", got, err)
		}
	})
}

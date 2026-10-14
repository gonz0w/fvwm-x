// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"strings"
	"testing"
)

func parsedDesktop(t *testing.T, fields string) *Desktop {
	t.Helper()
	d, err := ParseDesktop([]byte("[Desktop Entry]\nType=Application\n" + fields))
	if err != nil {
		t.Fatal(err)
	}
	return d
}

func TestDesktopGroupsAndValues(t *testing.T) {
	d := parsedDesktop(t, "Name=Café\\sMenu\nIcon=fixture\nCategories=Utility;Text\\;Tools;;\n"+
		"[Desktop Action Edit]\nName=Edit\\tDocument\nExec=fixture-edit\n")
	name, err := d.String("Desktop Entry", "Name")
	if err != nil || name != "Café Menu" {
		t.Fatal(name, err)
	}
	categories, err := d.List("Desktop Entry", "Categories")
	if err != nil || !reflect.DeepEqual(categories, []string{"Utility", "Text;Tools", ""}) {
		t.Fatal(categories, err)
	}
	action, err := d.String("Desktop Action Edit", "Name")
	if err != nil || action != "Edit\tDocument" {
		t.Fatal(action, err)
	}
	missing, err := d.String("missing", "missing")
	if err != nil || missing != "" {
		t.Fatal(missing, err)
	}
	d = parsedDesktop(t, "Name=Fixture\u00a0\n")
	name, err = d.String("Desktop Entry", "Name")
	if err != nil || name != "Fixture\u00a0" {
		t.Fatal("non-ASCII name whitespace changed", name, err)
	}
}

func TestDesktopRefusesMalformedInput(t *testing.T) {
	cases := map[string]string{
		"no-group":       "Name=Fixture\n",
		"no-main-group":  "[Other]\nName=Fixture\n",
		"duplicate":      "[Desktop Entry]\nName=One\nName=Two\n",
		"repeated-group": "[Desktop Entry]\n[Desktop Entry]\n",
		"bad-key":        "[Desktop Entry]\nName with space=One\n",
		"empty-locale":   "[Desktop Entry]\nName[]=One\n",
		"bad-locale":     "[Desktop Entry]\nName[fr/FR]=One\n",
		"bad-group":      "[Desktop Entry]\n[bad\x01group]\n",
		"no-equals":      "[Desktop Entry]\nName\n",
		"nul":            "[Desktop Entry]\nName=One\x00Two\n",
		"invalid-utf8":   "[Desktop Entry]\nName=\xff\n",
		"oversized":      strings.Repeat(" ", MaxSourceBytes+1),
		"long-line":      "[Desktop Entry]\nName=" + strings.Repeat("a", MaxValueBytes),
		"many-lines":     "[Desktop Entry]\n" + strings.Repeat("\n", MaxItems),
	}
	for name, input := range cases {
		t.Run(name, func(t *testing.T) {
			if d, err := ParseDesktop([]byte(input)); err == nil || d != nil {
				t.Fatal("accepted malformed entry", d, err)
			}
		})
	}
}

func TestDesktopValueEscapes(t *testing.T) {
	for _, raw := range []string{`unknown\q`, `incomplete\`, "control\x01", `list\;escape`} {
		t.Run(raw, func(t *testing.T) {
			if _, err := decodeValue(raw, false); err == nil {
				t.Fatal("accepted invalid string escape")
			}
		})
	}
	values, err := decodeValue(`\s\n\t\r\\`, false)
	if err != nil || values[0] != " \n\t\r\\" {
		t.Fatal(values, err)
	}
	d := parsedDesktop(t, "Name=Fixture\r\nHidden=false\r\n")
	value, err := d.Boolean("Desktop Entry", "Hidden")
	if err != nil || value {
		t.Fatal(value, err)
	}
	for _, raw := range []string{"", "True", "1", "yes"} {
		t.Run("boolean-"+raw, func(t *testing.T) {
			d := parsedDesktop(t, "Hidden="+raw+"\n")
			if _, err := d.Boolean("Desktop Entry", "Hidden"); err == nil {
				t.Fatal("accepted invalid boolean")
			}
		})
	}
}

func TestLocaleFallback(t *testing.T) {
	cases := []struct {
		locale string
		want   []string
	}{
		{"sr_YU.UTF-8@Latn", []string{"sr_YU@Latn", "sr_YU", "sr@Latn", "sr"}},
		{"sr_YU", []string{"sr_YU", "sr"}},
		{"sr@Latn", []string{"sr@Latn", "sr"}},
		{"fr.UTF-8", []string{"fr"}},
		{"C.UTF-8", nil},
		{"POSIX", nil},
	}
	for _, c := range cases {
		t.Run(c.locale, func(t *testing.T) {
			if got := LocaleCandidates(c.locale); !reflect.DeepEqual(got, c.want) {
				t.Fatal(got, c.want)
			}
		})
	}
	d := parsedDesktop(t, "Name=Base\nName[sr_YU]=Country\nName[sr@Latn]=Modifier\n"+
		"Name[sr]=Language\nName[fr]=Bonjour\n")
	for locale, want := range map[string]string{"sr_YU.UTF-8@Latn": "Country", "sr_RS@Latn": "Modifier",
		"sr_RS": "Language", "fr": "Bonjour", "de": "Base", "C": "Base"} {
		t.Run("name-"+locale, func(t *testing.T) {
			got, err := d.Localized("Desktop Entry", "Name", []string{locale})
			if err != nil || got != want {
				t.Fatal(got, err)
			}
		})
	}
	d = parsedDesktop(t, "Name[fr]=Bonjour\n")
	if _, err := d.Localized("Desktop Entry", "Name", []string{"fr"}); err == nil {
		t.Fatal("localized key without required base accepted")
	}
}

func TestVisibilityPolicy(t *testing.T) {
	cases := []struct {
		name, fields string
		visible      bool
		invalid      bool
	}{
		{"ordinary", "", true, false},
		{"hidden", "Hidden=true\n", false, false},
		{"nodisplay", "NoDisplay=true\n", false, false},
		{"only-match", "OnlyShowIn=GNOME;FVWM;\n", true, false},
		{"only-other", "OnlyShowIn=GNOME;\n", false, false},
		{"not-match", "NotShowIn=FVWM;\n", false, false},
		{"not-other", "NotShowIn=GNOME;\n", true, false},
		{"case-sensitive", "OnlyShowIn=fvwm;\n", false, false},
		{"both-filters", "OnlyShowIn=FVWM;\nNotShowIn=GNOME;\n", false, true},
		{"try-present", "TryExec=fixture-present\n", true, false},
		{"try-absent", "TryExec=fixture-absent\n", false, false},
		{"bad-boolean", "Hidden=1\n", false, true},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			d := parsedDesktop(t, c.fields)
			got, err := d.Visible([]string{"FVWM"}, func(value string) bool { return value == "fixture-present" })
			if got != c.visible || (err != nil) != c.invalid {
				t.Fatal(got, err)
			}
		})
	}
	d := parsedDesktop(t, "TryExec=fixture-present\n")
	if visible, err := d.Visible(nil, nil); err != nil || visible {
		t.Fatal(visible, err)
	}
}

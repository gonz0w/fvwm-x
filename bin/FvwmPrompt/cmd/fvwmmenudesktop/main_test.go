// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"context"
	"encoding/base64"
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

func TestDesktopCLIOptions(t *testing.T) {
	d, _ := xdgmenu.DefaultsFromPreferences(nil)
	for _, args := range [][]string{{"--without-titles", "--with-titles"}, {"-w", "--without-titles"}, {"--title", "two words", "--set-menus", "/a.menu /b.menu"}, {"--dynamic", "--all-menus", "--include-items", "none"}, {"--size", "32", "--enable-mini-icons"}, {"-vw", "-s32", "-tTwo words"}, {"--title", "-dash data"}, {"--enable-style", "--xterm", "ignored old value"}} {
		t.Run(strings.Join(args, " "), func(t *testing.T) {
			o, err := parseOptions(args, d)
			if err != nil {
				t.Fatal(err)
			}
			if args[0] == "--without-titles" && !o.Titles || args[0] == "-w" && o.Titles {
				t.Fatal(o)
			}
		})
	}
	for _, args := range [][]string{{"--get-menus", "bad"}, {"--include-items", "bad"}, {"--install-prefix", "relative"}, {"--size", "bad"}, {"--timeout", "0s"}, {"--unknown"}} {
		t.Run(strings.Join(args, " "), func(t *testing.T) {
			if _, err := parseOptions(args, d); err == nil {
				t.Fatal("accepted invalid option")
			}
		})
	}
	for _, size := range []string{"0", "-1", "65536"} {
		if _, err := parseOptions([]string{"--enable-mini-icons", "--size", size}, d); err != nil {
			t.Fatal("numeric legacy size must remain usable", err)
		}
	}
}

func TestDesktopCLIProductionPipeline(t *testing.T) {
	root := t.TempDir()
	home := filepath.Join(root, "home")
	user := filepath.Join(home, ".fvwm")
	apps := filepath.Join(root, "data", "applications")
	menus := filepath.Join(root, "config", "menus")
	for _, dir := range []string{user, apps, menus} {
		if err := os.MkdirAll(dir, 0700); err != nil {
			t.Fatal(err)
		}
	}
	t.Setenv("HOME", home)
	t.Setenv("FVWM_USERDIR", user)
	t.Setenv("XDG_CONFIG_HOME", filepath.Dir(menus))
	t.Setenv("XDG_CONFIG_DIRS", filepath.Join(root, "missing"))
	t.Setenv("XDG_DATA_HOME", filepath.Dir(apps))
	t.Setenv("XDG_DATA_DIRS", filepath.Join(root, "missing-data"))
	t.Setenv("XDG_MENU_PREFIX", "")
	t.Setenv("LANG", "C")
	t.Setenv("LC_ALL", "C")
	entry := filepath.Join(apps, "a.desktop")
	if err := os.WriteFile(entry, []byte("[Desktop Entry]\nType=Application\nName=Alpha & 100%\nExec=fixture-app \"two words\" %%\n"), 0600); err != nil {
		t.Fatal(err)
	}
	menu := filepath.Join(menus, "applications.menu")
	if err := os.WriteFile(menu, []byte(`<Menu><Name>Applications</Name><DefaultAppDirs/><Include><All/></Include></Menu>`), 0600); err != nil {
		t.Fatal(err)
	}
	t.Run("list-has-no-action-publication", func(t *testing.T) {
		var out bytes.Buffer
		if err := produce(context.Background(), []string{"--get-menus", "all"}, &out, &bytes.Buffer{}); err != nil || out.String() != menu+"\n" {
			t.Fatal(out.String(), err)
		}
		entries, _ := os.ReadDir(user)
		if len(entries) != 0 {
			t.Fatal("published during discovery", entries)
		}
	})
	t.Run("render-and-private-literal-records", func(t *testing.T) {
		var out bytes.Buffer
		if err := produce(context.Background(), []string{"--set-menus", menu, "--include-items", "none"}, &out, &bytes.Buffer{}); err != nil {
			t.Fatal(err)
		}
		if !strings.Contains(out.String(), `+ "Alpha && 100%%" Exec exec fvwm-menu-desktop --launch `) {
			t.Fatal(out.String())
		}
		entries, _ := os.ReadDir(user)
		if len(entries) != 2 {
			t.Fatal(entries)
		}
		var catalogPath string
		for _, entry := range entries {
			if !strings.HasSuffix(entry.Name(), ".lock") {
				catalogPath = filepath.Join(user, entry.Name())
			}
		}
		data, err := xdgsource.ReadPrivate(catalogPath)
		if err != nil {
			t.Fatal(err)
		}
		var catalog actionCatalog
		if err = json.Unmarshal(data, &catalog); err != nil {
			t.Fatal(err)
		}
		if len(catalog.Records) != 1 {
			t.Fatal(catalog)
		}
		for _, record := range catalog.Records {
			if !reflect.DeepEqual(record.Argv, []string{"fixture-app", "two words", "%"}) {
				t.Fatal(record)
			}
		}
	})
}

func TestApplicationActionEncoding(t *testing.T) {
	e, err := newActionEncoder(t.TempDir(), "Menu", "xterm -e")
	if err != nil {
		t.Fatal(err)
	}
	item := xdgmenu.PresentedItem{Argv: []string{"fixture-app", "a ' quoted $ value", "\n"}, WorkingDir: "/working path"}
	action, err := e.encode(item)
	if err != nil {
		t.Fatal(err)
	}
	parts := strings.Fields(action)
	if len(parts) != 6 || parts[0] != "Exec" || parts[4] == "" {
		t.Fatal(action)
	}
	path, err := base64.RawURLEncoding.DecodeString(parts[4])
	if err != nil || string(path) != e.path {
		t.Fatal(path, err)
	}
	if strings.ContainsAny(parts[4]+parts[5], "$'\"`\\;\n") {
		t.Fatal("action contains metadata syntax", action)
	}
	if err = e.publish(); err != nil {
		t.Fatal(err)
	}
	data, err := xdgsource.ReadPrivate(e.path)
	if err != nil {
		t.Fatal(err)
	}
	var catalog actionCatalog
	if err = json.Unmarshal(data, &catalog); err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(catalog.Records[parts[5]].Argv, item.Argv) {
		t.Fatal(catalog)
	}
	if _, err = e.encode(xdgmenu.PresentedItem{}); err == nil {
		t.Fatal("accepted empty action")
	}
}

func TestDesktopIconScaleOption(t *testing.T) {
	defaults, _ := xdgmenu.DefaultsFromPreferences(nil)
	for _, tc := range []struct {
		text     string
		quarters int
	}{{"1", 4}, {"1.25", 5}, {"2.5", 10}, {"4.00", 16}} {
		o, err := parseOptions([]string{"--icon-scale", tc.text}, defaults)
		if err != nil || o.iconScaleQuarters != tc.quarters {
			t.Fatal(tc, o.iconScaleQuarters, err)
		}
	}
	for _, text := range []string{"", "0", "4.25", "1.1", "1e0", "-1", "NaN"} {
		if _, err := parseOptions([]string{"--icon-scale", text}, defaults); err == nil {
			t.Fatal("accepted", text)
		}
	}
	o, err := parseOptions(nil, defaults)
	if err != nil || o.iconScaleQuarters != 4 {
		t.Fatal(o.iconScaleQuarters, err)
	}
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"context"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestMenuParityDesktopEnvironmentDoesNotReplaceSelector(t *testing.T) {
	root := t.TempDir()
	home, config, data := filepath.Join(root, "home"), filepath.Join(root, "config"), filepath.Join(root, "data")
	for _, dir := range []string{home, filepath.Join(home, ".fvwm"), filepath.Join(config, "menus"), filepath.Join(data, "applications")} {
		if err := os.MkdirAll(dir, 0700); err != nil {
			t.Fatal(err)
		}
	}
	for key, value := range map[string]string{"HOME": home, "FVWM_USERDIR": filepath.Join(home, ".fvwm"), "XDG_CONFIG_HOME": config, "XDG_CONFIG_DIRS": filepath.Join(root, "no-config"), "XDG_DATA_HOME": data, "XDG_DATA_DIRS": filepath.Join(root, "no-data"), "XDG_MENU_PREFIX": "", "LC_ALL": "C", "LANG": "C", "XDG_CURRENT_DESKTOP": "Other:Third"} {
		t.Setenv(key, value)
	}
	for name, visibility := range map[string]string{"Other visible": "OnlyShowIn=Other;", "FVWM hidden": "OnlyShowIn=FVWM;", "Other excluded": "NotShowIn=Other;"} {
		if err := os.WriteFile(filepath.Join(data, "applications", name+".desktop"), []byte("[Desktop Entry]\nType=Application\nName="+name+"\nExec=/bin/true\n"+visibility+"\n"), 0600); err != nil {
			t.Fatal(err)
		}
	}
	menu := filepath.Join(config, "menus", "chosen-applications.menu")
	if err := os.WriteFile(menu, []byte(`<Menu><Name>Chosen menu</Name><DefaultAppDirs/><Include><All/></Include></Menu>`), 0600); err != nil {
		t.Fatal(err)
	}
	var out bytes.Buffer
	if err := produce(context.Background(), []string{"--desktop", "chosen", "--include-items", "none"}, &out, &bytes.Buffer{}); err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(out.String(), `+ "Other visible" Exec`) || strings.Contains(out.String(), `+ "FVWM hidden"`) || strings.Contains(out.String(), `+ "Other excluded"`) {
		t.Fatal("visibility ignored environment or --desktop became visibility", out.String())
	}
	t.Setenv("XDG_CURRENT_DESKTOP", "")
	out.Reset()
	if err := produce(context.Background(), []string{"--desktop", "chosen", "--include-items", "none"}, &out, &bytes.Buffer{}); err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(out.String(), `+ "FVWM hidden" Exec`) || strings.Contains(out.String(), `+ "Other visible"`) {
		t.Fatal("empty desktop environment did not fall back FVWM", out.String())
	}
}

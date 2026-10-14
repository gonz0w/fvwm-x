// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"context"
	"encoding/base64"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

func fieldArgument(key, value string) string {
	return base64.RawURLEncoding.EncodeToString([]byte(key)) + "=" + base64.RawURLEncoding.EncodeToString([]byte(value))
}
func TestFormDataBoundary(t *testing.T) {
	root := t.TempDir()
	initial := []xdgmenu.Preference{{Key: "UnknownFutureField", Value: "kept"}, {Key: "Title", Value: "before"}}
	data, _ := xdgmenu.EncodePreferences(initial)
	if err := os.WriteFile(settingsPath(root), data, 0644); err != nil {
		t.Fatal(err)
	}
	var fields fieldFlags
	for _, pair := range [][2]string{{"Title", `A 'quoted' $[value] ; literal`}, {"Path", filepath.Join(root, "output")}, {"IconsOn", "on"}, {"IconsOff", ""}} {
		if err := fields.Set(fieldArgument(pair[0], pair[1])); err != nil {
			t.Fatal(err)
		}
	}
	records, encoded, err := formPreferences(root, fields)
	if err != nil {
		t.Fatal(err)
	}
	d, err := xdgmenu.DefaultsFromPreferences(records)
	if err != nil || d.Title != `A 'quoted' $[value] ; literal` || !d.Icons {
		t.Fatal(d, err)
	}
	if !bytes.Contains(encoded, []byte("UnknownFutureField kept")) {
		t.Fatal(string(encoded))
	}
	if err := xdgsource.PublishPrivate(settingsPath(root), encoded); err != nil {
		t.Fatal(err)
	}
	info, _ := os.Stat(settingsPath(root))
	if info.Mode().Perm() != 0600 {
		t.Fatal(info.Mode())
	}
	before, _ := os.ReadFile(settingsPath(root))
	var invalid fieldFlags
	if err := invalid.Set(fieldArgument("Title", "two\nlines")); err != nil {
		t.Fatal(err)
	}
	if _, _, err := formPreferences(root, invalid); err == nil {
		t.Fatal("accepted multiline preference")
	}
	after, _ := os.ReadFile(settingsPath(root))
	if !bytes.Equal(before, after) {
		t.Fatal("modified on refusal")
	}
	for _, value := range []string{"!", "YQ", "YQ=%%%"} {
		var f fieldFlags
		if f.Set(value) == nil {
			t.Fatal(value)
		}
	}
}

func TestNativeFormOutput(t *testing.T) {
	root := t.TempDir()
	paths := xdgmenu.SearchPaths{Config: []string{root}, Data: []string{root}}
	o := options{}
	o.ProviderDefaults, _ = xdgmenu.DefaultsFromPreferences(nil)
	o.Title = `quoted " and $[literal]`
	var out bytes.Buffer
	if err := renderForm(context.Background(), xdgsource.New(nil), paths, map[string]string{}, root, o, &out); err != nil {
		t.Fatal(err)
	}
	text := out.String()
	for _, required := range []string{"Module FvwmForm FvwmForm-XDGMenu-Config", "$(Title:Base64)", "--form-action save", "--form-action regenerate", "--form-action export", "$$[literal]"} {
		if !strings.Contains(text, required) {
			t.Fatal(required, text)
		}
	}
	if !strings.Contains(text, "Command !fvwm-menu-desktop") || !strings.Contains(text, "CommandAppend") || strings.Contains(text, "UseData") || strings.Contains(text, "$(Title)") {
		t.Fatal(text)
	}
}

func TestActionReferenceRetention(t *testing.T) {
	root := t.TempDir()
	first, err := newActionEncoder(root, "menu", "xterm -e")
	if err != nil {
		t.Fatal(err)
	}
	old, err := first.encode(xdgmenu.PresentedItem{Argv: []string{"old-app", "literal"}})
	if err != nil {
		t.Fatal(err)
	}
	if err = first.publish(); err != nil {
		t.Fatal(err)
	}
	next, err := newActionEncoder(root, "menu", "xterm -e")
	if err != nil {
		t.Fatal(err)
	}
	if len(next.catalog.Records) != 1 {
		t.Fatal(next.catalog)
	}
	if _, err = next.encode(xdgmenu.PresentedItem{Argv: []string{"new-app"}}); err != nil {
		t.Fatal(err)
	}
	if err = next.publish(); err != nil {
		t.Fatal(err)
	}
	id := strings.Fields(old)[5]
	if next.catalog.Records[id].Argv[0] != "old-app" {
		t.Fatal("evicted live/exported action")
	}
}

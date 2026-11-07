// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import (
	"encoding/json"
	"os"
	"strings"
	"testing"
)

func paletteFixture(t *testing.T) []byte {
	t.Helper()
	data, err := os.ReadFile("../../../../default-config/palettes.json")
	if err != nil {
		t.Fatal(err)
	}
	return data
}
func TestPaletteSourceCompleteAndOperationOwned(t *testing.T) {
	data := paletteFixture(t)
	first, err := DecodePalettes(data)
	if err != nil || first["slate"]["dark"]["surface"] != "#202a31" {
		t.Fatal(first, err)
	}
	first["slate"]["dark"]["surface"] = "#123456"
	second, err := DecodePalettes(data)
	if err != nil || second["slate"]["dark"]["surface"] != "#202a31" {
		t.Fatal("source aliases another operation", err)
	}
}
func TestPaletteSourceRejectsAmbiguousAndMalformedData(t *testing.T) {
	data := paletteFixture(t)
	for _, malformed := range [][]byte{
		[]byte(`null`), []byte(`[]`), append(data, []byte(` {}`)...),
		[]byte(strings.Repeat(" ", 65537)),
		[]byte(strings.Replace(string(data), `"surface":"#202a31"`, `"surface":"#202a31","surface":"#000000"`, 1)),
		[]byte(strings.Replace(string(data), `"dark":`, `"dark":{},"dark":`, 1)),
	} {
		if _, err := DecodePalettes(malformed); err == nil {
			t.Fatal("accepted malformed palette data")
		}
	}
	for _, change := range []func(map[string]any){
		func(p map[string]any) { delete(p, "grove") },
		func(p map[string]any) { p["extra"] = p["grove"] },
		func(p map[string]any) { p["grove"] = nil },
		func(p map[string]any) { delete(p["ink"].(map[string]any), "light") },
		func(p map[string]any) { p["ink"].(map[string]any)["light"] = []any{} },
		func(p map[string]any) { p["dune"].(map[string]any)["dark"].(map[string]any)["surface"] = "red" },
		func(p map[string]any) { p["dune"].(map[string]any)["dark"].(map[string]any)["surface"] = "#12345z" },
		func(p map[string]any) { p["dune"].(map[string]any)["dark"].(map[string]any)["surface"] = 1 },
		func(p map[string]any) { p["dune"].(map[string]any)["dark"].(map[string]any)["extra"] = "#123456" },
	} {
		var object map[string]any
		if err := json.Unmarshal(data, &object); err != nil {
			t.Fatal(err)
		}
		change(object)
		malformed, _ := json.Marshal(object)
		if _, err := DecodePalettes(malformed); err == nil {
			t.Fatal("accepted incomplete/invalid palette")
		}
	}
	bad := strings.Replace(string(data), `"surface":"#202a31"`, `"surface":"bad"`, 1)
	if _, err := DecodePalettes([]byte(bad)); err == nil || !strings.Contains(err.Error(), "slate.dark.surface") {
		t.Fatal("missing role context", err)
	}
}

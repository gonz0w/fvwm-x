// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"image"
	"image/color"
	"image/png"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

func TestNativeIconConversion(t *testing.T) {
	root := t.TempDir()
	input := filepath.Join(root, "icon.png")
	write := func(c color.NRGBA) {
		img := image.NewNRGBA(image.Rect(0, 0, 2, 2))
		img.SetNRGBA(0, 0, c)
		var data bytes.Buffer
		if err := png.Encode(&data, img); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(input, data.Bytes(), 0600); err != nil {
			t.Fatal(err)
		}
	}
	o := options{}
	o.ProviderDefaults, _ = xdgmenu.DefaultsFromPreferences(nil)
	o.Size = 4
	o.IconDir = filepath.Join(root, "cache")
	resolve := func(path string) string {
		r, err := newIconRenderer(xdgsource.New(nil), xdgmenu.SearchPaths{}, map[string]string{"HOME": root}, o)
		if err != nil {
			t.Fatal(err)
		}
		got, err := r.resolve(path, false)
		if err != nil {
			t.Fatal(err)
		}
		return got
	}
	write(color.NRGBA{R: 255, A: 128})
	first := resolve(input)
	data, err := os.ReadFile(first)
	if err != nil {
		t.Fatal(err)
	}
	img, err := png.Decode(bytes.NewReader(data))
	if err != nil {
		t.Fatal(err)
	}
	if img.Bounds().Dx() != 4 || img.Bounds().Dy() != 4 || color.NRGBAModel.Convert(img.At(0, 0)).(color.NRGBA).A != 128 {
		t.Fatal(img.Bounds(), img.At(0, 0))
	}
	write(color.NRGBA{G: 255, A: 255})
	if next := resolve(input); next == first {
		t.Fatal("reused stale core cache identity")
	}
	svg := filepath.Join(root, "icon.svg")
	if err := os.WriteFile(svg, []byte("<svg/>"), 0600); err != nil {
		t.Fatal(err)
	}
	if got := resolve(svg); !strings.HasSuffix(got, "icon.svg:4x4") {
		t.Fatal(got)
	}
}

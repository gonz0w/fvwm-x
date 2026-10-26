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
	if got := resolve(svg); !strings.HasSuffix(got, ".svg:4x4") || !strings.HasPrefix(got, o.IconDir+string(os.PathSeparator)) {
		t.Fatal(got)
	}
}

func TestNativeIconFallbackScaleAndFormatGates(t *testing.T) {
	root := t.TempDir()
	data := filepath.Join(root, "data")
	theme := filepath.Join(data, "icons", "test")
	if err := os.MkdirAll(filepath.Join(theme, "24x24"), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(theme, "index.theme"), []byte("[Icon Theme]\nDirectories=24x24\n[24x24]\nSize=24\nType=Fixed\n"), 0600); err != nil {
		t.Fatal(err)
	}
	for _, name := range []string{"application", "directory"} {
		if err := os.WriteFile(filepath.Join(theme, "24x24", name+".svg"), []byte("<svg>"+name+"</svg>"), 0600); err != nil {
			t.Fatal(err)
		}
	}
	o := options{iconScaleQuarters: 5}
	o.ProviderDefaults, _ = xdgmenu.DefaultsFromPreferences(nil)
	o.Theme, o.Size, o.AppIcon, o.DirIcon, o.IconDir = "test", 17, "application", "directory", filepath.Join(root, "cache")
	makeRenderer := func() *iconRenderer {
		r, err := newIconRenderer(xdgsource.New(nil), xdgmenu.SearchPaths{Data: []string{data}}, map[string]string{"HOME": root}, o)
		if err != nil {
			t.Fatal(err)
		}
		return r
	}
	r := makeRenderer()
	app, err := r.resolve("missing", false)
	if err != nil || !strings.HasSuffix(app, ".svg:22x22") {
		t.Fatal(app, err)
	}
	directory, err := r.resolve("missing", true)
	if err != nil || !strings.HasSuffix(directory, ".svg:22x22") || directory == app {
		t.Fatal(directory, err)
	}
	original := [3]string{pngEnabled, svgEnabled, xpmEnabled}
	defer func() { pngEnabled, svgEnabled, xpmEnabled = original[0], original[1], original[2] }()
	pngEnabled, svgEnabled, xpmEnabled = "false", "false", "false"
	if got, err := makeRenderer().resolve("missing", false); err != nil || got != "" {
		t.Fatal(got, err)
	}
	for _, size := range []int{0, -1, 65536} {
		o.Size = size
		if got, err := makeRenderer().resolve("missing", false); err != nil || got != "" {
			t.Fatal(size, got, err)
		}
	}
}

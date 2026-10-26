// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package iconrender

import (
	"bytes"
	"context"
	"encoding/binary"
	"errors"
	"hash/crc32"
	"image"
	"image/color"
	"image/png"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

func fixturePNG(t *testing.T, size int, pixel color.NRGBA) []byte {
	t.Helper()
	picture := image.NewNRGBA(image.Rect(0, 0, size, size))
	for y := 0; y < size; y++ {
		for x := 0; x < size; x++ {
			picture.SetNRGBA(x, y, pixel)
		}
	}
	var out bytes.Buffer
	if err := png.Encode(&out, picture); err != nil {
		t.Fatal(err)
	}
	return out.Bytes()
}

func writeFixture(t *testing.T, path string, data []byte) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, data, 0600); err != nil {
		t.Fatal(err)
	}
}

func newTestRenderer(t *testing.T, dir string, extensions []string) *Renderer {
	t.Helper()
	r, err := New(xdgsource.New(nil), xdgmenu.SearchPaths{}, dir,
		Config{CacheDir: filepath.Join(dir, "cache"), Extensions: extensions})
	if err != nil {
		t.Fatal(err)
	}
	return r
}

func TestRenderedContentGeneration(t *testing.T) {
	for _, extension := range []string{"png", "svg", "xpm"} {
		t.Run(extension, func(t *testing.T) {
			dir := t.TempDir()
			path := filepath.Join(dir, "source."+extension)
			before, after := []byte("first source"), []byte("second source")
			if extension == "png" {
				before = fixturePNG(t, 16, color.NRGBA{R: 255, A: 255})
				after = fixturePNG(t, 16, color.NRGBA{G: 255, A: 255})
			}
			writeFixture(t, path, before)
			r := newTestRenderer(t, dir, []string{extension})
			request := xdgmenu.IconRequest{Name: path, Size: 16, ScaleQuarters: 4}
			old, err := r.Resolve(request)
			if err != nil || old == "" || old == path {
				t.Fatal("missing private content copy", old, err)
			}
			asset := strings.TrimSuffix(old, ":16x16")
			copied, err := os.ReadFile(asset)
			if err != nil || !bytes.Equal(copied, before) {
				t.Fatal("changed unscaled image bytes", err)
			}
			info, err := os.Stat(asset)
			if err != nil || info.Mode().Perm() != 0600 {
				t.Fatal("nonprivate artifact", info, err)
			}
			writeFixture(t, path, after)
			if got, err := r.Resolve(request); err != nil || got != old {
				t.Fatal("job snapshot changed", got, err)
			}
			fresh := newTestRenderer(t, dir, []string{extension})
			next, err := fresh.Resolve(request)
			if err != nil || next == "" || next == old {
				t.Fatal("fresh Source reused stale core image name", next, err)
			}
			if copied, err := os.ReadFile(asset); err != nil || !bytes.Equal(copied, before) {
				t.Fatal("previous generation artifact changed", err)
			}
		})
	}
}

func TestRenderPhysicalScaleAndRequestCache(t *testing.T) {
	dir := t.TempDir()
	normal := filepath.Join(dir, `normal\literal.png`)
	selected := filepath.Join(dir, `selected\literal.png`)
	writeFixture(t, normal, fixturePNG(t, 16, color.NRGBA{R: 255, A: 255}))
	writeFixture(t, selected, fixturePNG(t, 16, color.NRGBA{G: 255, A: 255}))
	r := newTestRenderer(t, dir, []string{"png"})
	request := xdgmenu.IconRequest{Name: normal, Size: 17, ScaleQuarters: 5}
	fractional, err := r.Resolve(request)
	if err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(fractional)
	if err != nil {
		t.Fatal(err)
	}
	config, err := png.DecodeConfig(bytes.NewReader(data))
	if err != nil || config.Width != 22 || config.Height != 22 {
		t.Fatal("fractional physical extent", config, err)
	}
	request.State = "selected"
	request.Override = selected
	chosen, err := r.Resolve(request)
	if err != nil || chosen == fractional {
		t.Fatal("override/state cache collision", chosen, err)
	}
	request.State = "disabled"
	request.Override = filepath.Join(dir, "absent.png")
	if got, err := r.Resolve(request); err != nil || got != fractional {
		t.Fatal("missing override did not use normal image", got, err)
	}
	request.State = ""
	request.Override = ""
	if got, err := r.Resolve(request); err != nil || got != fractional {
		t.Fatal("override leaked to normal request", got, err)
	}
	request.ScaleQuarters = 4
	if got, err := r.Resolve(request); err != nil || got == fractional {
		t.Fatal("scale cache collision", got, err)
	}
	request.Name = filepath.Join(dir, "absent.png")
	request.Fallback = selected
	if got, err := r.Resolve(request); err != nil || got == "" || got == fractional {
		t.Fatal("fallback cache collision", got, err)
	}
}

func TestSVGPhysicalPresentationAndFormatGates(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "source.svg")
	writeFixture(t, path, []byte("<svg/>"))
	request := xdgmenu.IconRequest{Name: path, Size: 17, ScaleQuarters: 5}
	r := newTestRenderer(t, dir, []string{"svg"})
	got, err := r.Resolve(request)
	if err != nil || !strings.HasSuffix(got, ".svg:22x22") {
		t.Fatal("SVG physical extent", got, err)
	}
	for _, extensions := range [][]string{nil, {"png"}, {"xpm"}} {
		r = newTestRenderer(t, t.TempDir(), extensions)
		got, err = r.Resolve(request)
		if err != nil || got != "" {
			t.Fatal("disabled SVG resolved", got, err)
		}
		if _, err := os.Stat(r.dir); !os.IsNotExist(err) {
			t.Fatal("unavailable format wrote cache", err)
		}
	}
	r = newTestRenderer(t, t.TempDir(), nil)
	request.Name = "anything"
	request.Theme = "unread"
	if got, err := r.Resolve(request); err != nil || got != "" || len(r.cache) != 0 {
		t.Fatal("disabled renderer performed acquisition", got, err)
	}
	request.ScaleQuarters = 3
	if _, err := r.Resolve(request); err == nil {
		t.Fatal("disabled renderer accepted malformed request")
	}
}

func TestRenderBoundsAndDeadline(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "input.png")
	writeFixture(t, path, []byte("malformed PNG"))
	request := xdgmenu.IconRequest{Name: path, Size: 24, ScaleQuarters: 4}
	r := newTestRenderer(t, dir, []string{"png"})
	if _, err := r.Resolve(request); err == nil {
		t.Fatal("accepted malformed PNG")
	}
	large := fixturePNG(t, 1, color.NRGBA{A: 255})
	binary.BigEndian.PutUint32(large[16:20], 32769)
	binary.BigEndian.PutUint32(large[29:33], crc32.ChecksumIEEE(large[12:29]))
	writeFixture(t, path, large)
	r = newTestRenderer(t, dir, []string{"png"})
	if _, err := r.Resolve(request); err == nil {
		t.Fatal("accepted oversized PNG geometry")
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	r, err := New(xdgsource.New(ctx), xdgmenu.SearchPaths{}, dir,
		Config{CacheDir: filepath.Join(dir, "cancelled"), Extensions: []string{"png"}})
	if err != nil {
		t.Fatal(err)
	}
	if _, err := r.Resolve(request); !errors.Is(err, context.Canceled) {
		t.Fatal("ignored cancellation", err)
	}
	r = newTestRenderer(t, dir, []string{"png"})
	for i := 0; i < xdgmenu.MaxItems; i++ {
		r.cache[xdgmenu.IconRequest{Name: strconv.Itoa(i)}] = ""
	}
	if _, err := r.Resolve(request); err == nil {
		t.Fatal("ignored request cache bound")
	}
	r = newTestRenderer(t, dir, []string{"png"})
	r.cacheBytes = 0
	if _, err := r.Resolve(request); err == nil {
		t.Fatal("ignored request byte bound")
	}
}

func TestAggregatePublicationBudget(t *testing.T) {
	dir := t.TempDir()
	firstSource, secondSource := filepath.Join(dir, "first.png"), filepath.Join(dir, "second.png")
	firstPixel := color.NRGBA{R: 255, A: 255}
	writeFixture(t, firstSource, fixturePNG(t, 2, firstPixel))
	writeFixture(t, secondSource, fixturePNG(t, 2, color.NRGBA{G: 255, A: 255}))
	r := newTestRenderer(t, dir, []string{"png"})
	if r.outputBytes != xdgsource.MaxJobBytes {
		t.Fatal("publication budget is not bounded to source job limit", r.outputBytes)
	}
	// Both small sources require resampling; the remaining aggregate budget
	// allows precisely the first rendered image, not another publication.
	r.outputBytes = len(fixturePNG(t, 4, firstPixel))
	request := xdgmenu.IconRequest{Name: firstSource, Size: 4, ScaleQuarters: 4}
	first, err := r.Resolve(request)
	if err != nil || first == "" || r.outputBytes != 0 {
		t.Fatal("first publication did not consume budget", first, r.outputBytes, err)
	}
	before, err := os.ReadDir(r.dir)
	if err != nil || len(before) != 1 {
		t.Fatal("missing first private artifact", before, err)
	}
	request.Name = secondSource
	if got, err := r.Resolve(request); err == nil || got != "" {
		t.Fatal("published image beyond aggregate budget", got, err)
	}
	after, err := os.ReadDir(r.dir)
	if err != nil || len(after) != 1 || after[0].Name() != before[0].Name() {
		t.Fatal("rejected publication became visible", after, err)
	}
	request.Name = firstSource
	if got, err := r.Resolve(request); err != nil || got != first {
		t.Fatal("exhaustion broke existing generation cache", got, err)
	}
}

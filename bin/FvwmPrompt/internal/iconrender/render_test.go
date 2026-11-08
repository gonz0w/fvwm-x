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
			} else if extension == "xpm" {
				before = []byte("/* XPM */ static char *x[]={\"1 1 1 1\",\". c #ff0000\",\".\"};\n")
				after = []byte("/* XPM */ static char *x[]={\"1 1 1 1\",\". c #00ff00\",\".\"};\n")
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

func TestOptionalImageFallbackPreservesStrictResolution(t *testing.T) {
	dir := t.TempDir()
	bad, good := filepath.Join(dir, "bad.png"), filepath.Join(dir, "good.png")
	writeFixture(t, bad, []byte("malformed PNG"))
	writeFixture(t, good, fixturePNG(t, 16, color.NRGBA{G: 255, A: 255}))
	r := newTestRenderer(t, dir, []string{"png"})
	request := xdgmenu.IconRequest{Name: bad, Fallback: good, Size: 16, ScaleQuarters: 4}
	var notices []error
	got, err := r.ResolveOptional(request, func(err error) { notices = append(notices, err) })
	if err != nil || got == "" || len(notices) != 1 {
		t.Fatal("malformed image lost its fallback", got, err, notices)
	}
	var asset *AssetError
	if _, err := r.Resolve(request); !errors.As(err, &asset) {
		t.Fatal("optional cache changed strict error contract", err)
	}
	request.Override, request.Name, request.Fallback = bad, good, bad
	if path, err := r.ResolveOptional(request, nil); err != nil || path != got {
		t.Fatal("malformed override did not use name before fallback", path, err)
	}
	request.Override, request.Name, request.Fallback = "", bad, bad
	if path, err := r.ResolveOptional(request, nil); err != nil || path != "" {
		t.Fatal("malformed images lost text fallback", path, err)
	}
	truncated := filepath.Join(dir, "header-only.png")
	data := fixturePNG(t, 16, color.NRGBA{A: 255})
	writeFixture(t, truncated, data[:33])
	request.Name, request.Fallback = truncated, good
	if path, err := r.ResolveOptional(request, nil); err != nil || path != got {
		t.Fatal("same-size malformed pixel data bypassed fallback", path, err)
	}
	if _, err := r.Resolve(request); !errors.As(err, &asset) {
		t.Fatal("strict PNG accepted header without pixels", err)
	}
	// A selected file can disappear between finder admission and acquisition.
	if _, err := r.present(filepath.Join(dir, "disappeared.png"), 16); !errors.As(err, &asset) || !errors.Is(err, os.ErrNotExist) {
		t.Fatal("disappeared image was not classified locally", err)
	}
}

func TestOptionalBoundedXPMAndMalformedTheme(t *testing.T) {
	dir := t.TempDir()
	bad := filepath.Join(dir, "bad.xpm")
	writeFixture(t, bad, []byte("/* XPM */ static char *x[]={\"no geometry\"};"))
	good := filepath.Join(dir, "good.xpm")
	data := []byte("/* XPM */ static char *x[]={\"1 1 1 1\",\". c #fff\",\".\"};\n")
	writeFixture(t, good, data)
	request := xdgmenu.IconRequest{Name: bad, Size: 24, ScaleQuarters: 4}
	for _, pixels := range []int{64 * 1024 * 1024, 256 * 256} {
		r := newTestRenderer(t, dir, []string{"xpm"})
		r.sourcePixels = pixels
		if path, err := r.ResolveOptional(request, nil); err != nil || path != "" {
			t.Fatal("malformed XPM did not retain text", pixels, path, err)
		}
		var asset *AssetError
		if _, err := r.Resolve(request); !errors.As(err, &asset) {
			t.Fatal("strict XPM admission lost typed error", pixels, err)
		}
		fallback := request
		fallback.Fallback = good
		path, err := r.ResolveOptional(fallback, nil)
		if err != nil || path == "" {
			t.Fatal("malformed XPM header lost healthy fallback", pixels, path, err)
		}
		if published, err := os.ReadFile(path); err != nil || !bytes.Equal(published, data) {
			t.Fatal("admitted native XPM bytes changed", pixels, err)
		}
	}
	writeFixture(t, filepath.Join(dir, ".icons", "broken", "index.theme"), []byte("[unrelated]\n"))
	r := newTestRenderer(t, dir, []string{"png"})
	request.Name, request.Theme = "application", "broken"
	var content *xdgsource.IconThemeContentError
	if _, err := r.Resolve(request); !errors.As(err, &content) {
		t.Fatal("strict theme error lost its classification", err)
	}
	if path, err := r.ResolveOptional(request, nil); err != nil || path != "" {
		t.Fatal("malformed theme did not select explicit text fallback", path, err)
	}
}

func TestOptionalFailuresRetainFatalBoundaries(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "good.png")
	writeFixture(t, path, fixturePNG(t, 16, color.NRGBA{A: 255}))
	request := xdgmenu.IconRequest{Name: path, Size: 16, ScaleQuarters: 4}
	for _, kind := range []string{"invalid", "cancel", "acquisition", "cache", "publication", "unsafe-publication"} {
		t.Run(kind, func(t *testing.T) {
			r := newTestRenderer(t, t.TempDir(), []string{"png"})
			candidate := request
			switch kind {
			case "invalid":
				candidate.Fallback = "../invalid"
			case "cancel":
				ctx, cancel := context.WithCancel(context.Background())
				cancel()
				r.source = xdgsource.New(ctx)
			case "acquisition":
				r.source, _ = xdgsource.NewLimited(context.Background(), 1, 1)
				r.finder.Source = r.source
			case "cache":
				r.cacheBytes = 1
			case "publication":
				r.outputBytes = 1
			case "unsafe-publication":
				r.dir = filepath.Join(t.TempDir(), "not-directory")
				writeFixture(t, r.dir, []byte("existing private file"))
			}
			var asset *AssetError
			if got, err := r.ResolveOptional(candidate, nil); err == nil || got != "" || errors.As(err, &asset) {
				t.Fatal("fatal failure treated as missing image", got, err)
			}
		})
	}
}

func TestOptionalDiagnosticsAreBounded(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "bad.png")
	writeFixture(t, path, []byte("bad PNG"))
	r := newTestRenderer(t, dir, []string{"png"})
	notices := 0
	for i := 0; i < 12; i++ {
		_, err := r.ResolveOptional(xdgmenu.IconRequest{Name: path, Size: 16, ScaleQuarters: 4}, func(err error) {
			notices++
			if len(err.Error()) > 256 || strings.ContainsAny(err.Error(), "\r\n\x00") {
				t.Fatal("unbounded optional diagnostic", err)
			}
		})
		if err != nil {
			t.Fatal(err)
		}
	}
	if notices != 8 {
		t.Fatal("diagnostic count", notices)
	}
}

func TestOptionalDiagnosticCancellationWinsOverTextFallback(t *testing.T) {
	for _, kind := range []string{"image", "theme"} {
		t.Run(kind, func(t *testing.T) {
			dir := t.TempDir()
			ctx, cancel := context.WithCancel(context.Background())
			defer cancel()
			r, err := New(xdgsource.New(ctx), xdgmenu.SearchPaths{}, dir,
				Config{CacheDir: filepath.Join(dir, "cache"), Extensions: []string{"png"}})
			if err != nil {
				t.Fatal(err)
			}
			request := xdgmenu.IconRequest{Size: 24, ScaleQuarters: 4}
			if kind == "image" {
				request.Name = filepath.Join(dir, "bad.png")
				writeFixture(t, request.Name, []byte("malformed PNG"))
			} else {
				request.Name, request.Theme = "icon", "broken"
				writeFixture(t, filepath.Join(dir, ".icons", "broken", "index.theme"), []byte("[unrelated]\n"))
			}
			notices := 0
			got, err := r.ResolveOptional(request, func(error) {
				notices++
				cancel()
			})
			if got != "" || !errors.Is(err, context.Canceled) || notices != 1 {
				t.Fatal("canceled optional request became text fallback", got, err, notices)
			}
		})
	}
}

func TestOptionalResolverKeepsNormalThemeInheritance(t *testing.T) {
	dir := t.TempDir()
	root := filepath.Join(dir, ".icons")
	for name, parent := range map[string]string{"child": "parent", "parent": ""} {
		writeFixture(t, filepath.Join(root, name, "index.theme"), []byte(
			"[Icon Theme]\nDirectories=16x16\nInherits="+parent+"\n[16x16]\nSize=16\nType=Fixed\n"))
	}
	data := fixturePNG(t, 16, color.NRGBA{A: 255})
	writeFixture(t, filepath.Join(root, "parent", "16x16", "good.png"), data)
	r := newTestRenderer(t, dir, []string{"png"})
	request := xdgmenu.IconRequest{Name: "good", Theme: "child", Size: 16, ScaleQuarters: 4}
	path, err := r.ResolveOptional(request, nil)
	if err != nil || path == "" {
		t.Fatal("optional consumer bypassed inherited theme", path, err)
	}
	if got, err := os.ReadFile(path); err != nil || !bytes.Equal(got, data) {
		t.Fatal("wrong inherited asset", err)
	}
}

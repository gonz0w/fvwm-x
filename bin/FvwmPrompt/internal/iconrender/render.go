// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package iconrender presents named icons for one bounded native-menu job.
// Source, finder and request cache have the same acquisition-generation lifetime.
package iconrender

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"image"
	"image/png"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

type Config struct {
	CacheDir   string
	Extensions []string
	// Zero budgets retain the existing menu defaults. Smaller isolated
	// consumers bound acquisition separately through xdgsource.NewLimited.
	CacheBytes, OutputBytes, SourcePixels int
	ExtraRoots                            []string
}

type Renderer struct {
	source       *xdgsource.Source
	finder       *xdgsource.IconFinder
	dir          string
	cache        map[xdgmenu.IconRequest]string
	cacheBytes   int
	outputBytes  int
	sourcePixels int
}

func New(source *xdgsource.Source, paths xdgmenu.SearchPaths, home string, config Config) (*Renderer, error) {
	if source == nil {
		return nil, errors.New("missing icon acquisition owner")
	}
	dir := config.CacheDir
	if strings.HasPrefix(dir, "~/") {
		dir = filepath.Join(home, dir[2:])
	}
	absolute, err := filepath.Abs(dir)
	if err != nil {
		return nil, err
	}
	finder := xdgsource.NewIconFinder(source, home, paths, "", 0)
	if len(config.ExtraRoots) > 4 {
		return nil, errors.New("too many private icon roots")
	}
	for i := len(config.ExtraRoots) - 1; i >= 0; i-- {
		root := config.ExtraRoots[i]
		if !filepath.IsAbs(root) || filepath.Clean(root) != root || len(root) > 1024 || !utf8.ValidString(root) || strings.ContainsAny(root, "\x00\r\n") {
			return nil, errors.New("invalid private icon root")
		}
		finder.Roots = append([]string{root}, finder.Roots...)
	}
	cacheBytes, outputBytes, pixels := config.CacheBytes, config.OutputBytes, config.SourcePixels
	if cacheBytes == 0 {
		cacheBytes = xdgsource.MaxJobBytes
	}
	if outputBytes == 0 {
		outputBytes = xdgsource.MaxJobBytes
	}
	if pixels == 0 {
		pixels = 64 * 1024 * 1024
	}
	if cacheBytes < 1 || cacheBytes > xdgsource.MaxJobBytes || outputBytes < 1 || outputBytes > xdgsource.MaxJobBytes || pixels < 1 || pixels > 64*1024*1024 {
		return nil, errors.New("invalid icon presentation budget")
	}
	finder.Extensions = nil
	for _, extension := range config.Extensions {
		if extension != "png" && extension != "svg" && extension != "xpm" {
			return nil, errors.New("unsupported icon format")
		}
		if len(finder.Extensions) >= 3 {
			return nil, errors.New("too many icon formats")
		}
		for _, previous := range finder.Extensions {
			if previous == extension {
				return nil, errors.New("duplicate icon format")
			}
		}
		finder.Extensions = append(finder.Extensions, extension)
	}
	return &Renderer{source: source, finder: finder, dir: absolute,
		cache: make(map[xdgmenu.IconRequest]string), cacheBytes: cacheBytes,
		outputBytes: outputBytes, sourcePixels: pixels}, nil
}

func (r *Renderer) Resolve(request xdgmenu.IconRequest) (string, error) {
	size, err := request.PhysicalSize()
	if err != nil {
		return "", err
	}
	if err := r.source.ContextError(); err != nil {
		return "", err
	}
	if request.State == "" {
		request.State = "normal"
	}
	if cached, ok := r.cache[request]; ok {
		return cached, nil
	}
	if len(r.finder.Extensions) == 0 {
		return "", nil
	}
	cost := len(request.Name) + len(request.Theme) + len(request.State) + len(request.Override) + len(request.Fallback)
	if len(r.cache) >= xdgmenu.MaxItems || cost > r.cacheBytes {
		return "", errors.New("icon request cache exceeds job limit")
	}
	path, err := r.finder.Resolve(request)
	if err != nil {
		return "", err
	}
	if path != "" {
		path, err = r.present(path, size)
		if err != nil {
			return "", err
		}
	}
	if strings.ContainsAny(path, "%*\r\n") {
		path = ""
	}
	cost += len(path)
	if cost > r.cacheBytes {
		return "", errors.New("icon request cache exceeds byte limit")
	}
	r.cacheBytes -= cost
	r.cache[request] = path
	return path, nil
}

// xpmSkip consumes only C whitespace and block comments. A limited consumer
// refuses other comment dialects, nested comments and NUL ambiguity rather
// than disagreeing with the native XPM reader about the first data string.
func xpmSkip(data []byte, offset int) (int, error) {
	for offset < len(data) {
		switch data[offset] {
		case ' ', '\t', '\r', '\n', '\v', '\f':
			offset++
		case '/':
			if offset+1 >= len(data) || data[offset+1] != '*' {
				return 0, errors.New("unsupported XPM comment syntax")
			}
			start := offset + 2
			end := bytes.Index(data[start:], []byte("*/"))
			if end < 0 {
				return 0, errors.New("unterminated XPM comment")
			}
			comment := data[start : start+end]
			if bytes.Contains(comment, []byte("/*")) || bytes.IndexByte(comment, 0) >= 0 {
				return 0, errors.New("ambiguous XPM comment")
			}
			offset = start + end + 2
		default:
			return offset, nil
		}
	}
	return offset, nil
}

// limitedXPMHeader locates the first actual XPM3 array string, not a quoted
// example inside a comment. The source already has an acquisition byte bound.
// This deliberately accepts a simple declaration subset, without interpreting
// C preprocessing, character literals, escapes or concatenated header strings.
func limitedXPMHeader(data []byte) ([]byte, error) {
	offset := 0
	declaration, bracket, array, assigned := false, false, false, false
	for {
		var err error
		offset, err = xpmSkip(data, offset)
		if err != nil {
			return nil, err
		}
		if offset == len(data) {
			return nil, errors.New("XPM icon has no geometry header")
		}
		c := data[offset]
		if assigned {
			if c != '{' {
				return nil, errors.New("unsupported XPM initializer")
			}
			offset++
			break
		}
		switch {
		case c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_':
			declaration = true
			offset++
		case c >= '0' && c <= '9', c == '*':
			offset++
		case c == '[' && !bracket && !array:
			bracket = true
			offset++
		case c == ']' && bracket:
			bracket, array = false, true
			offset++
		case c == '=' && declaration && array && !bracket:
			assigned = true
			offset++
		default:
			return nil, errors.New("unsupported XPM declaration")
		}
	}
	offset, err := xpmSkip(data, offset)
	if err != nil {
		return nil, err
	}
	if offset == len(data) || data[offset] != '"' {
		return nil, errors.New("XPM icon has no geometry string")
	}
	start := offset + 1
	for offset = start; offset < len(data) && offset-start <= 128; offset++ {
		c := data[offset]
		if c == '"' {
			next, err := xpmSkip(data, offset+1)
			if err != nil {
				return nil, err
			}
			if next == len(data) || data[next] != ',' && data[next] != '}' {
				return nil, errors.New("ambiguous XPM geometry string")
			}
			return data[start:offset], nil
		}
		if c == '\\' || c < 32 && c != '\t' || c > 126 {
			return nil, errors.New("unsupported XPM geometry string")
		}
	}
	return nil, errors.New("XPM icon geometry header exceeds limit")
}

func (r *Renderer) present(path string, size int) (string, error) {
	data, err := r.source.ReadImage(path)
	if err != nil {
		return "", err
	}
	extension := filepath.Ext(path)
	if extension == ".xpm" && r.sourcePixels < 64*1024*1024 {
		// Native XPM decoding still belongs to the shared picture path.
		// A smaller consumer first admits only a bounded raster header.
		header, err := limitedXPMHeader(data)
		if err != nil {
			return "", err
		}
		fields := strings.Fields(string(header))
		if len(fields) < 4 {
			return "", errors.New("invalid XPM icon geometry")
		}
		width, e1 := strconv.ParseUint(fields[0], 10, 32)
		height, e2 := strconv.ParseUint(fields[1], 10, 32)
		if e1 != nil || e2 != nil || width < 1 || height < 1 || width > 32768 || height > 32768 || width*height > uint64(r.sourcePixels) {
			return "", errors.New("XPM icon exceeds geometry limits")
		}
		colors, e3 := strconv.ParseUint(fields[2], 10, 32)
		characters, e4 := strconv.ParseUint(fields[3], 10, 32)
		// A small consumer also bounds native color-table and row work.
		// Retained cells cannot justify more color records than pixels.
		if e3 != nil || e4 != nil || colors < 1 || colors > width*height || characters < 1 || characters > 8 {
			return "", errors.New("XPM icon exceeds color or character limits")
		}
	}
	if extension == ".png" {
		data, err = r.scaledPNG(data, size)
		if err != nil {
			return "", err
		}
	}
	if err := r.source.ContextError(); err != nil {
		return "", err
	}
	// Every format gets a private content identity, including unscaled raster
	// images. Replacing a source in a fresh job cannot reuse the old core cache
	// name, and earlier menu generations keep their immutable bytes available.
	digest := sha256.Sum256(data)
	filename := filepath.Join(r.dir, ".fvwm-xdg-"+hex.EncodeToString(digest[:])+extension)
	if strings.ContainsAny(filename, "%*\r\n") {
		return "", nil
	}
	// Resampling can amplify a small source, so published bytes have their own
	// aggregate job budget. Charge every publication conservatively, including
	// repeated content, before creating a new visible artifact.
	if len(data) > r.outputBytes {
		return "", errors.New("icon publication exceeds job byte limit")
	}
	r.outputBytes -= len(data)
	if err := os.MkdirAll(r.dir, 0700); err != nil {
		return "", err
	}
	if err := xdgsource.PublishPrivate(filename, data); err != nil {
		return "", err
	}
	if extension == ".svg" {
		filename = fmt.Sprintf("%s:%dx%d", filename, size, size)
	}
	return filename, nil
}

func (r *Renderer) scaledPNG(data []byte, size int) ([]byte, error) {
	config, err := png.DecodeConfig(bytes.NewReader(data))
	if err != nil {
		return nil, err
	}
	if config.Width < 1 || config.Height < 1 || config.Width > 32768 || config.Height > 32768 || uint64(config.Width)*uint64(config.Height) > uint64(r.sourcePixels) {
		return nil, errors.New("PNG icon exceeds geometry limits")
	}
	if config.Width == size && config.Height == size {
		return data, nil
	}
	input, err := png.Decode(bytes.NewReader(data))
	if err != nil {
		return nil, err
	}
	output := image.NewNRGBA(image.Rect(0, 0, size, size))
	bounds := input.Bounds()
	// Preserve the existing deterministic conversion without an image toolkit.
	// Cancellation is cooperative; the parent worker owns the hard deadline.
	for y := 0; y < size; y++ {
		if err := r.source.ContextError(); err != nil {
			return nil, err
		}
		for x := 0; x < size; x++ {
			output.Set(x, y, input.At(bounds.Min.X+x*bounds.Dx()/size, bounds.Min.Y+y*bounds.Dy()/size))
		}
	}
	var encoded bytes.Buffer
	if err := png.Encode(&encoded, output); err != nil {
		return nil, err
	}
	return encoded.Bytes(), nil
}

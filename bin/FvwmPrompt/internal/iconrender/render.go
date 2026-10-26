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
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

type Config struct {
	CacheDir   string
	Extensions []string
}

type Renderer struct {
	source      *xdgsource.Source
	finder      *xdgsource.IconFinder
	dir         string
	cache       map[xdgmenu.IconRequest]string
	cacheBytes  int
	outputBytes int
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
		cache: make(map[xdgmenu.IconRequest]string), cacheBytes: xdgsource.MaxJobBytes,
		outputBytes: xdgsource.MaxJobBytes}, nil
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

func (r *Renderer) present(path string, size int) (string, error) {
	data, err := r.source.ReadImage(path)
	if err != nil {
		return "", err
	}
	extension := filepath.Ext(path)
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
	if config.Width < 1 || config.Height < 1 || config.Width > 32768 || config.Height > 32768 || uint64(config.Width)*uint64(config.Height) > 64*1024*1024 {
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

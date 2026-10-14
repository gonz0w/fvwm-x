// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

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

var svgEnabled = "true"
var pngEnabled = "true"
var xpmEnabled = "true"

type iconRenderer struct {
	finder                *xdgsource.IconFinder
	source                *xdgsource.Source
	dir, appIcon, dirIcon string
	size                  int
	cache                 map[string]string
}

func newIconRenderer(source *xdgsource.Source, paths xdgmenu.SearchPaths, env map[string]string, o options) (*iconRenderer, error) {
	dir := o.IconDir
	if strings.HasPrefix(dir, "~/") {
		dir = filepath.Join(env["HOME"], dir[2:])
	}
	absolute, err := filepath.Abs(dir)
	if err != nil {
		return nil, err
	}
	finder := xdgsource.NewIconFinder(source, env["HOME"], paths, o.Theme, o.Size)
	finder.Extensions = nil
	if pngEnabled == "true" {
		finder.Extensions = append(finder.Extensions, "png")
	}
	if svgEnabled == "true" {
		finder.Extensions = append(finder.Extensions, "svg")
	}
	if xpmEnabled == "true" {
		finder.Extensions = append(finder.Extensions, "xpm")
	}
	return &iconRenderer{finder: finder, source: source, dir: absolute, size: o.Size, appIcon: o.AppIcon, dirIcon: o.DirIcon, cache: make(map[string]string)}, nil
}
func (r *iconRenderer) resolve(name string, directory bool) (string, error) {
	key := name
	if directory {
		key += "\x00directory"
	}
	if cached, ok := r.cache[key]; ok {
		return cached, nil
	}
	path, err := r.finder.Find(name)
	if err != nil {
		return "", err
	}
	if path == "" {
		fallback := r.appIcon
		if directory {
			fallback = r.dirIcon
		}
		path, err = r.finder.Find(fallback)
		if err != nil {
			return "", err
		}
	}
	if path != "" {
		switch filepath.Ext(path) {
		case ".svg":
			path = fmt.Sprintf("%s:%dx%d", path, r.size, r.size)
		case ".png":
			path, err = r.scaledPNG(path)
			if err != nil {
				return "", err
			}
		}
	}
	if strings.ContainsAny(path, "%*\r\n") {
		path = ""
	}
	r.cache[key] = path
	return path, nil
}
func (r *iconRenderer) scaledPNG(path string) (string, error) {
	data, err := r.source.ReadImage(path)
	if err != nil {
		return "", err
	}
	config, err := png.DecodeConfig(bytes.NewReader(data))
	if err != nil {
		return "", err
	}
	if config.Width < 1 || config.Height < 1 || config.Width > 32768 || config.Height > 32768 || uint64(config.Width)*uint64(config.Height) > 64*1024*1024 {
		return "", errors.New("PNG icon exceeds geometry limits")
	}
	if config.Width == r.size && config.Height == r.size {
		return path, nil
	}
	if r.size < 1 || r.size > 4096 {
		return path, nil
	}
	input, err := png.Decode(bytes.NewReader(data))
	if err != nil {
		return "", err
	}
	output := image.NewNRGBA(image.Rect(0, 0, r.size, r.size))
	bounds := input.Bounds()
	// The approved simple conversion fallback is deterministic and needs no
	// image toolkit. Check cooperative cancellation once per output scanline.
	for y := 0; y < r.size; y++ {
		if err := r.source.ContextError(); err != nil {
			return "", err
		}
		for x := 0; x < r.size; x++ {
			output.Set(x, y, input.At(bounds.Min.X+x*bounds.Dx()/r.size, bounds.Min.Y+y*bounds.Dy()/r.size))
		}
	}
	var encoded bytes.Buffer
	if err = png.Encode(&encoded, output); err != nil {
		return "", err
	}
	if err = os.MkdirAll(r.dir, 0700); err != nil {
		return "", err
	}
	// A new content identity also gives the core image cache a new filename.
	// Existing menu generations keep their preceding converted image usable.
	digest := sha256.Sum256(data)
	hash := sha256.Sum256([]byte(fmt.Sprintf("%s:%d:%x", path, r.size, digest)))
	filename := filepath.Join(r.dir, ".fvwm-xdg-"+hex.EncodeToString(hash[:16])+".png")
	if err = xdgsource.PublishPrivate(filename, encoded.Bytes()); err != nil {
		return "", err
	}
	return filename, nil
}

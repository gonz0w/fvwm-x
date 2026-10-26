// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"path/filepath"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/iconrender"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

var svgEnabled = "true"
var pngEnabled = "true"
var xpmEnabled = "true"

type iconRenderer struct {
	renderer                     *iconrender.Renderer
	dir, theme, appIcon, dirIcon string
	size, scaleQuarters          int
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
	var extensions []string
	if pngEnabled == "true" {
		extensions = append(extensions, "png")
	}
	if svgEnabled == "true" {
		extensions = append(extensions, "svg")
	}
	if xpmEnabled == "true" {
		extensions = append(extensions, "xpm")
	}
	renderer, err := iconrender.New(source, paths, env["HOME"], iconrender.Config{CacheDir: absolute, Extensions: extensions})
	if err != nil {
		return nil, err
	}
	scale := o.iconScaleQuarters
	if scale == 0 {
		scale = 4
	}
	return &iconRenderer{renderer: renderer, dir: absolute, theme: o.Theme, size: o.Size,
		scaleQuarters: scale, appIcon: o.AppIcon, dirIcon: o.DirIcon}, nil
}

func (r *iconRenderer) resolve(name string, directory bool) (string, error) {
	// Legacy numeric size options remain accepted. Out-of-domain geometry keeps
	// its text item usable without submitting an invalid named-icon request.
	if r.size < 1 || r.size > 4096 || r.size*r.scaleQuarters > 4096*4 {
		return "", nil
	}
	fallback := r.appIcon
	if directory {
		fallback = r.dirIcon
	}
	return r.renderer.Resolve(xdgmenu.IconRequest{Name: name, Theme: r.theme,
		Size: r.size, ScaleQuarters: r.scaleQuarters, Fallback: fallback})
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"path/filepath"
	"strings"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/internal/pinboard"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

type options struct {
	state, socket, opener, browser, theme, formats, font string
	colorset                                             int
	create                                               bool
	pin                                                  pinboard.Pin
	layout                                               pinboard.Layout
}

type targetFlag struct {
	value []byte
	set   bool
}

func (f *targetFlag) String() string { return "" }
func (f *targetFlag) Set(value string) error {
	if f.set {
		return errors.New("only one --pin is accepted per data-only edit")
	}
	path, err := filemodel.ParseTarget(value)
	if err != nil {
		return err
	}
	f.value, f.set = path, true
	return nil
}

func parseOptions(args []string, home string, output io.Writer) (options, error) {
	var o options
	var target targetFlag
	var monitor string
	flags := flag.NewFlagSet("fvwm-pinboard", flag.ContinueOnError)
	flags.SetOutput(output)
	flags.StringVar(&o.state, "state", filepath.Join(home, ".local", "state", "fvwm-x", "pinboard", "state.json"), "private absolute pin state path")
	flags.StringVar(&o.socket, "socket", "", "existing FvwmMFL socket")
	flags.StringVar(&o.opener, "opener", "", "literal file-opener executable (default: xdg-open)")
	flags.StringVar(&o.browser, "browser", "", "literal folder-browser executable (default: adjacent fvwm-file-browser)")
	flags.StringVar(&o.theme, "icon-theme", "", "named icon theme")
	flags.StringVar(&o.formats, "icon-formats", "png,svg,xpm", "enabled native icon formats; empty retains labels")
	flags.StringVar(&o.font, "font", "", "native module font override")
	flags.IntVar(&o.colorset, "colorset", -1, "native module Colorset override")
	flags.Var(&target, "pin", "add one literal absolute path or local file URI to data only; does not start a canvas")
	flags.StringVar(&o.pin.Label, "label", "", "pin display label")
	flags.StringVar(&o.pin.Icon, "icon", "", "optional named icon override")
	flags.StringVar(&monitor, "monitor", "", "preferred literal monitor name")
	flags.IntVar(&o.pin.Column, "column", 0, "preferred nonnegative grid column")
	flags.IntVar(&o.pin.Row, "row", 0, "preferred nonnegative grid row")
	flags.IntVar(&o.layout.CellWidth, "cell-width", 0, "saved cell width in logical pixels (64..512)")
	flags.IntVar(&o.layout.CellHeight, "cell-height", 0, "saved cell height in logical pixels (64..512)")
	flags.IntVar(&o.layout.IconSize, "icon-size", 0, "saved named icon size in logical pixels (16..128)")
	if err := flags.Parse(args); err != nil {
		return o, err
	}
	if flags.NArg() != 0 {
		return o, errors.New("unexpected positional arguments; use --pin PATH")
	}
	if !filepath.IsAbs(o.state) || filepath.Clean(o.state) != o.state || len(o.state) > filemodel.MaxPath || strings.ContainsRune(o.state, 0) || !utf8.ValidString(o.state) {
		return o, errors.New("state requires a normalized absolute path")
	}
	if o.colorset < -1 || o.colorset > 65535 {
		return o, errors.New("invalid Colorset")
	}
	if len(o.font) > 256 || !utf8.ValidString(o.font) || strings.ContainsAny(o.font, "\x00\r\n\t\"\\$%*;") {
		return o, errors.New("invalid literal native font")
	}
	if o.theme != "" && !xdgmenu.ValidIconTheme(o.theme) {
		return o, errors.New("invalid icon theme")
	}
	if o.formats != "" {
		seen := map[string]bool{}
		for _, f := range strings.Split(o.formats, ",") {
			if seen[f] || f != "png" && f != "svg" && f != "xpm" {
				return o, errors.New("icon formats must be unique png,svg,xpm names")
			}
			seen[f] = true
		}
	}
	for _, value := range []string{o.socket, o.opener, o.browser} {
		if len(value) > filemodel.MaxPath || strings.ContainsRune(value, 0) || !utf8.ValidString(value) {
			return o, errors.New("configured executable/socket exceeds limit")
		}
	}
	metadata := false
	invalidLayout := false
	flags.Visit(func(f *flag.Flag) {
		switch f.Name {
		case "label", "icon", "monitor", "column", "row":
			metadata = true
		case "cell-width":
			invalidLayout = invalidLayout || o.layout.CellWidth < 64 || o.layout.CellWidth > 512
		case "cell-height":
			invalidLayout = invalidLayout || o.layout.CellHeight < 64 || o.layout.CellHeight > 512
		case "icon-size":
			invalidLayout = invalidLayout || o.layout.IconSize < 16 || o.layout.IconSize > 128
		}
	})
	if !target.set && metadata {
		return o, errors.New("pin metadata options require --pin")
	}
	if invalidLayout {
		return o, errors.New("invalid saved layout bounds")
	}
	o.create = target.set
	if o.create {
		o.pin.Target = target.value
		o.pin.Monitor = []byte(monitor)
		if o.pin.Label == "" {
			o.pin.Label = pinboard.DefaultLabel(target.value)
		}
		// Validate without consuming a random ID or doing filesystem work.
		p := o.pin
		p.ID = strings.Repeat("0", 32)
		s := pinboard.Empty()
		s.Pins = []pinboard.Pin{p}
		if err := s.Validate(); err != nil {
			return o, err
		}
	}
	return o, nil
}

func runDataEdit(ctx context.Context, executable string, o options, output io.Writer) error {
	id, err := pinboard.NewID()
	if err != nil {
		return err
	}
	o.pin.ID = id
	result := startWorker(ctx, executable, workRequest{Operation: "pin", Path: o.state, Pin: &o.pin, Layout: &o.layout})
	if result.Status != "saved" || result.State == nil {
		if result.Status == "cancelled" {
			return errors.New("Data-only save outcome unknown; inspect/reload saved data before retrying")
		}
		if result.Error == "" {
			return errors.New("Data-only pin edit returned no committed state")
		}
		return errors.New(result.Error)
	}
	if err := result.State.Validate(); err != nil {
		return err
	}
	if i := result.State.Index(id); i < 0 || string(result.State.Pins[i].Target) != string(o.pin.Target) {
		return errors.New("Data-only pin edit returned an unexpected reference")
	}
	_, err = fmt.Fprintf(output, "Pin %s saved at revision %d. Reload an active pinboard to observe it.\n", id, result.State.Revision)
	return err
}

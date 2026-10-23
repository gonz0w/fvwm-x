// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"errors"
	"flag"
	"io"
)

func parseOptions(args []string, home string, output io.Writer) (options, error) {
	var o options
	flags := flag.NewFlagSet("fvwm-file-browser", flag.ContinueOnError)
	flags.SetOutput(output)
	flags.StringVar(&o.dir, "dir", home, "initial absolute local directory or local file URI")
	var reveal, properties targetFlags
	flags.Var(&o.actions, "action", "configured LABEL=EXECUTABLE; repeat up to four")
	flags.Var(&reveal, "reveal", "select literal local target in its parent; repeat")
	flags.Var(&properties, "properties", "show observed properties for local targets; repeat")
	flags.StringVar(&o.view, "view", "list", "initial collection view: list or grid")
	flags.StringVar(&o.watch, "watch", "off", "directory watching: off or auto")
	flags.StringVar(&o.socket, "socket", "", "FvwmMFL socket (default: private discovery)")
	flags.StringVar(&o.opener, "opener", "", "literal opener executable; default: xdg-open")
	flags.StringVar(&o.terminal, "terminal", "", "literal terminal executable; default: x-terminal-emulator or xterm")
	flags.BoolVar(&o.hidden, "hidden", false, "include hidden entries")
	flags.BoolVar(&o.inspect, "inspect", false, "emit bounded schema-1 directory JSON without X")
	flags.BoolVar(&o.reuse, "reuse", false, "hand off to an idle matching browser; otherwise start an independent view")
	if err := flags.Parse(args); err != nil {
		return o, err
	}
	dirSet, custom := false, false
	flags.Visit(func(f *flag.Flag) {
		if f.Name == "dir" {
			dirSet = true
		}
		switch f.Name {
		case "view", "watch", "opener", "terminal", "action", "hidden", "inspect":
			custom = true
		}
	})
	if flags.NArg() > 1 {
		return o, errors.New("expected at most one local folder argument")
	}
	if flags.NArg() == 1 {
		if dirSet || len(reveal)+len(properties) != 0 {
			return o, errors.New("folder argument cannot be combined with --dir/--reveal/--properties")
		}
		o.dir, dirSet = flags.Arg(0), true
	}
	if o.reuse && custom {
		return o, errors.New("--reuse accepts only folder/target/socket options; configure an independent browser first")
	}
	return o, prepareOptions(&o, dirSet, reveal, properties)
}

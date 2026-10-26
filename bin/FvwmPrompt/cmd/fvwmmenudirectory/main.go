// Copyright (c) 1999-2009 Mikhael Goikhman
// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

var version = "unknown"

type options struct {
	name, title, item, dir, home, userDir, program           string
	titleIcon, dirIcon, fileIcon, appIcon                    string
	xterm, execTitle, execFile                               string
	execApp, commandTitle, commandFile, commandApp           *string
	all, links, reuse, wmIcons, checkSubdirs, memoryForSpeed bool
	order                                                    int64
	specialDirs                                              *string
	menuStyle, funcName                                      string
	iconTheme, iconDir                                       string
	iconSize, iconScaleQuarters                              int
	pictures                                                 [4]string
}

func valueOr(key, fallback string) string {
	value := os.Getenv(key)
	if value == "" || value == "0" {
		return fallback
	}
	return value
}

func defaults(program string) options {
	home := valueOr("HOME", "/tmp")
	userDir := valueOr("FVWM_USERDIR", home+"/.fvwm")
	if info, err := os.Stat(userDir); err != nil || !info.IsDir() {
		userDir = home
	}
	return options{name: "MenuBrowser", title: "%*-40p", item: "%n", dir: home, home: home,
		userDir: userDir, program: program, xterm: "xterm -e", execTitle: valueOr("SHELL", "/bin/sh"),
		execFile: valueOr("EDITOR", "vi"), order: 5, funcName: "FuncFvwmMenuDirectory", iconSize: 24, iconScaleQuarters: 4, iconDir: "~/.fvwm/icons"}
}

func parse(args []string, out, diagnostic io.Writer, program string) (options, bool, int) {
	o := defaults(program)
	stringsOptions := map[string]*string{"name": &o.name, "title": &o.title, "item": &o.item,
		"icon-title": &o.titleIcon, "icon-dir": &o.dirIcon, "icon-file": &o.fileIcon, "icon-app": &o.appIcon,
		"dir": &o.dir, "xterm": &o.xterm, "exec-title": &o.execTitle, "exec-file": &o.execFile,
		"menu-style": &o.menuStyle, "func-name": &o.funcName, "icon-theme": &o.iconTheme, "mini-icon-dir": &o.iconDir}
	pointers := map[string]**string{"exec-app": &o.execApp, "command-title": &o.commandTitle,
		"command-file": &o.commandFile, "command-app": &o.commandApp, "special-dirs": &o.specialDirs}
	booleans := map[string]*bool{"all": &o.all, "links": &o.links, "reuse": &o.reuse,
		"check-subdirs": &o.checkSubdirs, "memory-for-speed": &o.memoryForSpeed, "wm-icons": &o.wmIcons}
	names := []string{"help", "version", "order", "icon-size", "icon-scale"}
	for name := range stringsOptions {
		names = append(names, name)
	}
	for name := range pointers {
		names = append(names, name)
	}
	for name := range booleans {
		names = append(names, name)
	}
	sort.Strings(names)
	invalid := false
	for i := 0; i < len(args); i++ {
		a := args[i]
		if a == "--" {
			invalid = invalid || i+1 != len(args)
			break
		}
		if a == "-" || !strings.HasPrefix(a, "-") {
			invalid = true
			continue
		}
		key, v, supplied := strings.Cut(strings.TrimPrefix(strings.TrimPrefix(a, "-"), "-"), "=")
		key = strings.ToLower(key)
		if key == "h" || key == "?" {
			key = "help"
		}
		if key == "v" {
			key = "version"
		}
		// Keep the documented title-icon abbreviation after adding theme.
		if key == "icon-t" {
			key = "icon-title"
		}
		negative := false
		lookup := key
		if strings.HasPrefix(key, "no") {
			lookup = strings.TrimPrefix(key, "no")
			lookup = strings.TrimPrefix(lookup, "-")
			negative = true
		}
		var matches []string
		resolved := ""
		for _, name := range names {
			if negative && (booleans[name] == nil || name == "wm-icons") {
				continue
			}
			if lookup == name {
				resolved = name
				break
			}
			if strings.HasPrefix(name, lookup) {
				matches = append(matches, name)
			}
		}
		if resolved == "" && len(matches) == 1 {
			resolved = matches[0]
		}
		if resolved == "" {
			if len(matches) == 0 {
				fmt.Fprintf(diagnostic, "Unknown option: %s\n", key)
			} else {
				fmt.Fprintf(diagnostic, "Option %s is ambiguous (%s)\n", key, strings.Join(matches, ", "))
			}
			invalid = true
			continue
		}
		boolean := booleans[resolved] != nil || resolved == "help" || resolved == "version"
		if boolean && supplied {
			fmt.Fprintf(diagnostic, "Option %s does not take an argument\n", key)
			invalid = true
			continue
		}
		if !boolean && supplied && v == "" && resolved != "exec-app" && resolved != "special-dirs" {
			fmt.Fprintf(diagnostic, "Option %s requires an argument\n", key)
			invalid = true
			continue
		}
		if !boolean && !supplied {
			optional := resolved == "exec-app" || resolved == "special-dirs"
			if i+1 < len(args) && (!optional || !strings.HasPrefix(args[i+1], "-")) {
				i++
				v = args[i]
			} else if !optional {
				fmt.Fprintf(diagnostic, "Option %s requires an argument\n", key)
				invalid = true
				continue
			}
		}
		switch resolved {
		case "help":
			help(out, program, o)
			return o, true, 0
		case "version":
			fmt.Fprintln(out, version)
			return o, true, 0
		case "icon-size":
			n, err := strconv.Atoi(v)
			if err != nil || n < 1 || n > 4096 {
				fmt.Fprintln(diagnostic, "--icon-size must be between 1 and 4096")
				invalid = true
			} else {
				o.iconSize = n
			}
		case "icon-scale":
			n, err := xdgmenu.ParseIconScale(v)
			if err != nil {
				fmt.Fprintln(diagnostic, err)
				invalid = true
			} else {
				o.iconScaleQuarters = n
			}
		case "order":
			n, err := strconv.ParseInt(v, 10, 64)
			if err != nil {
				fmt.Fprintf(diagnostic, "Value %q invalid for option %s (number expected)\n", v, key)
				invalid = true
			} else {
				o.order = n
			}
		default:
			if p := booleans[resolved]; p != nil {
				*p = !negative
			} else if p := stringsOptions[resolved]; p != nil {
				if (resolved == "icon-theme" || resolved == "mini-icon-dir") && v == "" {
					fmt.Fprintf(diagnostic, "Option %s requires a nonempty argument\n", key)
					invalid = true
				}
				*p = v
			} else {
				copy := v
				*pointers[resolved] = &copy
			}
		}
	}
	if o.iconSize*o.iconScaleQuarters > 4096*4 {
		fmt.Fprintln(diagnostic, "scaled icon extent exceeds 4096 pixels")
		invalid = true
	}
	for _, value := range []string{o.iconTheme, o.iconDir} {
		if !validIconValue(value) {
			fmt.Fprintln(diagnostic, "invalid icon theme or cache directory")
			invalid = true
		}
	}
	if o.iconTheme != "" {
		for _, value := range []string{o.titleIcon, o.dirIcon, o.fileIcon, o.appIcon} {
			_, err := (xdgmenu.IconRequest{Name: "folder", Theme: o.iconTheme, Size: o.iconSize,
				ScaleQuarters: o.iconScaleQuarters, Override: value}).PhysicalSize()
			if err != nil {
				fmt.Fprintln(diagnostic, err)
				invalid = true
				break
			}
		}
	}
	if invalid {
		fmt.Fprintf(diagnostic, "Try '%s --help' for more information.\n", program)
		return o, true, 255
	}
	return o, false, 0
}

func run(args []string, out, diagnostic io.Writer, program string) int {
	if len(args) > 0 && args[0] == "--_icons" {
		return iconWorker(args[1:], out, diagnostic, program)
	}

	if len(args) > 0 && args[0] == "--fvwm-remember" {
		if len(args) != 4 || !singleLine(args[3]) {
			fmt.Fprintln(diagnostic, "fvwm-menu-directory: invalid navigation request")
			return 1
		}
		if err := remember(args[1], args[2]); err != nil {
			fmt.Fprintln(diagnostic, "fvwm-menu-directory:", err)
			return 1
		}
		_, err := fmt.Fprintf(out, "Menu \"%s\" WarpTitle\n", escapeFVWM(args[3]))
		if err != nil {
			fmt.Fprintln(diagnostic, "fvwm-menu-directory:", err)
			return 1
		}
		return 0
	}
	o, done, status := parse(args, out, diagnostic, program)
	if done {
		return status
	}
	if !o.reuse {
		o.name = o.dir
	}
	if o.dir == "~" || strings.HasPrefix(o.dir, "~/") {
		o.dir = o.home + strings.TrimPrefix(o.dir, "~")
	}
	if o.reuse {
		previous, err := readRemembered(o.userDir)
		if err != nil {
			fmt.Fprintln(diagnostic, "fvwm-menu-directory:", err)
			return 1
		}
		if previous != "" {
			o.dir = previous
		}
	}
	info, err := os.Stat(o.dir)
	if err != nil || !info.IsDir() {
		o.dir = o.home
	}
	if !o.links {
		if info, err := os.Lstat(o.dir); err == nil && info.Mode()&os.ModeSymlink != 0 {
			return 255
		}
	}
	// Keep lexical menu names/paths while resolving filesystem operations once.
	base, err := filepath.Abs(o.dir)
	if err != nil {
		fmt.Fprintln(diagnostic, "fvwm-menu-directory:", err)
		return 1
	}
	if o.iconTheme != "" {
		pictures, iconErr := acquireIcons(o)
		if iconErr != nil {
			fmt.Fprintln(diagnostic, "fvwm-menu-directory: icons:", iconErr)
		}
		o.pictures = pictures
	}
	entries, err := listing(base, o)
	if err == nil {
		writer := bufio.NewWriter(out)
		err = menu(writer, o, entries)
		if err == nil {
			err = writer.Flush()
		}
	}
	if err != nil {
		fmt.Fprintln(diagnostic, "fvwm-menu-directory:", err)
		return 1
	}
	return 0
}

func main() { os.Exit(run(os.Args[1:], os.Stdout, os.Stderr, os.Args[0])) }

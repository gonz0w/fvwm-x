// Copyright (c) 1999-2009 Mikhael Goikhman
// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bufio"
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"
)

var version = "unknown"

const probeLimit = 1024 * 1024

var specifier = regexp.MustCompile(`%(-?\d+)?(\*(-?)(\d+))?([ndD])`)
var modeName = regexp.MustCompile(`^[A-Za-z0-9_][A-Za-z0-9_-]*$`)

type options struct {
	name, title, item, titleIcon, itemIcon string
	wmIcons, specialFirst                  bool
	params                                 []string
}

type mode struct {
	name, description string
	special           bool
}

func defaults() options {
	return options{name: "XLockMenu", title: "XLock Modes", item: `%n\t(%d)`}
}

func help(out io.Writer, program string, o options) {
	fmt.Fprintf(out, "A small utility which builds xlock menu for fvwm.\n\nUsage: %s [OPTIONS] [-- XLOCK-OPTIONS]\nOptions:\n", program)
	fmt.Fprint(out, "\t--help           show this help and exit\n\t--version        show the version and exit\n")
	fmt.Fprintf(out, "\t--name=NAME      menu name,  default is '%s'\n\t--title=NAME     menu title, default is '%s'\n\t--item=NAME      menu item format, default is '%s'\n", o.name, o.title, o.item)
	fmt.Fprint(out, "\t--icon-title=XPM menu title icon, default is no\n\t--icon-item=XPM  menu item  icon, default is no\n\t--wm-icons       define icon names to use with wm-icons\n\t--special-first  put special modes first\nShort options are ok if not ambiguous: -h, -t.\n\nSome useful xlock(1) options, 'xlock -h' for more:\n\t-delay usecs     delay between batches of animations\n\t-nolock          screensaver, don't lock the display\n\t-inwindow        run in window as opposite to -inroot\n\t-sound           turn on sound if enabled\n\t-nice level      decrease the process priority (0 .. 19)\n")
}

func parse(args []string, out, diagnostic io.Writer, program string) (options, bool, int) {
	o := defaults()
	names := []string{"help", "version", "name", "title", "item", "icon-title", "icon-item", "wm-icons", "special-first"}
	invalid := false
	for i := 0; i < len(args); i++ {
		a := args[i]
		if a == "--" {
			o.params = append(o.params, args[i+1:]...)
			break
		}
		if a == "-" || !strings.HasPrefix(a, "-") {
			o.params = append(o.params, a)
			continue
		}
		key, value, supplied := strings.Cut(strings.TrimPrefix(strings.TrimPrefix(a, "-"), "-"), "=")
		key = strings.ToLower(key)
		if key == "?" || key == "h" {
			key = "help"
		} else if key == "v" {
			key = "version"
		}
		resolved := ""
		var matches []string
		for _, name := range names {
			if key == name {
				resolved = name
				break
			}
			if strings.HasPrefix(name, key) {
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
				sort.Strings(matches)
				fmt.Fprintf(diagnostic, "Option %s is ambiguous (%s)\n",
					key, strings.Join(matches, ", "))
			}
			invalid = true
			continue
		}
		boolean := resolved == "help" || resolved == "version" || resolved == "wm-icons" || resolved == "special-first"
		if boolean && supplied {
			fmt.Fprintf(diagnostic, "Option %s does not take an argument\n", key)
			invalid = true
			continue
		}
		if !boolean && !supplied {
			if i+1 == len(args) {
				fmt.Fprintf(diagnostic, "Option %s requires an argument\n", key)
				invalid = true
				continue
			}
			i++
			value = args[i]
		}
		switch resolved {
		case "help":
			help(out, program, o)
			return o, true, 0
		case "version":
			fmt.Fprintln(out, version)
			return o, true, 0
		case "name":
			o.name = value
		case "title":
			o.title = value
		case "item":
			o.item = value
		case "icon-title":
			o.titleIcon = value
		case "icon-item":
			o.itemIcon = value
		case "wm-icons":
			o.wmIcons = true
		case "special-first":
			o.specialFirst = true
		}
	}
	if invalid {
		fmt.Fprintf(diagnostic, "Try '%s --help' for more information.\n", program)
		return o, true, 255
	}
	if o.wmIcons && (o.itemIcon == "" || o.itemIcon == "0") {
		o.itemIcon = "menu/lock.xpm"
	}
	o.item = strings.ReplaceAll(o.item, `\t`, "\t")
	return o, false, 0
}

type probeBuffer struct {
	data   bytes.Buffer
	cancel context.CancelFunc
	full   bool
}

func (b *probeBuffer) Write(data []byte) (int, error) {
	if len(data) > probeLimit-b.data.Len() {
		b.full = true
		b.cancel()
		return 0, errors.New("xlock help exceeds 1 MiB")
	}
	return b.data.Write(data)
}

func probe(ctx context.Context) ([]byte, error) {
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()
	buffer := probeBuffer{cancel: cancel}
	cmd := exec.CommandContext(ctx, "xlock", "-display", "NONE", "-help")
	// The same comparable writer serializes stdout/stderr copying in os/exec.
	cmd.Stdout, cmd.Stderr = &buffer, &buffer
	cmd.WaitDelay = 250 * time.Millisecond
	err := cmd.Run()
	if buffer.full {
		return nil, errors.New("xlock help exceeds 1 MiB")
	}
	if ctx.Err() != nil {
		return nil, ctx.Err()
	}
	var exit *exec.ExitError
	if err != nil && !errors.As(err, &exit) {
		return nil, fmt.Errorf("could not read xlock help: %w", err)
	}
	// xlock variants can return nonzero for -help; parse their actual text.
	return buffer.data.Bytes(), nil
}

func modes(data []byte) []mode {
	var result []mode
	started, special := false, false
	scanner := bufio.NewScanner(bytes.NewReader(data))
	scanner.Buffer(make([]byte, 4096), probeLimit+1)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.Contains(line, "where mode is one of:") {
			started = true
			continue
		}
		if !started || line == "" || (line[0] != ' ' && line[0] != '\t') {
			continue
		}
		line = strings.TrimLeft(line, " \t")
		end := strings.IndexAny(line, " \t")
		if end <= 0 || !modeName.MatchString(line[:end]) {
			continue
		}
		name := line[:end]
		description := strings.TrimLeft(line[end:], " \t")
		if name == "blank" {
			special = true
		}
		result = append(result, mode{name, description, special})
	}
	return result
}

func padding(out io.Writer, count uint64) error {
	const spaces = "                                                                "
	for count != 0 {
		n := uint64(len(spaces))
		if count < n {
			n = count
		}
		if _, err := io.WriteString(out, spaces[:n]); err != nil {
			return err
		}
		count -= n
	}
	return nil
}

func formatItem(out io.Writer, format string, m mode) error {
	position := 0
	for _, match := range specifier.FindAllStringSubmatchIndex(format, -1) {
		if _, err := io.WriteString(out, format[position:match[0]]); err != nil {
			return err
		}
		value := m.name
		switch format[match[10]:match[11]] {
		case "d":
			value = m.description
		case "D":
			value = strings.TrimPrefix(m.description, "Shows ")
		}
		if match[8] >= 0 {
			length, err := strconv.ParseUint(format[match[8]:match[9]], 10, 64)
			if err != nil {
				return errors.New("invalid truncation width")
			}
			if length > 3 && length <= uint64(len(value)) {
				if match[6] != match[7] {
					value = "..." + value[len(value)-int(length)+3:]
				} else {
					value = value[:int(length)-3] + "..."
				}
			}
		}
		left, width := false, uint64(0)
		if match[2] >= 0 {
			size := format[match[2]:match[3]]
			left = strings.HasPrefix(size, "-")
			var err error
			width, err = strconv.ParseUint(strings.TrimPrefix(size, "-"), 10, 64)
			if err != nil {
				return errors.New("invalid field width")
			}
		}
		pad := uint64(0)
		if width > uint64(len(value)) {
			pad = width - uint64(len(value))
		}
		if !left {
			if err := padding(out, pad); err != nil {
				return err
			}
		}
		if _, err := io.WriteString(labelWriter{out}, value); err != nil {
			return err
		}
		if left {
			if err := padding(out, pad); err != nil {
				return err
			}
		}
		position = match[1]
	}
	_, err := io.WriteString(out, format[position:])
	return err
}

type labelWriter struct{ out io.Writer }

func (w labelWriter) Write(data []byte) (int, error) {
	for _, c := range data {
		if c == '\\' || c == '"' {
			if _, err := w.out.Write([]byte{'\\'}); err != nil {
				return 0, err
			}
		}
		if c == '\r' || c == '\n' {
			c = ' '
		}
		if _, err := w.out.Write([]byte{c}); err != nil {
			return 0, err
		}
	}
	return len(data), nil
}

func menu(out io.Writer, o options, entries []mode) error {
	icon := func(value string) string {
		if value == "" || value == "0" {
			return ""
		}
		return "%" + value + "%"
	}
	if _, err := fmt.Fprintf(out, "DestroyMenu %s\nAddToMenu %s \"%s%s\" Title\n", o.name, o.name, icon(o.titleIcon), o.title); err != nil {
		return err
	}
	for group := 0; group < 2; group++ {
		special := (group == 0) == o.specialFirst
		for _, m := range entries {
			if m.special != special {
				continue
			}
			if _, err := fmt.Fprintf(out, "+ \"%s", icon(o.itemIcon)); err != nil {
				return err
			}
			if err := formatItem(out, o.item, m); err != nil {
				return err
			}
			params := ""
			if len(o.params) != 0 {
				params = " " + strings.Join(o.params, " ")
			}
			if _, err := fmt.Fprintf(out, "\" Exec xlock%s -mode %s\n", params, m.name); err != nil {
				return err
			}
		}
		if group == 0 {
			if _, err := io.WriteString(out, "+ \"\" Nop\n"); err != nil {
				return err
			}
		}
	}
	return nil
}

func main() {
	o, done, status := parse(os.Args[1:], os.Stdout, os.Stderr, os.Args[0])
	if done {
		os.Exit(status)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	data, err := probe(ctx)
	cancel()
	if err == nil {
		writer := bufio.NewWriter(os.Stdout)
		err = menu(writer, o, modes(data))
		if err == nil {
			err = writer.Flush()
		}
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, "fvwm-menu-xlock:", err)
		os.Exit(1)
	}
}

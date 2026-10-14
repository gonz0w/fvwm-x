// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"syscall"
)

const listingLimit = 100000

var fieldPattern = regexp.MustCompile(`%(-?\d+)?(\*(-?)(\d+))?([nNspdDtTp])`)
var specialPattern = regexp.MustCompile(`([\d,]+)(-?)`)

type field struct {
	literal, key string
	width, limit uint64
	left, tail   bool
}

type entry struct {
	name, path                  string
	info                        os.FileInfo
	directory, link, executable bool
}

func plan(format, sized, plain string) ([]field, error) {
	format = strings.ReplaceAll(format, `\t`, "\t")
	var result []field
	position := 0
	for _, m := range fieldPattern.FindAllStringSubmatchIndex(format, -1) {
		key := format[m[10]:m[11]]
		if !strings.Contains(sized, key) && !(strings.Contains(plain, key) && m[2] < 0 && m[4] < 0) {
			continue
		}
		f := field{literal: format[position:m[0]], key: key}
		if m[2] >= 0 {
			width := format[m[2]:m[3]]
			f.left = strings.HasPrefix(width, "-")
			n, err := strconv.ParseUint(strings.TrimPrefix(width, "-"), 10, 64)
			if err != nil {
				return nil, errors.New("invalid field width")
			}
			f.width = n
		}
		if m[8] >= 0 {
			n, err := strconv.ParseUint(format[m[8]:m[9]], 10, 64)
			if err != nil {
				return nil, errors.New("invalid truncation width")
			}
			f.limit = n
			f.tail = m[6] != m[7]
		}
		result = append(result, f)
		position = m[1]
	}
	return append(result, field{literal: format[position:]}), nil
}

func pad(out io.Writer, n uint64) error {
	const spaces = "                                                                "
	for n > 0 {
		take := uint64(len(spaces))
		if n < take {
			take = n
		}
		if _, err := io.WriteString(out, spaces[:take]); err != nil {
			return err
		}
		n -= take
	}
	return nil
}

func formatted(out io.Writer, fields []field, values map[string]string, title bool) error {
	for _, f := range fields {
		if _, err := io.WriteString(out, f.literal); err != nil {
			return err
		}
		if f.key == "" {
			continue
		}
		value := values[f.key]
		if f.limit > 3 && f.limit <= uint64(len(value)) {
			if f.tail {
				value = "..." + value[len(value)-int(f.limit)+3:]
			} else {
				value = value[:int(f.limit)-3] + "..."
			}
		}
		n := uint64(0)
		if f.width > uint64(len(value)) {
			n = f.width - uint64(len(value))
		}
		if !f.left {
			if err := pad(out, n); err != nil {
				return err
			}
		}
		writer := out
		if title {
			writer = labelWriter{out}
		}
		if _, err := io.WriteString(writer, value); err != nil {
			return err
		}
		if f.left {
			if err := pad(out, n); err != nil {
				return err
			}
		}
	}
	return nil
}

func escapeFVWM(value string) string {
	return strings.NewReplacer(`\`, `\\`, `"`, `\"`, "$", "$$").Replace(value)
}
func escapeMenu(value string) string {
	return strings.NewReplacer("^", "^^", "@", "@@").Replace(escapeFVWM(value))
}
func escapeFile(value string) string {
	return strings.NewReplacer(`\`, `\\`, `"`, `\"`, "$", `\$$`, "`", "\\`").Replace(value)
}
func singleLine(value string) bool { return !strings.ContainsAny(value, "\x00\r\n") }
func shellArg(value string) string { return "'" + strings.ReplaceAll(value, "'", `'\''`) + "'" }

type labelWriter struct{ out io.Writer }

func (w labelWriter) Write(data []byte) (int, error) {
	start := 0
	for i, c := range data {
		replacement := ""
		switch c {
		case '\\':
			replacement = `\\`
		case '"':
			replacement = `\"`
		case '$':
			replacement = "$$"
		case '%':
			replacement = "%%"
		case '&':
			replacement = "&&"
		case '*':
			replacement = "**"
		case '\n', '\r':
			replacement = " "
		}
		if replacement == "" {
			continue
		}
		if _, err := w.out.Write(data[start:i]); err != nil {
			return start, err
		}
		if _, err := io.WriteString(w.out, replacement); err != nil {
			return i, err
		}
		start = i + 1
	}
	_, err := w.out.Write(data[start:])
	if err != nil {
		return start, err
	}
	return len(data), nil
}

func listing(base string, o options) ([]entry, error) {
	return listingBounded(base, o, listingLimit)
}

func listingBounded(base string, o options, limit int) ([]entry, error) {
	dir, err := os.Open(base)
	if err != nil {
		return nil, err
	}
	defer dir.Close()
	var result []entry
	scanned := 0
	for {
		batch, err := dir.ReadDir(128)
		if err != nil && err != io.EOF {
			return nil, err
		}
		for _, de := range batch {
			scanned++
			if scanned > limit {
				return nil, fmt.Errorf("directory exceeds %d entries", limit)
			}
			name := de.Name()
			if !o.all && strings.HasPrefix(name, ".") {
				continue
			}
			info, e := de.Info()
			if e != nil {
				return nil, e
			}
			link := info.Mode()&os.ModeSymlink != 0
			full := filepath.Join(base, name)
			if link {
				target, e := os.Stat(full)
				if e == nil {
					info = target
				} else if !os.IsNotExist(e) {
					return nil, e
				}
			}
			result = append(result, entry{name: name, path: collapseSlashes(o.dir + "/" + name), info: info, directory: info.IsDir(), link: link,
				executable: info.Mode().IsRegular() && syscall.Access(full, 1) == nil})
		}
		if err == io.EOF {
			break
		}
	}
	// Magnitude is checked by comparisons, without negating INT64_MIN.
	order := o.order
	if order < 0 && order >= -6 {
		order = -order
	}
	if order >= 2 && order <= 6 {
		sort.SliceStable(result, func(i, j int) bool {
			a, b := result[i], result[j]
			if order != 4 && a.directory != b.directory {
				if order == 2 || order == 5 {
					return a.directory
				}
				return !a.directory
			}
			return order >= 4 && a.name < b.name
		})
	}
	if o.order < 0 {
		for i, j := 0, len(result)-1; i < j; i, j = i+1, j-1 {
			result[i], result[j] = result[j], result[i]
		}
	}
	return result, nil
}

func collapseSlashes(path string) string {
	var out strings.Builder
	slash := false
	for _, c := range []byte(path) {
		if c != '/' || !slash {
			out.WriteByte(c)
		}
		slash = c == '/'
	}
	return out.String()
}

func values(e entry, fields []field) map[string]string {
	result := map[string]string{"n": e.name, "N": e.path}
	types := false
	for _, f := range fields {
		switch f.key {
		case "t", "T":
			types = true
		case "s":
			result["s"] = strconv.FormatInt(e.info.Size(), 10)
		case "d":
			result["d"] = e.info.ModTime().Local().Format("2006-01-02 15:04:05")
		case "D":
			result["D"] = e.info.ModTime().Local().Format("2006-01-02")
		}
	}
	if !types {
		return result
	}
	typeName, typeCode := "File", "F"
	m := e.info.Mode()
	switch {
	case m&os.ModeNamedPipe != 0:
		typeName, typeCode = "Pipe", "P"
	case m&os.ModeCharDevice != 0:
		typeName, typeCode = "Char", "C"
	case m&os.ModeDevice != 0:
		typeName, typeCode = "Blck", "B"
	case e.link:
		typeName, typeCode = "Link", "L"
	case m&os.ModeSocket != 0:
		typeName, typeCode = "Sock", "S"
	case e.directory:
		typeName, typeCode = "Dir ", "D"
	}
	result["t"], result["T"] = typeName, typeCode
	return result
}

func execCommand(value *string, xterm string) *string {
	if value == nil || *value == "-" {
		return nil
	}
	v := *value
	if strings.HasPrefix(v, "^") {
		v = v[1:]
	} else {
		v = xterm + " " + v
	}
	return &v
}
func command(value *string, fallback string) string {
	if value == nil || *value == "-" {
		return fallback
	}
	return *value
}
func icon(value string) string {
	if value == "" || value == "0" {
		return ""
	}
	return "%" + value + "%"
}

func menu(out io.Writer, o options, entries []entry) error {
	if !singleLine(o.name) || !singleLine(o.dir) {
		return errors.New("directory/menu name contains a line break or NUL")
	}
	item, err := plan(o.item, "nNs", "dDtT")
	if err != nil {
		return err
	}
	title, err := plan(o.title, "dp", "")
	if err != nil {
		return err
	}
	if o.wmIcons {
		for _, pair := range []struct {
			value    *string
			fallback string
		}{{&o.titleIcon, "menu/folder-open.xpm"}, {&o.dirIcon, "menu/folder.xpm"}, {&o.fileIcon, "menu/file.xpm"}, {&o.appIcon, "menu/utility.xpm"}} {
			if *pair.value == "" || *pair.value == "0" {
				*pair.value = pair.fallback
			}
		}
	}
	titleAct, fileAct := "Nop", "Nop"
	if c := execCommand(&o.execTitle, o.xterm); c != nil {
		titleAct = `Exec cd "%d"; ` + *c
	}
	if c := execCommand(&o.execFile, o.xterm); c != nil {
		fileAct = `Exec ` + *c + ` "%f"`
	}
	titleAct = command(o.commandTitle, titleAct)
	fileAct = command(o.commandFile, fileAct)
	appAct := fileAct
	if c := execCommand(o.execApp, o.xterm); c != nil {
		appAct = `Exec ` + *c + ` "%f"`
	}
	appAct = command(o.commandApp, appAct)
	write := func(format string, args ...any) error { _, err := fmt.Fprintf(out, format, args...); return err }
	if err := write("DestroyMenu recreate \"%s\"\nAddToMenu \"%s\"\n", escapeFile(o.name), escapeMenu(o.name)); err != nil {
		return err
	}
	if !o.memoryForSpeed && !o.reuse {
		if err := write("+ DynamicPopDownAction DestroyMenu \"%s\"\n", escapeFile(o.name)); err != nil {
			return err
		}
	}
	if !o.reuse {
		if err := write("+ MissingSubmenuFunction %s\n", o.funcName); err != nil {
			return err
		}
	}
	if o.title != "" {
		if err := write("+ \"%s", icon(o.titleIcon)); err != nil {
			return err
		}
		if err := formatted(out, title, map[string]string{"d": filepath.Base(o.dir), "p": o.dir}, true); err != nil {
			return err
		}
		action := strings.ReplaceAll(titleAct, "%d", escapeFile(o.dir))
		if action == "" {
			action = "Title"
		}
		if err := write("\" %s\n", action); err != nil {
			return err
		}
		if titleAct != "" {
			if err := write("+ \"\" Nop\n"); err != nil {
				return err
			}
		}
	}
	folder := func(e entry) error {
		if err := write("+ \"%s", icon(o.dirIcon)); err != nil {
			return err
		}
		if e.info == nil || o.item == "%f" {
			if _, err := io.WriteString(labelWriter{out}, e.name); err != nil {
				return err
			}
		} else if err := formatted(labelWriter{out}, item, values(e, item), false); err != nil {
			return err
		}
		action := "Nop"
		if e.path != "" && singleLine(e.path) && (!o.checkSubdirs || syscall.Access(e.path, 1) == nil) {
			if !o.reuse {
				action = `Popup "` + escapeFile(e.path) + `" item +100 c`
			} else {
				program := o.program
				if p, err := os.Executable(); err == nil {
					program = p
				}
				argv := []string{program, "--fvwm-remember", o.userDir, e.path, o.name}
				for i := range argv {
					argv[i] = shellArg(argv[i])
				}
				action = `PipeRead "` + escapeFVWM(strings.Join(argv, " ")) + `"`
			}
		}
		return write("\" %s\n", action)
	}
	var special []entry
	bottom := false
	if o.specialDirs != nil {
		v := *o.specialDirs
		if v == "" {
			v = "1,2"
		}
		match := specialPattern.FindStringSubmatch(v)
		if match != nil {
			bottom = match[2] != ""
			parent := ""
			if o.dir != "/" && !strings.HasSuffix(o.dir, "/") && strings.Contains(o.dir, "/") {
				parent = o.dir[:strings.LastIndexByte(o.dir, '/')]
				if parent == "" {
					parent = "/"
				}
			}
			paths := []string{parent, o.home, "/"}
			labels := []string{"..", "~", "/"}
			for _, part := range strings.Split(match[1], ",") {
				if len(part) != 1 || part[0] < '1' || part[0] > '3' {
					continue
				}
				i := int(part[0] - '1')
				p := paths[i]
				info, _ := os.Stat(p)
				special = append(special, entry{name: labels[i], path: p, info: info, directory: true})
			}
		}
	}
	printSpecial := func() error {
		for i, e := range special {
			if i > 0 {
				if err := write("+ \"\" Nop\n"); err != nil {
					return err
				}
			}
			if err := folder(e); err != nil {
				return err
			}
		}
		return nil
	}
	if len(special) > 0 && !bottom {
		if err := printSpecial(); err != nil {
			return err
		}
		if err := write("+ \"\" Nop\n"); err != nil {
			return err
		}
	}
	for _, e := range entries {
		if e.directory {
			if err := folder(e); err != nil {
				return err
			}
			continue
		}
		im, action := o.fileIcon, fileAct
		if e.executable {
			if icon(o.appIcon) != "" {
				im = o.appIcon
			}
			action = appAct
		}
		if err := write("+ \"%s", icon(im)); err != nil {
			return err
		}
		if o.item == "%f" {
			if _, err := io.WriteString(labelWriter{out}, e.name); err != nil {
				return err
			}
		} else if err := formatted(labelWriter{out}, item, values(e, item), false); err != nil {
			return err
		}
		if !singleLine(e.path) {
			action = "Nop"
		} else {
			action = strings.ReplaceAll(action, "%f", escapeFile(e.path))
		}
		if err := write("\" %s\n", action); err != nil {
			return err
		}
	}
	if len(special) > 0 && bottom {
		if err := write("+ \"\" Nop\n"); err != nil {
			return err
		}
		if err := printSpecial(); err != nil {
			return err
		}
	}
	if o.menuStyle != "" {
		return write("ChangeMenuStyle \"%s\" \"%s\"\n", o.menuStyle, escapeFile(o.name))
	}
	return nil
}

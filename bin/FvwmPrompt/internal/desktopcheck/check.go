// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package desktopcheck inspects a declared literal configuration subset. It
// never interprets FVWM functions, expands input, executes commands or writes.
package desktopcheck

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"syscall"
	"time"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

type Request struct {
	Path    string `json:"path"`
	Userdir string `json:"userdir"`
	Datadir string `json:"datadir"`
	Cwd     string `json:"cwd"`
}
type Source struct {
	Path    string `json:"path"`
	Line    int    `json:"line"`
	EndLine int    `json:"end_line"`
}
type Location struct {
	Source
	IncludedFrom []Source `json:"included_from"`
}
type Diagnostic struct {
	Location
	Severity string `json:"severity"`
	Code     string `json:"code"`
	Message  string `json:"message"`
	Parser   string `json:"parser"`
}
type File struct {
	Path   string `json:"path"`
	Bytes  int    `json:"bytes"`
	SHA256 string `json:"sha256"`
}
type Result struct {
	SchemaVersion     int          `json:"schema_version"`
	Coverage          string       `json:"coverage"`
	Status            string       `json:"status"`
	Errors            int          `json:"errors"`
	Unchecked         int          `json:"unchecked"`
	CheckedDirectives int          `json:"checked_directives"`
	Files             []File       `json:"files"`
	Diagnostics       []Diagnostic `json:"diagnostics"`
}
type limits struct {
	bytes, lineBytes, files, depth, lines, diagnostics int
	duration                                           time.Duration
}

func defaultLimits() limits {
	return limits{4 * 1024 * 1024, 1024 * 1024, 64, 8, 10000, 64, 3 * time.Second}
}

type inode struct{ dev, ino uint64 }
type scheduleValue struct {
	value    any
	location Location
}
type checker struct {
	ctx                  context.Context
	deadline             time.Time
	limits               limits
	search               []string
	bytes, lines, visits int
	result               Result
	active               map[inode]bool
	schedule             map[string]scheduleValue
	stopped              bool
}

// Validate bounds paths before any filesystem operation, also for direct
// resident callers. Search roots are explicit, never inferred from an include.
func (r Request) Validate() error {
	for _, value := range []string{r.Path, r.Userdir, r.Datadir, r.Cwd} {
		if value == "" || len(value) > 4096 || strings.ContainsRune(value, 0) || !utf8.ValidString(value) {
			return errors.New("invalid configuration check path")
		}
	}
	return nil
}

// DecodeRequest uses the shared strict JSON decoder so duplicate/nested fields
// cannot choose different paths in different protocol layers.
func DecodeRequest(data []byte) (Request, error) {
	value, err := desktopsettings.DecodeJSON(data)
	if err != nil {
		return Request{}, err
	}
	fields, ok := value.(map[string]any)
	if !ok || len(fields) != 4 {
		return Request{}, errors.New("invalid configuration check fields")
	}
	r := Request{}
	for key, dest := range map[string]*string{"path": &r.Path, "userdir": &r.Userdir, "datadir": &r.Datadir, "cwd": &r.Cwd} {
		text, ok := fields[key].(string)
		if !ok {
			return Request{}, errors.New("invalid configuration check fields")
		}
		*dest = text
	}
	return r, r.Validate()
}

// filepath.Abs/Join clean ".." lexically, which changes the target when a
// preceding component is a symlink. Let the filesystem resolve literal Reads.
func literalJoin(root, name string) string {
	return strings.TrimRight(root, string(os.PathSeparator)) + string(os.PathSeparator) + name
}
func absolute(path string) (string, error) {
	if filepath.IsAbs(path) {
		return path, nil
	}
	cwd, err := os.Getwd()
	if err != nil {
		return "", err
	}
	return literalJoin(cwd, path), nil
}

func Check(ctx context.Context, r Request) (Result, error) {
	if err := r.Validate(); err != nil {
		return Result{}, err
	}
	return check(ctx, r, defaultLimits()), nil
}
func check(ctx context.Context, r Request, l limits) Result {
	c := checker{ctx: ctx, deadline: time.Now().Add(l.duration), limits: l,
		active: map[inode]bool{}, schedule: map[string]scheduleValue{},
		result: Result{SchemaVersion: 1, Coverage: "desktop-settings-literals-and-Read-v1",
			Files: []File{}, Diagnostics: []Diagnostic{}}}
	for _, root := range []string{r.Userdir, r.Datadir, r.Cwd} {
		absolute, err := absolute(root)
		if err != nil {
			absolute = root
		}
		c.search = append(c.search, absolute)
	}
	path, err := absolute(r.Path)
	if err != nil {
		path = r.Path
	}
	c.read([]string{path}, nil, false)
	day, hasDay := c.schedule["day"]
	night, hasNight := c.schedule["night"]
	if hasDay && hasNight && day.value == night.value {
		c.diagnostic(night.location, "error", "schedule-equal",
			"Explicit day and night times must be different", "FvwmDesktop")
	}
	for _, d := range c.result.Diagnostics {
		if d.Severity == "error" {
			c.result.Errors++
		}
		if d.Severity == "unchecked" {
			c.result.Unchecked++
		}
	}
	c.result.Status = "checked-subset"
	if c.result.Unchecked != 0 {
		c.result.Status = "partial"
	}
	if c.result.Errors != 0 {
		c.result.Status = "errors"
	}
	return c.result
}
func (c *checker) expired() bool {
	return c.ctx.Err() != nil || !time.Now().Before(c.deadline)
}
func (c *checker) diagnostic(loc Location, severity, code, message, parser string) {
	if c.stopped {
		return
	}
	if len(c.result.Diagnostics) == c.limits.diagnostics-1 {
		severity, code, message = "unchecked", "diagnostic-limit",
			"Diagnostic limit reached; remaining input was not checked"
		c.stopped = true
	}
	c.result.Diagnostics = append(c.result.Diagnostics,
		Diagnostic{loc, severity, code, message, parser})
}
func (c *checker) limit(loc Location, code, message string) {
	c.diagnostic(loc, "unchecked", code, message, "configuration-source")
	c.stopped = true
}
func (c *checker) read(paths []string, origin *Location, quiet bool) {
	if c.stopped {
		return
	}
	stack := []Source{}
	if origin != nil {
		stack = append(stack, origin.IncludedFrom...)
		stack = append(stack, origin.Source)
	}
	loc := Location{Source{paths[0], 1, 1}, stack}
	at := func() Location {
		if origin != nil {
			return *origin
		}
		return loc
	}
	if len(stack) >= c.limits.depth || c.visits >= c.limits.files {
		c.limit(at(), "include-limit", "Include depth or file-visit limit reached")
		return
	}
	c.visits++
	var file *os.File
	for _, path := range paths {
		fd, err := syscall.Open(path, syscall.O_RDONLY|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0)
		if errors.Is(err, syscall.ENOENT) || errors.Is(err, syscall.ENOTDIR) {
			continue
		}
		if err != nil {
			c.readError(at(), err)
			return
		}
		file = os.NewFile(uintptr(fd), path)
		loc.Path = path
		break
	}
	if file == nil {
		if !quiet {
			c.diagnostic(at(), "error", "read-missing",
				"Cannot find literal configuration file", "configuration-source")
		}
		return
	}
	// The open descriptor owns the identity/type check and read; no second path
	// lookup can substitute a pipe/device after the regular-file test.
	data, id, ok := c.readFile(file, loc, at())
	if !ok {
		return
	}
	sum := sha256.Sum256(data)
	c.result.Files = append(c.result.Files, File{loc.Path, len(data), hex.EncodeToString(sum[:])})
	c.active[id] = true
	defer delete(c.active, id)
	logical := []byte{}
	start, number := 1, 0
	for len(data) > 0 && !c.stopped {
		end := bytes.IndexByte(data, '\n')
		newline := end >= 0
		if !newline {
			end = len(data)
		}
		part := data[:end]
		if newline {
			data = data[end+1:]
		} else {
			data = nil
		}
		number++
		c.lines++
		loc.Line, loc.EndLine = start, number
		if c.lines > c.limits.lines || c.expired() {
			c.limit(loc, "work-limit", "Line or elapsed checking budget reached")
			break
		}
		if len(part) > c.limits.lineBytes-len(logical) {
			c.limit(loc, "line-limit", "Logical source line limit reached")
			break
		}
		logical = append(logical, part...)
		if newline && bytes.HasSuffix(logical, []byte{'\\'}) {
			logical = logical[:len(logical)-1]
			continue
		}
		c.line(logical, loc)
		logical = logical[:0]
		start = number + 1
	}
	if len(logical) > 0 && !c.stopped {
		c.line(logical, loc)
	}
}
func (c *checker) readFile(file *os.File, loc, origin Location) ([]byte, inode, bool) {
	defer file.Close()
	info, err := file.Stat()
	if err != nil {
		c.readError(origin, err)
		return nil, inode{}, false
	}
	if !info.Mode().IsRegular() {
		c.diagnostic(origin, "error", "read-type",
			"Configuration source must be a regular file", "configuration-source")
		return nil, inode{}, false
	}
	stat := info.Sys().(*syscall.Stat_t)
	id := inode{uint64(stat.Dev), uint64(stat.Ino)}
	if c.active[id] {
		c.diagnostic(origin, "error", "include-cycle", "Literal Read cycle detected",
			"configuration-source")
		return nil, id, false
	}
	var data bytes.Buffer
	chunk := make([]byte, 65536)
	for {
		if c.expired() {
			c.limit(loc, "time-limit", "Elapsed checking budget reached")
			return nil, id, false
		}
		size := min(len(chunk), c.limits.bytes-c.bytes+1)
		n, err := file.Read(chunk[:size])
		c.bytes += n
		if c.bytes > c.limits.bytes {
			c.limit(loc, "byte-limit", "Combined source byte limit reached")
			return nil, id, false
		}
		data.Write(chunk[:n])
		if err == io.EOF {
			break
		}
		if err != nil {
			c.readError(origin, err)
			return nil, id, false
		}
	}
	return data.Bytes(), id, true
}
func (c *checker) readError(loc Location, err error) {
	var pathError *os.PathError
	if errors.As(err, &pathError) {
		err = pathError.Err
	}
	c.diagnostic(loc, "error", "read-error",
		"Cannot inspect configuration source: "+err.Error(), "configuration-source")
}
func asciiLower(text string) string {
	return strings.Map(func(r rune) rune {
		if r >= 'A' && r <= 'Z' {
			return r + 'a' - 'A'
		}
		return r
	}, text)
}

const spaces = " \t\r\v\f"

var literalRead = regexp.MustCompile(`^[ \t\r\v\f]+(?:"([^"\\]+)"|'([^'\\]+)'|([^ \t\r\v\f'"\\]+))(?:[ \t\r\v\f]+([Qq][Uu][Ii][Ee][Tt]))?$`)

func (c *checker) line(data []byte, loc Location) {
	diagnostic := func(severity, code, message string) { c.diagnostic(loc, severity, code, message, "FvwmDesktop") }
	source := func(severity, code, message string) {
		c.diagnostic(loc, severity, code, message, "configuration-source")
	}
	if bytes.IndexByte(data, 0) >= 0 {
		source("error", "nul-byte", "NUL byte in source")
		return
	}
	if !utf8.Valid(data) {
		source("unchecked", "encoding", "This check supports UTF-8 source only")
		return
	}
	text := strings.Trim(string(data), spaces)
	if text == "" || strings.HasPrefix(text, "#") {
		return
	}
	if strings.ContainsAny(text, "$`") {
		diagnostic("unchecked", "expansion", "Expansion or generated configuration is not evaluated")
		return
	}
	lower := asciiLower(text)
	if lower == "read" || (strings.HasPrefix(lower, "read") && len(text) > 4 && strings.ContainsRune(spaces, rune(text[4]))) {
		if lower == "read" {
			source("error", "read-argument", "Read requires a filename")
			return
		}
		match := literalRead.FindStringSubmatch(text[4:])
		if match != nil {
			name := match[1] + match[2] + match[3]
			if !strings.HasPrefix(name, "~") {
				paths := []string{name}
				if !filepath.IsAbs(name) {
					paths = nil
					for _, root := range c.search {
						paths = append(paths, literalJoin(root, name))
					}
				}
				c.result.CheckedDirectives++
				c.read(paths, &loc, match[4] != "")
				return
			}
		}
		source("unchecked", "read-syntax", "Only literal Read filenames and optional Quiet are checked")
		return
	}
	if strings.HasPrefix(lower, "*fvwmdesktop:") {
		option := strings.TrimSpace(text[len("*FvwmDesktop:"):])
		key, value, _ := strings.Cut(option, " ")
		key = strings.ToLower(key)
		value = strings.TrimSpace(value)
		if key == "terminal" {
			diagnostic("unchecked", "terminal-provider", "Terminal invocation and provider availability are not checked")
			return
		}
		keys, err := desktopsettings.Scope(key)
		if err != nil || len(keys) != 1 || keys[0] != key {
			diagnostic("error", "setting-name", "Unknown FvwmDesktop option")
			return
		}
		parsed, err := desktopsettings.Value(key, value, true)
		if err != nil {
			diagnostic("error", "setting-value", err.Error())
			return
		}
		c.result.CheckedDirectives++
		if key == "day" || key == "night" {
			c.schedule[key] = scheduleValue{parsed, loc}
		}
		return
	}
	diagnostic("unchecked", "unsupported", "Command or module syntax is outside the desktop-settings subset")
}

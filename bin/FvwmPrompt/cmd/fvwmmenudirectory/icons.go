// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/iconrender"
	"github.com/fvwmorg/FvwmPrompt/internal/runtimepath"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

var svgEnabled = "true"
var pngEnabled = "true"
var xpmEnabled = "true"

const iconDeadline = 5 * time.Second
const iconResultLimit = 4 * (xdgmenu.MaxValueBytes + 32)

type iconOutput struct {
	bytes.Buffer
	limit int
}

func (out *iconOutput) Write(data []byte) (int, error) {
	if len(data) > out.limit-out.Len() {
		return 0, errors.New("icon worker output exceeds limit")
	}
	return out.Buffer.Write(data)
}

func validIconValue(value string) bool {
	return len(value) <= xdgmenu.MaxValueBytes && utf8.ValidString(value) && singleLine(value)
}

func validPicture(value string) bool {
	return validIconValue(value) && !strings.ContainsAny(value, "%*") && (value == "" || filepath.IsAbs(value))
}

func menuIcon(o options, value string) string {
	if o.iconTheme == "" {
		return icon(value)
	}
	if value == "" {
		return ""
	}
	return "%" + escapeFVWM(value) + "%"
}

func iconArguments(o options) []string {
	// Fixed role metadata is the complete worker protocol. Neither actions nor
	// the directory listing cross this boundary, and no command is interpreted.
	return []string{"--icon-theme", o.iconTheme, "--icon-size", strconv.Itoa(o.iconSize),
		"--icon-scale", strconv.FormatFloat(float64(o.iconScaleQuarters)/4, 'f', -1, 64),
		"--mini-icon-dir", o.iconDir, "--icon-title", o.titleIcon,
		"--icon-dir", o.dirIcon, "--icon-file", o.fileIcon, "--icon-app", o.appIcon}
}

func acquireIcons(o options) ([4]string, error) {
	executable, err := runtimepath.Executable(o.program)
	if err != nil {
		return [4]string{}, err
	}
	ctx, cancel := context.WithTimeout(context.Background(), iconDeadline)
	defer cancel()
	return superviseIcons(ctx, executable, iconArguments(o))
}

// Only completed, schema-checked metadata is exposed to menu rendering. A hard
// parent deadline also covers filesystem calls which ignore cancellation.
func superviseIcons(ctx context.Context, executable string, args []string) ([4]string, error) {
	var result [4]string
	command := exec.Command(executable, append([]string{"--_icons"}, args...)...)
	stdout, stderr := &iconOutput{limit: iconResultLimit}, &iconOutput{limit: 64 * 1024}
	command.Stdout, command.Stderr = stdout, stderr
	if err := command.Start(); err != nil {
		return result, err
	}
	completed := make(chan error, 1)
	go func() { completed <- command.Wait() }()
	select {
	case err := <-completed:
		if err != nil {
			return result, fmt.Errorf("icon worker failed: %w: %s", err, strings.TrimSpace(stderr.String()))
		}
		values, err := decodePictures(stdout.Bytes())
		if err != nil {
			return result, err
		}
		return values, nil
	case <-ctx.Done():
		// This fixed metadata worker never starts descendants. Process.Kill
		// retains Go's lifetime protection if Wait wins the deadline race.
		_ = command.Process.Kill()
		return result, ctx.Err()
	}
}

func decodePictures(data []byte) ([4]string, error) {
	var result [4]string
	if !utf8.Valid(data) {
		return result, errors.New("invalid UTF-8 icon result")
	}
	if len(data) > iconResultLimit {
		return result, errors.New("icon result exceeds limit")
	}
	var values []json.RawMessage
	decoder := json.NewDecoder(bytes.NewReader(data))
	if err := decoder.Decode(&values); err != nil {
		return result, err
	}
	if len(values) != len(result) {
		return result, errors.New("icon result requires four roles")
	}
	var extra any
	if err := decoder.Decode(&extra); err != io.EOF {
		return result, errors.New("trailing icon result")
	}
	for i, raw := range values {
		var value string
		if len(raw) == 0 || raw[0] != '"' {
			return [4]string{}, errors.New("icon role must be a string")
		}
		if err := json.Unmarshal(raw, &value); err != nil {
			return [4]string{}, err
		}
		if !validPicture(value) {
			return [4]string{}, errors.New("invalid native menu picture from icon worker")
		}
		result[i] = value
	}
	return result, nil
}

func resolveIcons(ctx context.Context, o options, env map[string]string) ([4]string, error) {
	var result [4]string
	paths, err := xdgmenu.SearchDirectories(o.home, env)
	if err != nil {
		return result, err
	}
	dir := o.iconDir
	if strings.HasPrefix(dir, "~/") {
		dir = filepath.Join(o.home, dir[2:])
	}
	dir, err = filepath.Abs(dir)
	if err != nil {
		return result, err
	}
	extensions := []string{}
	if pngEnabled == "true" {
		extensions = append(extensions, "png")
	}
	if svgEnabled == "true" {
		extensions = append(extensions, "svg")
	}
	if xpmEnabled == "true" {
		extensions = append(extensions, "xpm")
	}
	renderer, err := iconrender.New(xdgsource.New(ctx), paths, o.home, iconrender.Config{CacheDir: dir, Extensions: extensions})
	if err != nil {
		return result, err
	}
	names := [4]string{"folder-open", "folder", "text-x-generic", "application-x-executable"}
	overrides := [4]string{o.titleIcon, o.dirIcon, o.fileIcon, o.appIcon}
	for i, name := range names {
		if overrides[i] == "0" {
			continue
		}
		result[i], err = renderer.Resolve(xdgmenu.IconRequest{Name: name, Theme: o.iconTheme,
			Size: o.iconSize, ScaleQuarters: o.iconScaleQuarters, Override: overrides[i]})
		if err != nil {
			return [4]string{}, err
		}
		if !validPicture(result[i]) {
			return [4]string{}, errors.New("invalid resolved native menu picture")
		}
	}
	return result, nil
}

func iconWorker(args []string, out, diagnostics io.Writer, program string) int {
	o, done, status := parse(args, io.Discard, diagnostics, program)
	if done {
		return status
	}
	if o.iconTheme == "" {
		fmt.Fprintln(diagnostics, "icon worker requires --icon-theme")
		return 1
	}
	env := make(map[string]string)
	for _, entry := range os.Environ() {
		key, value, _ := strings.Cut(entry, "=")
		env[key] = value
	}
	ctx, cancel := context.WithTimeout(context.Background(), iconDeadline)
	defer cancel()
	result, err := resolveIcons(ctx, o, env)
	if err == nil {
		err = json.NewEncoder(out).Encode(result)
	}
	if err != nil {
		fmt.Fprintln(diagnostics, "fvwm-menu-directory: icons:", err)
		return 1
	}
	return 0
}

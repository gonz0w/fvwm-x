// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
	"time"
)

func TestNamedIconOptions(t *testing.T) {
	t.Setenv("HOME", t.TempDir())
	args := []string{"--icon-theme", "test", "--icon-size", "17", "--icon-scale", "1.25", "--mini-icon-dir", "/tmp/cache", "--icon-app", "0"}
	o, done, status := parse(args, &bytes.Buffer{}, &bytes.Buffer{}, "tool")
	if done || status != 0 || o.iconTheme != "test" || o.iconSize != 17 || o.iconScaleQuarters != 5 || o.iconDir != "/tmp/cache" || o.appIcon != "0" {
		t.Fatal(o, done, status)
	}
	again, done, status := parse(iconArguments(o), &bytes.Buffer{}, &bytes.Buffer{}, "tool")
	if done || status != 0 || again.iconTheme != o.iconTheme || again.iconSize != o.iconSize || again.iconScaleQuarters != o.iconScaleQuarters || again.appIcon != "0" {
		t.Fatal(again, done, status)
	}
	for _, args := range [][]string{
		{"--icon-theme", ""}, {"--icon-theme", "../bad"}, {"--icon-theme", "bad\nvalue"}, {"--mini-icon-dir", ""},
		{"--icon-size", "0"}, {"--icon-size", "-1"}, {"--icon-size", "4097"}, {"--icon-size", "word"},
		{"--icon-scale", "0"}, {"--icon-scale", "1.2"}, {"--icon-scale", "4.25"}, {"--icon-scale", "NaN"},
		{"--icon-size", "4096", "--icon-scale", "1.25"}, {"--icon-theme", "test", "--icon-dir", "../bad"},
	} {
		var output, diagnostic bytes.Buffer
		if _, done, status := parse(args, &output, &diagnostic, "tool"); !done || status != 255 || diagnostic.Len() == 0 || output.Len() != 0 {
			t.Fatal(args, done, status, output.String(), diagnostic.String())
		}
	}
}

func TestLegacyTitleIconAbbreviation(t *testing.T) {
	t.Setenv("HOME", t.TempDir())
	for _, args := range [][]string{{"--icon-t=literal.xpm"}, {"--icon-t", "literal.xpm"}} {
		var diagnostic bytes.Buffer
		o, done, status := parse(args, &bytes.Buffer{}, &diagnostic, "tool")
		if done || status != 0 || o.titleIcon != "literal.xpm" || o.iconTheme != "" {
			t.Fatal(args, o, done, status, diagnostic.String())
		}
	}
	for _, args := range [][]string{{"--icon-th=test"}, {"--icon-th", "test"}} {
		var diagnostic bytes.Buffer
		o, done, status := parse(args, &bytes.Buffer{}, &diagnostic, "tool")
		if done || status != 0 || o.iconTheme != "test" || o.titleIcon != "" {
			t.Fatal(args, o, done, status, diagnostic.String())
		}
	}
}

func directoryIconFixture(t *testing.T) (options, map[string]string) {
	t.Helper()
	root := t.TempDir()
	t.Setenv("HOME", root)
	o := defaults("tool")
	o.iconTheme, o.iconSize, o.iconScaleQuarters, o.iconDir = "test", 17, 5, filepath.Join(root, "cache")
	data := filepath.Join(root, "data")
	theme := filepath.Join(data, "icons", "test")
	if err := os.MkdirAll(filepath.Join(theme, "24x24"), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(theme, "index.theme"), []byte("[Icon Theme]\nDirectories=24x24\n[24x24]\nSize=24\nType=Fixed\n"), 0600); err != nil {
		t.Fatal(err)
	}
	for _, name := range []string{"folder-open", "folder", "text-x-generic", "application-x-executable", "custom"} {
		if err := os.WriteFile(filepath.Join(theme, "24x24", name+".svg"), []byte("<svg>"+name+"</svg>"), 0600); err != nil {
			t.Fatal(err)
		}
	}
	return o, map[string]string{"XDG_DATA_HOME": data, "XDG_DATA_DIRS": filepath.Join(root, "missing-data"), "XDG_CONFIG_DIRS": filepath.Join(root, "missing-config")}
}

func TestDirectoryNamedIconRoles(t *testing.T) {
	o, env := directoryIconFixture(t)
	o.titleIcon, o.appIcon = "custom", "0"
	expectedArgs := iconArguments(o)
	got, err := resolveIcons(context.Background(), o, env)
	if err != nil {
		t.Fatal(err)
	}
	for i, want := range []string{"custom", "folder", "text-x-generic", ""} {
		if want == "" {
			if got[i] != "" {
				t.Fatal(got)
			}
			continue
		}
		if !strings.HasPrefix(got[i], o.iconDir+string(os.PathSeparator)) || !strings.HasSuffix(got[i], ".svg:22x22") {
			t.Fatal(got[i])
		}
		data, err := os.ReadFile(strings.TrimSuffix(got[i], ":22x22"))
		if err != nil || string(data) != "<svg>"+want+"</svg>" {
			t.Fatal(string(data), err)
		}
	}
	o.pictures = got
	o.name, o.dir, o.title, o.item, o.funcName = "test menu", "/test", "%d", "%n", "ExistingCallback"
	var out bytes.Buffer
	if err := menu(&out, o, []entry{{name: "subdir", path: "/test/subdir", directory: true}, {name: "binary", path: "/test/binary", executable: true}}); err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(out.String(), "+ MissingSubmenuFunction ExistingCallback\n") || !strings.Contains(out.String(), `" Popup "/test/subdir" item +100 c`) || !strings.Contains(out.String(), `+ "binary"`) {
		t.Fatal(out.String())
	}
	if !reflect.DeepEqual(iconArguments(o), expectedArgs) || o.titleIcon != "custom" || o.appIcon != "0" {
		t.Fatal("presentation replaced scoped callback options", o)
	}
	o.titleIcon, o.appIcon = "missing", "missing"
	fallback, err := resolveIcons(context.Background(), o, env)
	if err != nil || fallback[0] == "" || fallback[3] == "" {
		t.Fatal(fallback, err)
	}
}

func TestDirectoryIconFormatGatesAndCancellation(t *testing.T) {
	o, env := directoryIconFixture(t)
	original := [3]string{pngEnabled, svgEnabled, xpmEnabled}
	defer func() { pngEnabled, svgEnabled, xpmEnabled = original[0], original[1], original[2] }()
	pngEnabled, svgEnabled, xpmEnabled = "false", "false", "false"
	if got, err := resolveIcons(context.Background(), o, env); err != nil || got != [4]string{} {
		t.Fatal(got, err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, err := resolveIcons(ctx, o, env); !errors.Is(err, context.Canceled) {
		t.Fatal(err)
	}
}

func TestDirectoryLegacyIconRendering(t *testing.T) {
	o := options{name: "menu", dir: "/dir", item: "%n", title: "%d", wmIcons: true}
	var out bytes.Buffer
	if err := menu(&out, o, nil); err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(out.String(), `%menu/folder-open.xpm%`) {
		t.Fatal(out.String())
	}
	o.wmIcons, o.titleIcon = false, "literal.xpm"
	out.Reset()
	if err := menu(&out, o, nil); err != nil || !strings.Contains(out.String(), `%literal.xpm%`) {
		t.Fatal(out.String(), err)
	}
	// Named mode uses the existing quoting path for private picture filenames.
	if got := menuIcon(options{iconTheme: "test"}, `/cache/quote"$.svg:22x22`); got != `%/cache/quote\"$$.svg:22x22%` {
		t.Fatal(got)
	}
}

func TestIconWorkerMetadataRefusals(t *testing.T) {
	good := []string{"/cache/title.svg:22x22", "", "/cache/file.png", ""}
	encoded, _ := json.Marshal(good)
	if got, err := decodePictures(encoded); err != nil || got[0] != good[0] {
		t.Fatal(got, err)
	}
	for _, data := range [][]byte{
		[]byte(`[]`), []byte(`["", "", "", "", "extra"]`), []byte(`["", null, "", ""]`),
		[]byte(`["relative.png", "", "", ""]`), []byte(`["/bad%icon", "", "", ""]`),
		[]byte(`["/bad*icon", "", "", ""]`), []byte(`["/bad\nicon", "", "", ""]`),
		append(encoded, []byte(" []")...), []byte{'\xff'}, bytes.Repeat([]byte(" "), iconResultLimit+1),
	} {
		if _, err := decodePictures(data); err == nil {
			t.Fatal("accepted metadata", string(data[:min(len(data), 80)]))
		}
	}
	limited := &iconOutput{limit: 4}
	if _, err := limited.Write([]byte("12345")); err == nil || limited.Len() != 0 {
		t.Fatal("unbounded output")
	}
}

func TestIconSupervisorDeadlineAndOutput(t *testing.T) {
	root := t.TempDir()
	worker := filepath.Join(root, "worker")
	for _, tc := range []struct {
		name, body string
		deadline   time.Duration
		success    bool
	}{
		{"complete", "printf '[\"/cache/icon.png\",\"\",\"\",\"\"]'", time.Second, true},
		{"malformed", "printf 'arbitrary commands'", time.Second, false},
		{"failure", "printf '[\"\",\"\",\"\",\"\"]'; exit 1", time.Second, false},
		{"deadline", "while :; do :; done", 40 * time.Millisecond, false},
		{"oversized", "i=0; while [ $i -lt 10000 ]; do printf 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'; i=$((i+1)); done", time.Second, false},
	} {
		t.Run(tc.name, func(t *testing.T) {
			if err := os.WriteFile(worker, []byte("#!/bin/sh\n"+tc.body+"\n"), 0700); err != nil {
				t.Fatal(err)
			}
			ctx, cancel := context.WithTimeout(context.Background(), tc.deadline)
			defer cancel()
			start := time.Now()
			got, err := superviseIcons(ctx, worker, nil)
			if (err == nil) != tc.success {
				t.Fatal(got, err)
			}
			if tc.name == "deadline" && (!errors.Is(err, context.DeadlineExceeded) || time.Since(start) > time.Second) {
				t.Fatal(err, time.Since(start))
			}
			if !tc.success && got != [4]string{} {
				t.Fatal("published failed worker metadata", got)
			}
		})
	}
}

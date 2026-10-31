// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package trayicons

import (
	"bytes"
	"context"
	"encoding/hex"
	"errors"
	"fmt"
	"image"
	"image/color"
	"image/png"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func frame(directory, name, theme, extra string) string {
	hexText := func(text string) string {
		if text == "" {
			return "-"
		}
		return hex.EncodeToString([]byte(text))
	}
	return fmt.Sprintf("FVWM_TRAY_ICONS 1 1 %s png,xpm\n9 11 24 4 %s %s %s\nEND\n", hexText(directory), hexText(name), hexText(theme), hexText(extra))
}
func TestFrameRefusesMalformedAndReusedIdentities(t *testing.T) {
	good := frame("/tmp/private", "app", "hicolor", "")
	if batch, err := Decode([]byte(good)); err != nil || len(batch.Requests) != 1 || batch.Requests[0].ID != 9 || batch.Requests[0].Generation != 11 {
		t.Fatal(batch, err)
	}
	for _, bad := range []string{
		"", strings.Replace(good, " 1 1 ", " 2 1 ", 1),
		strings.Replace(good, " 1 1 ", " 1 9 ", 1),
		strings.Replace(good, "9 11 24 4", "9 11 7 4", 1),
		strings.Replace(good, "9 11 24 4", "9 11 129 4", 1),
		strings.Replace(good, "9 11 24 4", "9 11 24 33", 1),
		strings.Replace(good, "9 11 24 4", "+9 11 24 4", 1),
		strings.Replace(good, "9 11 24 4", "9 0 24 4", 1),
		strings.Replace(good, "9 11 24 4", "18446744073709551616 11 24 4", 1),
		strings.Replace(good, "png,xpm", "png,png", 1),
		strings.Replace(good, "png,xpm", "jpeg", 1),
		strings.TrimSuffix(good, "\n"), good + "END\n",
		strings.Replace(good, "\n", "\r\n", 1),
		frame("relative", "app", "hicolor", ""),
		frame("/tmp/private", strings.Repeat("x", 257), "hicolor", ""),
		frame("/tmp/private", "app\x00", "hicolor", ""),
		frame("/tmp/private", "app", "../theme", ""),
		frame("/tmp/private", "app", "hicolor", "relative"),
		frame("/tmp/private", "app", "hicolor", "/tmp/../other"),
	} {
		if _, err := Decode([]byte(bad)); err == nil {
			t.Fatalf("accepted malformed frame %q", bad)
		}
	}
	duplicate := strings.Replace(good, " 1 1 ", " 1 2 ", 1)
	duplicate = strings.Replace(duplicate, "END\n", strings.Split(good, "\n")[1]+"\nEND\n", 1)
	if _, err := Decode([]byte(duplicate)); err == nil {
		t.Fatal("reused identity accepted")
	}
}
func TestSharedFinderExtraRootAndPrivateSnapshot(t *testing.T) {
	dir, home := t.TempDir(), t.TempDir()
	if err := os.Chmod(dir, 0700); err != nil {
		t.Fatal(err)
	}
	root := t.TempDir()
	pic := image.NewNRGBA(image.Rect(0, 0, 24, 24))
	pic.SetNRGBA(0, 0, color.NRGBA{R: 255, A: 255})
	var data bytes.Buffer
	if err := png.Encode(&data, pic); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "app.png"), data.Bytes(), 0600); err != nil {
		t.Fatal(err)
	}
	var output bytes.Buffer
	env := map[string]string{"HOME": home, "XDG_DATA_HOME": filepath.Join(home, "data"), "XDG_DATA_DIRS": filepath.Join(home, "empty")}
	if err := Run(context.Background(), strings.NewReader(frame(dir, "app", "hicolor", root)), &output, env); err != nil {
		t.Fatal(err)
	}
	lines := strings.Split(strings.TrimSuffix(output.String(), "\n"), "\n")
	if len(lines) != 3 || lines[0] != "FVWM_TRAY_ICONS 1 1" || lines[2] != "END" {
		t.Fatal(output.String())
	}
	parts := strings.Split(lines[1], " ")
	if len(parts) != 3 || parts[0] != "9" || parts[1] != "11" || parts[2] == "-" {
		t.Fatal(output.String())
	}
	path, err := hex.DecodeString(parts[2])
	if err != nil {
		t.Fatal(err)
	}
	if filepath.Dir(string(path)) != dir {
		t.Fatal("not an owned immutable publication", string(path))
	}
	got, err := os.ReadFile(string(path))
	if err != nil || !bytes.Equal(got, data.Bytes()) {
		t.Fatal(err)
	}
	if err := os.Chmod(dir, 0755); err != nil {
		t.Fatal(err)
	}
	output.Reset()
	if err := Run(context.Background(), strings.NewReader(frame(dir, "app", "hicolor", root)), &output, env); err == nil || output.Len() != 0 {
		t.Fatal("nonprivate output accepted", err)
	}
}
func TestMissingIconAndCanceledInput(t *testing.T) {
	dir := t.TempDir()
	if err := os.Chmod(dir, 0700); err != nil {
		t.Fatal(err)
	}
	home := t.TempDir()
	env := map[string]string{"HOME": home, "XDG_DATA_HOME": filepath.Join(home, "data"), "XDG_DATA_DIRS": filepath.Join(home, "empty")}
	var output bytes.Buffer
	if err := Run(context.Background(), strings.NewReader(frame(dir, "fvwm-unavailable-unit-icon-8a3a", "hicolor", "")), &output, env); err != nil || !strings.Contains(output.String(), "9 11 -\n") {
		t.Fatal(err, output.String())
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	output.Reset()
	if err := Run(ctx, strings.NewReader(frame(dir, "app", "hicolor", "")), &output, env); err == nil || output.Len() != 0 {
		t.Fatal("canceled batch published", err)
	}
}

func TestInputDeadlineOwnsIncompleteFrame(t *testing.T) {
	reader, writer := io.Pipe()
	defer reader.Close()
	defer writer.Close()
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Millisecond)
	defer cancel()
	var output bytes.Buffer
	if err := Run(ctx, reader, &output, map[string]string{}); !errors.Is(err, context.DeadlineExceeded) || output.Len() != 0 {
		t.Fatal("incomplete input escaped its deadline", err)
	}
}

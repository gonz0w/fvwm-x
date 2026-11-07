// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopworker

import (
	"context"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"syscall"
	"testing"
)

func TestPaletteWorkerPrivateRequestAndSourceErrors(t *testing.T) {
	request := Request{Version: 1, Operation: "palettes", Path: "/theme/palettes.json"}
	data, _ := json.Marshal(request)
	if _, err := Decode(data); err != nil {
		t.Fatal(err)
	}
	for _, field := range []string{"system", "action", "directories", "check", "inspect"} {
		var object map[string]any
		json.Unmarshal(data, &object)
		object[field] = map[string]any{"system": "", "action": "", "directories": []any{}, "check": map[string]any{}, "inspect": map[string]any{}}[field]
		extra, _ := json.Marshal(object)
		if _, err := Decode(extra); err == nil {
			t.Fatal("accepted unexpected field", field)
		}
	}
	for _, data := range []string{`{"version":1,"operation":"palettes"}`, `{"version":1,"operation":"palettes","path":""}`} {
		if _, err := Decode([]byte(data)); err == nil {
			t.Fatal("accepted missing source")
		}
	}
	for _, read := range []func(string) ([]byte, error){
		func(string) ([]byte, error) { return nil, errors.New("missing source") },
		func(string) ([]byte, error) { return []byte(`{}`), nil },
	} {
		value, err := Run(context.Background(), request, Environment{Read: read})
		result, ok := value.(PaletteResult)
		if err != nil || !ok || result.Palettes != nil || !strings.Contains(result.Error, request.Path) {
			t.Fatal("lost source diagnostic", value, err)
		}
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, err := Run(ctx, request, Environment{Read: func(string) ([]byte, error) {
		t.Fatal("read after cancellation")
		return nil, nil
	}}); !errors.Is(err, context.Canceled) {
		t.Fatal(err)
	}
}
func TestPaletteWorkerLoadsBoundedRegularSource(t *testing.T) {
	data, err := os.ReadFile("../../../../default-config/palettes.json")
	if err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(t.TempDir(), "palettes.json")
	if err := os.WriteFile(path, data, 0600); err != nil {
		t.Fatal(err)
	}
	palettes, err := LoadPalettes(path)
	if err != nil || palettes["slate"]["dark"]["surface"] != "#202a31" {
		t.Fatal(palettes, err)
	}
	if err := os.WriteFile(path, []byte(strings.Repeat(" ", 65537)), 0600); err != nil {
		t.Fatal(err)
	}
	if _, err := LoadPalettes(path); err == nil || !strings.Contains(err.Error(), path) {
		t.Fatal("oversize source accepted", err)
	}
	if err := os.Remove(path); err != nil {
		t.Fatal(err)
	}
	if err := syscall.Mkfifo(path, 0600); err != nil {
		t.Fatal(err)
	}
	if _, err := LoadPalettes(path); err == nil {
		t.Fatal("FIFO accepted")
	}
	if _, err := LoadPalettes(filepath.Dir(path)); err == nil {
		t.Fatal("directory accepted")
	}
}

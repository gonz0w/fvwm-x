package main

import (
	"context"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/pinboard"
)

func TestDataOnlyPinOptionsKeepFullLiteralTargetBytes(t *testing.T) {
	path := "/" + strings.Repeat("x", 4094) + "\xff"
	o, err := parseOptions([]string{"--pin", path, "--label", "Full target", "--monitor", "preferred", "--column", "3", "--row", "4"}, "/home/user", io.Discard)
	if err != nil || !o.create || string(o.pin.Target) != path || o.pin.Column != 3 || o.pin.Row != 4 || o.pin.Label != "Full target" {
		t.Fatal(o, err)
	}
	for _, args := range [][]string{
		{"--pin", "/a", "--pin", "/b"}, {"--label", "orphan"}, {"--pin", "/a", "--row", "-1"},
		{"--pin", "/a", "--label", strings.Repeat("x", 257)}, {"--pin", "/a", "--icon", "/image.png"},
		{"--pin", "relative"}, {"--pin", "/a", "--cell-width", "63"}, {"--state", "relative"},
	} {
		if _, err := parseOptions(args, "/home/user", io.Discard); err == nil {
			t.Fatal("invalid options accepted", args)
		}
	}
}

func TestDataOnlyPinWorkerEditsReferencesWithoutInspectingTarget(t *testing.T) {
	dir := t.TempDir()
	if err := os.Chmod(dir, 0700); err != nil {
		t.Fatal(err)
	}
	state := filepath.Join(dir, "pins.json")
	target := filepath.Join(dir, "nonexistent-parent", "target")
	pin := pinboard.Pin{ID: strings.Repeat("a", 32), Target: []byte(target), Label: "Missing reference"}
	result := perform(context.Background(), workRequest{Operation: "pin", Path: state, Pin: &pin})
	if result.Status != "saved" || result.State == nil || result.State.Revision != 1 || len(result.Observed) != 0 {
		t.Fatal(result)
	}
	if _, err := os.Stat(filepath.Dir(target)); !os.IsNotExist(err) {
		t.Fatal("pin touched its target tree", err)
	}
	if result := perform(context.Background(), workRequest{Operation: "pin", Path: state, Pin: &pin}); result.Status != "failed" {
		t.Fatal("duplicate pin accepted", result)
	}
	loaded, err := pinboard.Load(state)
	if err != nil || len(loaded.Pins) != 1 || string(loaded.Pins[0].Target) != target {
		t.Fatal(loaded, err)
	}
}

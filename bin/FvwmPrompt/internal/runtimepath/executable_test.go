// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package runtimepath

import (
	"errors"
	"os"
	"path/filepath"
	"testing"
)

func TestReportedImageResolvesSymlinks(t *testing.T) {
	root, err := filepath.EvalSymlinks(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	image, alias := filepath.Join(root, "image"), filepath.Join(root, "alias")
	if err := os.WriteFile(image, nil, 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(image, alias); err != nil {
		t.Fatal(err)
	}
	if got, err := resolve(alias, nil, "/unused"); err != nil || got != image {
		t.Fatal(got, err)
	}
	if err := os.Remove(image); err != nil {
		t.Fatal(err)
	}
	if got, err := resolve(alias, nil, "/unused"); err == nil || got != "" {
		t.Fatal(got, err)
	}
}

func TestUnavailableProcfsUsesOnlyAbsoluteVerifiedInvocation(t *testing.T) {
	unavailable := errors.New("procfs unavailable")
	root, err := filepath.EvalSymlinks(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	image, alias := filepath.Join(root, "image"), filepath.Join(root, "alias")
	if err := os.WriteFile(image, nil, 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(image, alias); err != nil {
		t.Fatal(err)
	}
	if got, err := resolve(image, nil, "/unused"); err != nil || got != image {
		t.Fatal(got, err)
	}
	if got, err := resolve("", unavailable, alias); err != nil || got != image {
		t.Fatal(got, err)
	}
	for _, invoked := range []string{"", "image", "relative/image", filepath.Join(root, "missing"), "/bad\x00path"} {
		if got, err := resolve("", unavailable, invoked); err == nil || got != "" {
			t.Fatal(invoked, got, err)
		}
	}
	if got, err := resolve("", nil, "relative"); err == nil || got != "" {
		t.Fatal(got, err)
	}
}

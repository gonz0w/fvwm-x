// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
package xdgsource

import (
	"os"
	"path/filepath"
	"testing"
)

func TestSmallerAcquisitionBudgetBeforeAllocation(t *testing.T) {
	for _, pair := range [][2]int{{0, 1}, {1, 0}, {-1, 1}, {MaxJobBytes + 1, 1}} {
		if _, err := NewLimited(nil, pair[0], pair[1]); err == nil {
			t.Fatal("invalid budget accepted", pair)
		}
	}
	dir := t.TempDir()
	path := filepath.Join(dir, "bytes.png")
	if err := os.WriteFile(path, make([]byte, 4096), 0600); err != nil {
		t.Fatal(err)
	}
	source, err := NewLimited(nil, 128, 8)
	if err != nil {
		t.Fatal(err)
	}
	if _, err = source.ReadImage(path); err == nil || len(source.files) != 0 {
		t.Fatal("oversized source retained", err)
	}
	if New(nil).bytes != MaxJobBytes {
		t.Fatal("default acquisition budget changed")
	}
}

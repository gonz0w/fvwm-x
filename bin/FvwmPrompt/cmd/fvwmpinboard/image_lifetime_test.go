// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
package main

import (
	"bytes"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/pinboard"
)

func TestUnacknowledgedShutdownRetainsEveryImageOwner(t *testing.T) {
	b := testBoard(t)
	b.cacheRoot = filepath.Join(t.TempDir(), "fvwm-pinboard-"+b.protocol.Token)
	var caches []string
	name := ".fvwm-xdg-" + strings.Repeat("b", 64) + ".png"
	content := []byte("retained native image bytes")
	for _, generation := range []string{"g-1", "g-2", "g-3"} {
		cache := filepath.Join(b.cacheRoot, generation)
		if err := pinboard.PrepareImages(cache, b.protocol.Token); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(filepath.Join(cache, name), content, 0600); err != nil {
			t.Fatal(err)
		}
		caches = append(caches, cache)
	}
	b.cacheActive = caches[0]
	b.icons = &iconAcquisition{cache: caches[1]}
	b.obsolete = []string{caches[2]}
	// No native session can acknowledge Retire. Even queued obsolete work must
	// remain when the shutdown boundary cannot establish reference release.
	b.shutdown()
	for _, cache := range caches {
		data, err := os.ReadFile(filepath.Join(cache, name))
		if err != nil || !bytes.Equal(data, content) {
			t.Fatal("unconfirmed native retirement removed images", cache, err)
		}
	}
}

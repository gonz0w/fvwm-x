// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

func TestStartupOptionalFieldsPreserveOldCatalogHashes(t *testing.T) {
	old := []byte(`{"Argv":["app","literal"],"Directory":"/work","TerminalCommand":""}`)
	var record launchRecord
	if err := json.Unmarshal(old, &record); err != nil {
		t.Fatal(err)
	}
	data, err := json.Marshal(record)
	if err != nil || string(data) != string(old) {
		t.Fatal("old private action reference changed", string(data), err)
	}
	first, second := sha256.Sum256(old), sha256.Sum256(data)
	if hex.EncodeToString(first[:]) != hex.EncodeToString(second[:]) {
		t.Fatal("old reference hash changed")
	}
	e, err := newActionEncoder(t.TempDir(), "startup", "")
	if err != nil {
		t.Fatal(err)
	}
	defer e.close()
	notify := true
	metadata := &xdgmenu.StartupMetadata{Name: "App", Notify: &notify}
	if _, err := e.encode(xdgmenu.PresentedItem{Argv: []string{"app"}, Startup: metadata}); err != nil {
		t.Fatal(err)
	}
	for _, stored := range e.catalog.Records {
		if !xdgmenu.StartupEnabled(stored.Startup) {
			t.Fatal("private catalog dropped startup metadata", stored)
		}
	}
}

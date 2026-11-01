//go:build go1.25

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filejobs

import (
	"os"
	"path/filepath"
	"testing"
)

func TestCapturedDestinationRefusesReplacementBeforeMutation(t *testing.T) {
	source, destination := t.TempDir(), t.TempDir()
	write(t, filepath.Join(source, "original"), []byte("source stays"))
	req := request(t, source, "move", "original")
	req.Destination = []byte(destination)
	info, err := os.Stat(destination)
	if err != nil {
		t.Fatal(err)
	}
	identity := stamp(info)
	req.DestinationIdentity = &identity
	old := destination + "-old"
	if err := os.Rename(destination, old); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { os.RemoveAll(old) })
	if err := os.Mkdir(destination, 0700); err != nil {
		t.Fatal(err)
	}
	events := run(t, req, nil)
	if events[len(events)-1].State != "failed" || outcome(t, events).DestinationPresent {
		t.Fatal("replacement admitted", events)
	}
	if contents(t, filepath.Join(source, "original")) != "source stays" {
		t.Fatal("source changed")
	}
	if _, err := os.Stat(filepath.Join(destination, "original")); !os.IsNotExist(err) {
		t.Fatal("destination changed", err)
	}
}

func TestCapturedDestinationSurvivesEarlierGroupChanges(t *testing.T) {
	destination := t.TempDir()
	info, err := os.Stat(destination)
	if err != nil {
		t.Fatal(err)
	}
	identity := stamp(info)
	// Every group uses the same captured identity even though the first changes
	// destination size and timestamps. Those mutable values are not a lease.
	for _, name := range []string{"first", "second"} {
		source := t.TempDir()
		write(t, filepath.Join(source, name), []byte(name))
		req := request(t, source, "copy", name)
		req.Destination = []byte(destination)
		req.DestinationIdentity = &identity
		events := run(t, req, nil)
		if events[len(events)-1].State != "completed" {
			t.Fatal("group failed", events)
		}
		if contents(t, filepath.Join(destination, name)) != name {
			t.Fatal("missing group")
		}
	}
	req := request(t, destination, "rename", "first")
	req.Name = []byte("other")
	req.DestinationIdentity = &identity
	if ValidateRequest(req) == nil {
		t.Fatal("identity on non-destination operation")
	}
}

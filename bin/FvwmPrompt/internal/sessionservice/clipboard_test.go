// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionservice

import (
	"github.com/fvwmorg/FvwmPrompt/internal/clipboard"
	"testing"
)

func TestClipboardControlCannotSetLockOrLifetime(t *testing.T) {
	request := RequestMessage{Version: Version, Generation: "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", Operation: "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", Verb: "clipboard", Clipboard: &clipboard.Request{Verb: clipboard.Status, Ref: clipboard.Ref{Epoch: 19}}}
	if err := request.Validate(); err != nil {
		t.Fatal(err)
	}
	for _, verb := range []uint64{clipboard.State, clipboard.Quit, 0, 100} {
		request.Clipboard.Verb = verb
		if err := request.Validate(); err == nil {
			t.Fatalf("admitted verb %d", verb)
		}
	}
	request.Clipboard.Verb = clipboard.Status
	request.Verb = "status"
	if err := request.Validate(); err == nil {
		t.Fatal("clipboard fields attached to unrelated status")
	}
}

func TestClipboardDiscoveryAndPagingFieldsAreVerbScoped(t *testing.T) {
	request := RequestMessage{Version: Version, Generation: "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", Operation: "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", Verb: "clipboard-status"}
	if err := request.Validate(); err != nil {
		t.Fatal(err)
	}
	request.Clipboard = &clipboard.Request{Verb: clipboard.State, Ref: clipboard.Ref{Epoch: 19}, Value: clipboard.Unlocked}
	if request.Validate() == nil {
		t.Fatal("discovery accepted a hidden unlock")
	}
	request.Clipboard = nil
	request.Verb = "clipboard-list"
	request.ClipboardPage = &clipboard.PageRequest{Epoch: 19, Limit: 16}
	if err := request.Validate(); err != nil {
		t.Fatal(err)
	}
	request.ClipboardPage.Limit = 17
	if request.Validate() == nil {
		t.Fatal("unbounded page admitted")
	}
	request.ClipboardPage.Limit = 16
	request.Verb = "configure"
	request.Config = &Config{Bus: "off", Notifications: "off", Power: "portable"}
	if request.Validate() == nil {
		t.Fatal("configuration accepted hidden clipboard paging")
	}
}

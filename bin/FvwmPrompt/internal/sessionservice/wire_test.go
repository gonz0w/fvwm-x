// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionservice

import (
	"bytes"
	"encoding/binary"
	"strings"
	"testing"
)

func TestFixedVerbsAndPayloadOwnership(t *testing.T) {
	identity := strings.Repeat("a", 32)
	for _, request := range []RequestMessage{
		{Version: 1, Verb: "status"},
		{Version: 1, Verb: "configure", Generation: identity, Operation: identity, Config: &Config{Bus: "start", Notifications: "external", DND: true}},
		{Version: 1, Verb: "notify", Generation: identity, Operation: identity, Summary: "Plain <text>", Body: "Body"},
		{Version: 1, Verb: "close", Generation: identity, Operation: identity, NotificationID: 7},
	} {
		if err := request.Validate(); err != nil {
			t.Fatal(err)
		}
	}
	for _, request := range []RequestMessage{
		{Version: 1, Verb: "exec", Generation: identity, Operation: identity},
		{Version: 1, Verb: "reprobe"},
		{Version: 1, Verb: "status", Summary: "not a status field"},
		{Version: 1, Verb: "notify", Generation: identity, Operation: identity, Summary: "line\nbreak"},
		{Version: 1, Verb: "notify", Generation: identity, Operation: identity, Summary: strings.Repeat("x", 161)},
		{Version: 1, Verb: "configure", Generation: identity, Operation: identity, Config: &Config{Bus: "custom", Notifications: "off"}},
	} {
		if err := request.Validate(); err == nil {
			t.Fatalf("accepted %#v", request)
		}
	}
}

func TestFramingRefusesLengthBeforePayloadAndUnknownFields(t *testing.T) {
	var header [4]byte
	binary.BigEndian.PutUint32(header[:], MaxFrame+1)
	if _, err := ReadRequest(bytes.NewReader(header[:])); err == nil {
		t.Fatal("oversized frame admitted")
	}
	for _, body := range []string{`null`, `{"capability":"x","request":{"version":1,"verb":"status","argv":["evil"]}}`, `{"request":{"version":1,"verb":"status"}} {}`} {
		binary.BigEndian.PutUint32(header[:], uint32(len(body)))
		frame := append(append([]byte(nil), header[:]...), []byte(body)...)
		if _, err := ReadRequest(bytes.NewReader(frame)); err == nil {
			t.Fatalf("accepted %s", body)
		}
	}
	var output bytes.Buffer
	envelope := Envelope{Capability: strings.Repeat("f", 64), Request: RequestMessage{Version: 1, Verb: "status"}}
	if err := writeFrame(&output, envelope); err != nil {
		t.Fatal(err)
	}
	got, err := ReadRequest(&output)
	if err != nil || got.Capability != envelope.Capability || got.Request.Verb != "status" {
		t.Fatalf("%#v %v", got, err)
	}
}

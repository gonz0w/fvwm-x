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
		{Version: Version, Verb: "status"},
		{Version: Version, Verb: "configure", Generation: identity, Operation: identity, Config: &Config{Power: "portable", Bus: "start", Notifications: "external", DND: true}},
		{Version: Version, Verb: "notify", Generation: identity, Operation: identity, Summary: "Plain <text>", Body: "Body"},
		{Version: Version, Verb: "close", Generation: identity, Operation: identity, NotificationID: 7},
	} {
		if err := request.Validate(); err != nil {
			t.Fatal(err)
		}
	}
	for _, request := range []RequestMessage{
		{Version: Version, Verb: "exec", Generation: identity, Operation: identity},
		{Version: Version, Verb: "reprobe"},
		{Version: Version, Verb: "status", Summary: "not a status field"},
		{Version: Version, Verb: "notify", Generation: identity, Operation: identity, Summary: "line\nbreak"},
		{Version: Version, Verb: "notify", Generation: identity, Operation: identity, Summary: strings.Repeat("x", 161)},
		{Version: Version, Verb: "configure", Generation: identity, Operation: identity, Config: &Config{Power: "portable", Bus: "custom", Notifications: "off"}},
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
	for _, body := range []string{`null`, `{"capability":"x","request":{"version":2,"verb":"status","argv":["evil"]}}`, `{"request":{"version":2,"verb":"status"}} {}`} {
		binary.BigEndian.PutUint32(header[:], uint32(len(body)))
		frame := append(append([]byte(nil), header[:]...), []byte(body)...)
		if _, err := ReadRequest(bytes.NewReader(frame)); err == nil {
			t.Fatalf("accepted %s", body)
		}
	}
	var output bytes.Buffer
	envelope := Envelope{Capability: strings.Repeat("f", 64), Request: RequestMessage{Version: Version, Verb: "status"}}
	if err := writeFrame(&output, envelope); err != nil {
		t.Fatal(err)
	}
	got, err := ReadRequest(&output)
	if err != nil || got.Capability != envelope.Capability || got.Request.Verb != "status" {
		t.Fatalf("%#v %v", got, err)
	}
}

func TestProviderFieldsAndSecureReadinessAreBounded(t *testing.T) {
	identity := strings.Repeat("b", 32)
	request := RequestMessage{Version: Version, Verb: "poweroff", Generation: identity, Operation: identity, ProviderGeneration: identity}
	if err := request.Validate(); err != nil {
		t.Fatal(err)
	}
	request.ProviderGeneration = ""
	if err := request.Validate(); err == nil {
		t.Fatal("power mutation lacks provider observation")
	}
	request.Verb = "status"
	request.ProviderGeneration = identity
	if err := request.Validate(); err == nil {
		t.Fatal("status smuggles provider field")
	}
	locker := LockerObservation{Generation: identity, State: "running", SecureReady: true}
	if err := locker.Validate(); err == nil {
		t.Fatal("invented secure readiness accepted")
	}
	power := PowerObservation{Provider: "login1", Generation: identity, State: "ready", Capabilities: []PowerCapability{{Action: "poweroff", Permission: "yes"}, {Action: "reboot", Permission: "challenge"}, {Action: "suspend", Permission: "inhibitor-blocked"}}}
	if err := power.Validate(); err != nil {
		t.Fatal(err)
	}
	power.Capabilities[0].Permission = "future-answer"
	if err := power.Validate(); err == nil {
		t.Fatal("unknown provider permission accepted")
	}
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filetransfer

import (
	"bytes"
	"encoding/base64"
	"fmt"
	"strings"
	"testing"
)

func TestBytePreservingLocalURIs(t *testing.T) {
	paths := [][]byte{[]byte("/tmp/a%20 b"), append([]byte("/other/"), 0xff, '#', '\n')}
	text, err := Encode(paths, "actual-host")
	if err != nil {
		t.Fatal(err)
	}
	groups, err := Parse(text, "actual-host")
	if err != nil {
		t.Fatal(err)
	}
	if len(groups) != 2 || !bytes.Equal(groups[0].Paths[0], paths[0]) || !bytes.Equal(groups[1].Paths[0], paths[1]) {
		t.Fatalf("lost bytes: %#v", groups)
	}
	groups, err = Parse("file://localhost/tmp/a%2520\r\nfile:///tmp/a%2520\n", "actual-host")
	if err != nil || len(groups) != 1 || len(groups[0].Paths) != 1 || string(groups[0].Paths[0]) != "/tmp/a%20" {
		t.Fatal("decode or dedupe contract")
	}
}
func TestRefuseNonlocalMalformedAndOverLimits(t *testing.T) {
	for _, value := range []string{"file://remote/tmp/a", "file://host:22/tmp/a", "file://me@host/tmp/a", "file:///", "file:///tmp/../a", "file:///tmp/%2e/a", "file:///tmp/%00", "file:///tmp/a?x", "file:///tmp/a#x", "file:///tmp/%xy", "file:///tmp/a b", "https:///tmp/a"} {
		if _, err := Parse(value, "host"); err == nil {
			t.Errorf("accepted %q", value)
		}
	}
	if _, err := Parse(strings.Repeat("file:///a/x\n", 257), "host"); err == nil {
		t.Fatal("reference limit")
	}
	var groups strings.Builder
	for i := 0; i < 9; i++ {
		fmt.Fprintf(&groups, "file:///g%d/a\n", i)
	}
	if _, err := Parse(groups.String(), "host"); err == nil {
		t.Fatal("group limit")
	}
}
func TestLeaseDoesNotClearReplacementOrForeign(t *testing.T) {
	l := Lease{"10", "move"}
	if !l.ClearAfter("10", "move", true) {
		t.Fatal("own successful move")
	}
	for _, tc := range []struct {
		id, action string
		ok         bool
	}{{"9", "move", true}, {"", "move", true}, {"10", "copy", true}, {"10", "move", false}} {
		if l.ClearAfter(tc.id, tc.action, tc.ok) {
			t.Fatal("clears unrelated lease")
		}
	}
	l.Lost("9")
	if l.ID != "10" {
		t.Fatal("foreign loss")
	}
	l.Lost("10")
	if l.ID != "" {
		t.Fatal("lost current")
	}
}
func TestStrictSchemaAndSelection(t *testing.T) {
	for _, data := range []string{`{"v":1,"v":1,"event":"ready"}`, `{"v":1,"event":"ready","payload":"secret"}`, `{"v":1,"id":"01","status":"ok"}`, `{"v":1,"id":1,"status":"ok"}`, `{"v":1,"event":null}`, `{"v":1,"event":"ready"} {}`} {
		if _, err := Decode([]byte(data)); err == nil {
			t.Errorf("accepted %s", data)
		}
	}
	e := Event{Rows: 9, Selection: base64.StdEncoding.EncodeToString([]byte{0x81, 1})}
	rows, err := e.Selected()
	if err != nil || len(rows) != 3 || rows[2] != 8 {
		t.Fatal("bit mapping")
	}
	e.Selection = base64.StdEncoding.EncodeToString([]byte{0, 2})
	if _, err = e.Selected(); err == nil {
		t.Fatal("padding")
	}
	e.Selection = "AQ==\n"
	e.Rows = 1
	if _, err = e.Selected(); err == nil {
		t.Fatal("noncanonical base64")
	}
	e.Rows = 4097
	if _, err = e.Selected(); err == nil {
		t.Fatal("row bound")
	}
}

// These are the complete private native/browser schemas, rather than a generic
// registry. Updating one requires updating its native producer/consumer too.
func TestDocumentedNativeEventSchemas(t *testing.T) {
	for _, frame := range []string{
		`{"v":1,"event":"ready","hostname":"local-host"}`,
		`{"v":1,"id":"1","status":"ok"}`,
		`{"v":1,"id":"3","status":"ok","transfer":"100"}`,
		`{"v":1,"id":"2","status":"ok","lease":"10"}`,
		`{"v":1,"event":"attached","epoch":"3","generation":7,"frame":100,"list":101}`,
		`{"v":1,"event":"action","transfer":"1","kind":"copy","epoch":"3","generation":7,"time":123,"rows":1,"selection":"AQ=="}`,
		`{"v":1,"event":"drag-begin","transfer":"2","epoch":"3","generation":7,"time":124,"rows":1,"selection":"AQ=="}`,
		`{"v":1,"event":"proposal","id":"4","transfer":"3","kind":"clipboard","action":"move","uris":"file:///tmp/entry\r\n","epoch":"3","generation":7,"lease":"10"}`,
		`{"v":1,"event":"proposal","transfer":"4","kind":"dnd","action":"copy","uris":"file:///tmp/entry\r\n","epoch":"3","generation":7}`,
		`{"v":1,"event":"result","transfer":"4","status":"failed","action":"copy"}`,
		`{"v":1,"event":"lost","lease":"10"}`,
		`{"v":1,"event":"unavailable","code":"no_display"}`,
	} {
		if _, err := Decode([]byte(frame)); err != nil {
			t.Fatal("documented native schema rejected", err)
		}
	}
	for _, frame := range []string{
		`{"v":1,"event":"ready","hostname":"host","uris":"file:///tmp/entry"}`,
		`{"v":1,"event":"attached","epoch":"3","generation":7,"frame":100,"list":101,"selection":"AQ=="}`,
		`{"v":1,"event":"result","transfer":"4","status":"success","action":"delete"}`,
		`{"v":1,"event":"action","transfer":"1","kind":"paste","epoch":"3","generation":7,"time":123}`,
	} {
		if _, err := Decode([]byte(frame)); err == nil {
			t.Fatal("unrecognized event shape accepted")
		}
	}
}

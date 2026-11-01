// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filetransfer

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func helper(t *testing.T, body string) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), "helper")
	if err := os.WriteFile(path, []byte("#!/bin/sh\n"+body), 0700); err != nil {
		t.Fatal(err)
	}
	return path
}
func TestClientEOFWaitAndPrivateFraming(t *testing.T) {
	marker := filepath.Join(t.TempDir(), "waited")
	// The helper writes its final marker only after browser stdin EOF. Close
	// cannot return early merely because the command queue has been retired.
	path := helper(t, `printf '{"v":1,"event":"ready","hostname":"local-host"}\n'
IFS= read -r command || exit 2
printf '{"v":1,"id":"1","status":"ok"}\n'
cat >/dev/null
printf done > '`+marker+`'
`)
	c, err := Start(path, filepath.Join(t.TempDir(), "form"), strings.Repeat("a", 32))
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()
	select {
	case e := <-c.Events:
		if e.Event != "ready" {
			t.Fatal(e.Event)
		}
		c.Received(e)
	case <-time.After(3 * time.Second):
		t.Fatal("startup")
	}
	id, err := c.Send(Command{Command: "expect-form", Epoch: "1"})
	if err != nil || id != "1" {
		t.Fatal(id, err)
	}
	select {
	case e := <-c.Events:
		if e.ID != id || e.Status != "ok" {
			t.Fatal("correlation")
		}
		c.Received(e)
	case <-time.After(3 * time.Second):
		t.Fatal("reply")
	}
	c.Close()
	if data, err := os.ReadFile(marker); err != nil || string(data) != "done" {
		t.Fatal("Close preceded Wait", err)
	}
	if _, err := c.Send(Command{Command: "read"}); err == nil {
		t.Fatal("send after EOF")
	}
}
func TestClientRefusesUnterminatedAndDuplicateFrames(t *testing.T) {
	for _, body := range []string{`printf '{"v":1,"event":"ready","hostname":"host"}'`, `printf '{"v":1,"v":1,"event":"ready","hostname":"host"}\n'`} {
		c, err := Start(helper(t, body), filepath.Join(t.TempDir(), "form"), strings.Repeat("a", 32))
		if err != nil {
			t.Fatal(err)
		}
		select {
		case <-c.Done:
		case <-time.After(3 * time.Second):
			c.Close()
			t.Fatal("invalid helper did not retire")
		}
		select {
		case <-c.Events:
			t.Fatal("admitted invalid frame")
		default:
		}
		c.Close()
	}
}

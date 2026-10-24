// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"strings"
	"testing"
)

func TestOneShotRefusesMalformedRequest(t *testing.T) {
	for _, value := range []string{"", `[]`, `{"version":1,"operation":"unknown"}`, strings.Repeat("x", 65537)} {
		var output bytes.Buffer
		if err := run(strings.NewReader(value), &output); err == nil || output.Len() != 0 {
			t.Fatalf("%v %q", err, output.String())
		}
	}
}
func TestOneShotEmptyApplicationRoots(t *testing.T) {
	var output bytes.Buffer
	if err := run(strings.NewReader(`{"version":1,"operation":"apps","directories":[]}`), &output); err != nil || output.String() != "{\"version\":1,\"result\":[]}\n" {
		t.Fatalf("%v %q", err, output.String())
	}
}

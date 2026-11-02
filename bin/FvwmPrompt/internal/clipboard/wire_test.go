// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package clipboard

import (
	"strings"
	"testing"
)

func TestNumericOnlyProjection(t *testing.T) {
	line := "1 5 0 19 7 2 1 42 10000 1 1 2 65536 32768\n"
	reply, err := parseReply([]byte(line))
	if err != nil || reply.Ref != (Ref{19, 7, 2}) || reply.Length != 42 || reply.Mode != Unlocked {
		t.Fatalf("projection: %+v %v", reply, err)
	}
	for _, bad := range []string{
		strings.Replace(line, "42", "password", 1), strings.Replace(line, "42", "+42", 1),
		strings.Replace(line, "42", "042", 1), strings.Replace(line, "42", "18446744073709551616", 1),
		strings.TrimSuffix(line, "\n"), line + "\n", strings.Replace(line, "1 5", "1 0", 1),
		"1 5 0 19 0 0 0 42 0 0 0 0 0 0\n", "1 5 0 19 7 2 1 42 10000 1 2 2 65536 32768\n",
		"1 5 0 19 0 0 0 0 0 0 0 1 0 0\n",
	} {
		if _, err := parseReply([]byte(bad)); err == nil {
			t.Fatalf("accepted invalid frame %q", bad)
		}
	}
}

func TestExactReferenceControls(t *testing.T) {
	for _, request := range []Request{{Verb: Pin, Ref: Ref{19, 7, 2}, Value: 1}, {Verb: List, Ref: Ref{Epoch: 19}, Value: 3}, {Verb: State, Ref: Ref{Epoch: 19}, Value: Locked}} {
		if err := request.Validate(19); err != nil {
			t.Fatal(err)
		}
	}
	for _, request := range []Request{{Verb: Pin, Ref: Ref{Epoch: 19}}, {Verb: Remove, Ref: Ref{19, 7, 2}, Value: 1}, {Verb: Status, Ref: Ref{20, 0, 0}}, {Verb: State, Ref: Ref{Epoch: 19}, Value: 3}} {
		if err := request.Validate(19); err == nil {
			t.Fatalf("accepted %+v", request)
		}
	}
}

func TestRepliesBindToTheirOperationShape(t *testing.T) {
	row := Reply{Sequence: 1, Ref: Ref{19, 7, 2}, Representation: 1, Length: 5, Deadline: 10000, Mode: Unlocked}
	if row.ValidateFor(Request{Verb: List, Ref: Ref{Epoch: 19}}) != nil {
		t.Fatal("valid list refused")
	}
	for _, request := range []Request{{Verb: Status, Ref: Ref{Epoch: 19}}, {Verb: Pin, Ref: Ref{19, 7, 2}, Value: 1}, {Verb: List, Ref: Ref{Epoch: 20}}} {
		if row.ValidateFor(request) == nil {
			t.Fatalf("unsolicited item accepted for %+v", request)
		}
	}
	reply := Reply{Sequence: 1, Ref: Ref{Epoch: 19}, Mode: Unlocked}
	if reply.ValidateFor(Request{Verb: State, Ref: Ref{Epoch: 19}, Value: Locked}) == nil {
		t.Fatal("wrong state acknowledgment accepted")
	}
	if reply.ValidateFor(Request{Verb: List, Ref: Ref{Epoch: 19}}) == nil {
		t.Fatal("successful empty list item accepted")
	}
}

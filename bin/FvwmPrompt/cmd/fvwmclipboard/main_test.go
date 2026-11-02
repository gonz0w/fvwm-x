// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/clipboard"
	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
)

func identityReply() sessionservice.Reply {
	return sessionservice.Reply{Generation: strings.Repeat("a", 32), Operation: strings.Repeat("b", 32)}
}
func readyCapabilities() clipboard.Capabilities {
	limits, _ := clipboard.DefaultTextLimits(4096)
	return clipboard.Capabilities{Version: 1, State: "ready", Reason: "ready", Epoch: 19, Mode: clipboard.Unlocked, Limits: &limits, SupportedActions: clipboard.MetadataActions, ProcessPrepared: true, RequiredStorage: "native-locked-closed", Format: "UTF8_STRING", XlibReplyMax: 32768}
}

func TestMetadataSyntaxRejectsUnboundedOrAmbiguousActions(t *testing.T) {
	cases := [][]string{{"copy"}, {"search", "text"}, {"state"}, {"quit"}, {"list", "--limit", "17"}, {"list", "--limit", "0"}, {"list", "--offset", "4097"}, {"clear"}, {"clear", "--epoch", "+19"}, {"clear", "--epoch", "019"}, {"clear", "--epoch", "0x13"}, {"clear", "--epoch", "18446744073709551616"}, {"clear", "--epoch", "19", "--epoch", "20"}, {"pin", "--epoch", "19", "--item", "7"}, {"status", "--epoch", "19"}, {"clear", "--epoch", "19", "extra"}}
	for _, args := range cases {
		var out, diag bytes.Buffer
		called := false
		code := run(args, &out, &diag, func(context.Context, sessionservice.RequestMessage) (sessionservice.Reply, error) {
			called = true
			return sessionservice.Reply{}, nil
		})
		if code != 2 || called || out.Len() != 0 {
			t.Fatalf("%v: code%d called%t out%q", args, code, called, out.String())
		}
	}
}

func TestDisabledStatusIsAnObservationWithoutStartingAnything(t *testing.T) {
	var out, diag bytes.Buffer
	requests := 0
	code := run(nil, &out, &diag, func(_ context.Context, request sessionservice.RequestMessage) (sessionservice.Reply, error) {
		requests++
		if request.Verb == "status" {
			return identityReply(), nil
		}
		if request.Verb != "clipboard-status" || request.Validate() != nil {
			t.Fatalf("unexpected %+v", request)
		}
		caps := clipboard.DisabledCapabilities()
		return sessionservice.Reply{ClipboardCapabilities: &caps}, nil
	})
	var caps clipboard.Capabilities
	if code != 0 || requests != 2 || diag.Len() != 0 || json.Unmarshal(out.Bytes(), &caps) != nil || caps.State != "disabled" {
		t.Fatalf("code%d requests%d output%q errors%q", code, requests, out.String(), diag.String())
	}
}

func TestListUsesCurrentEpochAndOneBoundedPage(t *testing.T) {
	var out, diag bytes.Buffer
	requests := 0
	code := run([]string{"list", "--offset", "1", "--limit", "16"}, &out, &diag, func(_ context.Context, request sessionservice.RequestMessage) (sessionservice.Reply, error) {
		requests++
		switch request.Verb {
		case "status":
			return identityReply(), nil
		case "clipboard-status":
			caps := readyCapabilities()
			return sessionservice.Reply{ClipboardCapabilities: &caps}, nil
		case "clipboard-list":
			if request.Validate() != nil || request.ClipboardPage == nil || request.ClipboardPage.Epoch != 19 || request.ClipboardPage.Offset != 1 || request.ClipboardPage.Limit != 16 {
				t.Fatalf("bad page %+v", request)
			}
			page := clipboard.Page{Version: 1, Epoch: 19, Offset: 1, Next: 17, Slots: 32, Items: []clipboard.Descriptor{{Slot: 1, Ref: clipboard.Ref{Epoch: 19, Item: 7, Generation: 2}, Representation: 1, Length: 5, Deadline: 10000}}}
			return sessionservice.Reply{ClipboardPage: &page}, nil
		default:
			t.Fatalf("unexpected %s", request.Verb)
			return sessionservice.Reply{}, nil
		}
	})
	var page clipboard.Page
	if code != 0 || requests != 4 || diag.Len() != 0 || json.Unmarshal(out.Bytes(), &page) != nil || len(page.Items) != 1 || page.Next != 17 {
		t.Fatalf("code%d requests%d out%q err%q", code, requests, out.String(), diag.String())
	}
	if strings.Contains(out.String(), "capability") || strings.Contains(out.String(), "operation") {
		t.Fatal("session transport identity leaked into public output")
	}
}

func TestStaleActionIsNotRetriedOrReplaced(t *testing.T) {
	var out, diag bytes.Buffer
	requests := 0
	code := run([]string{"unpin", "--epoch", "19", "--item", "7", "--generation", "2"}, &out, &diag, func(_ context.Context, request sessionservice.RequestMessage) (sessionservice.Reply, error) {
		requests++
		if request.Verb == "status" {
			return identityReply(), nil
		}
		if request.Clipboard == nil || request.Clipboard.Ref != (clipboard.Ref{19, 7, 2}) || request.Clipboard.Verb != clipboard.Pin || request.Clipboard.Value != 0 {
			t.Fatalf("changed action %+v", request)
		}
		result := clipboard.Reply{Sequence: 1, Result: clipboard.Stale, Ref: clipboard.Ref{Epoch: 19}, Mode: clipboard.Unlocked}
		return sessionservice.Reply{Clipboard: &result}, nil
	})
	if code != 1 || requests != 2 || diag.Len() != 0 || !strings.Contains(out.String(), `"result":3`) {
		t.Fatalf("code%d requests%d out%q err%q", code, requests, out.String(), diag.String())
	}
}

func TestTransportFailurePrintsNoPartialMetadata(t *testing.T) {
	var out, diag bytes.Buffer
	code := run([]string{"status"}, &out, &diag, func(context.Context, sessionservice.RequestMessage) (sessionservice.Reply, error) {
		return sessionservice.Reply{}, errors.New("owner unavailable")
	})
	if code != 1 || out.Len() != 0 || !strings.Contains(diag.String(), "session control request failed") {
		t.Fatalf("code%d %q %q", code, out.String(), diag.String())
	}
}

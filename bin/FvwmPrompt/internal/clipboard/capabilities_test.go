// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package clipboard

import (
	"context"
	"testing"
	"time"
)

func TestTextDefaultsAccountForNativePages(t *testing.T) {
	for _, test := range []struct{ page, mapped, locked uint64 }{{4096, 1310720, 1048576}, {16384, 2097152, 1048576}, {65536, 6291456, 2097152}} {
		limits, err := DefaultTextLimits(test.page)
		if err != nil || limits.Items != 32 || limits.PerItem != 32768 || limits.Total != 1048576 || limits.Lifetime != 600000 || limits.Mapped != test.mapped || limits.Locked != test.locked {
			t.Fatalf("page %d: %+v %v", test.page, limits, err)
		}
	}
	for _, page := range []uint64{0, ^uint64(0)} {
		if limits, err := DefaultTextLimits(page); err == nil || limits != (Limits{}) {
			t.Fatalf("overflow accepted %+v %v", limits, err)
		}
	}
}

func TestCapabilitiesDistinguishDisabledBlockedAndUnavailable(t *testing.T) {
	if err := DisabledCapabilities().Validate(); err != nil {
		t.Fatal(err)
	}
	owner := peer(t, "echo")
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	caps := owner.Capabilities(ctx)
	if err := caps.Validate(); err != nil || caps.State != "blocked" || caps.Reason != "unknown-lock" || !caps.ProcessPrepared {
		t.Fatalf("initial %+v %v", caps, err)
	}
	if _, err := owner.Call(ctx, Request{Verb: State, Ref: Ref{Epoch: 19}, Value: Locked}); err != nil {
		t.Fatal(err)
	}
	caps = owner.Capabilities(ctx)
	if err := caps.Validate(); err != nil || caps.Reason != "locked" {
		t.Fatalf("locked %+v %v", caps, err)
	}
	owner.Close()
	if _, err := owner.Wait(ctx); err != nil {
		t.Fatal(err)
	}
	caps = owner.Capabilities(ctx)
	if err := caps.Validate(); err != nil || caps.State != "unavailable" || caps.ProcessPrepared {
		t.Fatalf("ended %+v %v", caps, err)
	}
	caps.XlibReplySwapExcluded = true
	if caps.Validate() == nil {
		t.Fatal("unsupported swap guarantee accepted")
	}
}

func TestPagesBoundWorkAndDropDescriptorsWhenLockChanges(t *testing.T) {
	for _, mode := range []string{"page", "page-lock", "page-final-lock"} {
		t.Run(mode, func(t *testing.T) {
			owner := peer(t, mode)
			ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
			defer cancel()
			if _, err := owner.Call(ctx, Request{Verb: State, Ref: Ref{Epoch: 19}, Value: Unlocked}); err != nil {
				t.Fatal(err)
			}
			request := PageRequest{Epoch: 19, Offset: 0, Limit: 4}
			result, err := owner.Page(ctx, request)
			if err != nil || result.Validate(request) != nil {
				t.Fatalf("page %+v %v", result, err)
			}
			if mode != "page" {
				if result.Result != Blocked || len(result.Items) != 0 || result.Next != 0 {
					t.Fatalf("locked partial page leaked %+v", result)
				}
				return
			}
			if result.Result != OK || len(result.Items) != 1 || result.Items[0].Ref != (Ref{19, 7, 2}) || result.Next != 4 || !result.Complete {
				t.Fatalf("page %+v", result)
			}
			request.Epoch = 20
			stale, err := owner.Page(ctx, request)
			if err != nil || stale.Result != Stale || len(stale.Items) != 0 {
				t.Fatalf("epoch %+v %v", stale, err)
			}
		})
	}
}

func TestPageValidationRefusesNoProgressAndOversizedResults(t *testing.T) {
	request := PageRequest{Epoch: 19, Limit: 4}
	page := Page{Version: 1, Epoch: 19, Slots: 4, Next: 4, Complete: true, Items: []Descriptor{{Slot: 0, Ref: Ref{19, 7, 2}, Representation: 1, Length: 5, Deadline: 10000}}}
	if page.Validate(request) != nil {
		t.Fatal("valid page refused")
	}
	page.Next, page.Complete = 0, false
	if page.Validate(request) == nil {
		t.Fatal("zero-progress page accepted")
	}
	page.Next, page.Complete = 4, true
	page.Items[0].Ref.Epoch = 20
	if page.Validate(request) == nil {
		t.Fatal("mixed-owner page accepted")
	}
	page.Items[0].Ref.Epoch = 19
	page.Result = Blocked
	if page.Validate(request) == nil {
		t.Fatal("blocked page retained items")
	}
	request.Limit = MaxPage + 1
	if request.Validate() == nil {
		t.Fatal("unbounded page admitted")
	}
}

func TestUnobservedStatusDoesNotClaimNativeExit(t *testing.T) {
	owner := peer(t, "echo")
	cancelled, cancel := context.WithCancel(context.Background())
	cancel()
	caps := owner.Capabilities(cancelled)
	if caps.Reason != "native-status-unconfirmed" || caps.ProcessPrepared || caps.Validate() != nil {
		t.Fatalf("cancelled %+v", caps)
	}
	live, stop := context.WithTimeout(context.Background(), 3*time.Second)
	defer stop()
	if _, err := owner.Call(live, Request{Verb: Status, Ref: Ref{Epoch: 19}}); err != nil {
		t.Fatalf("cancelled observation ended owner: %v", err)
	}
	busy := &Client{epoch: 19, config: owner.config, calls: make(chan call, 1), stop: make(chan struct{}), done: make(chan struct{})}
	busy.calls <- call{ctx: live, reply: make(chan callResult, 1)}
	caps = busy.Capabilities(live)
	if caps.Reason != "native-status-unconfirmed" || caps.ProcessPrepared || caps.Validate() != nil {
		t.Fatalf("busy %+v", caps)
	}
	select {
	case <-busy.done:
		t.Fatal("busy observation ended slot")
	default:
	}
}

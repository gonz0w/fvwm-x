// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package clipboard

import (
	"bufio"
	"context"
	"fmt"
	"os"
	"strconv"
	"strings"
	"testing"
	"time"
)

// Independent child protocol peer. It never reads/writes clipboard content.
func TestMain(m *testing.M) {
	if mode := os.Getenv("FVWM_TEST_CLIPBOARD_PEER"); mode != "" {
		if len(os.Args) != 9 || os.Args[1] != "--serve" {
			os.Exit(64)
		}
		epoch := os.Args[2]
		state := uint64(Unknown)
		reader := bufio.NewScanner(os.Stdin)
		for reader.Scan() {
			fields := strings.Fields(reader.Text())
			if len(fields) != 7 {
				os.Exit(65)
			}
			if mode == "stall" {
				time.Sleep(10 * time.Second)
				continue
			}
			if mode == "malformed" {
				fmt.Println("payload is not metadata")
				continue
			}
			if fields[2] == strconv.FormatUint(State, 10) {
				state, _ = strconv.ParseUint(fields[6], 10, 64)
			}
			sequence := fields[1]
			if mode == "wrong-sequence" {
				sequence = "900"
			}
			fmt.Printf("1 %s 0 %s 0 0 0 0 0 0 %d 0 0 0\n", sequence, epoch, state)
		}
		os.Exit(0)
	}
	os.Exit(m.Run())
}

func peer(t *testing.T, mode string) *Client {
	t.Helper()
	executable, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	config := Config{Path: executable, Epoch: 19, Items: 4, Bytes: 32768, Total: 131072, Mapped: 196608, Locked: 131072, LifetimeMilliseconds: 10000}
	environment := append(os.Environ(), "FVWM_TEST_CLIPBOARD_PEER="+mode)
	owner, err := Start(config, environment)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		owner.Close()
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		if _, err := owner.Wait(ctx); err != nil {
			t.Error(err)
		}
	})
	return owner
}

func TestOwnerConfirmedCleanupAndExactReplies(t *testing.T) {
	owner := peer(t, "echo")
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	for _, request := range []Request{{Verb: Status, Ref: Ref{Epoch: 19}}, {Verb: State, Ref: Ref{Epoch: 19}, Value: Locked}, {Verb: List, Ref: Ref{Epoch: 19}}} {
		reply, err := owner.Call(ctx, request)
		if err != nil || reply.Ref.Epoch != 19 {
			t.Fatalf("reply %+v %v", reply, err)
		}
		if request.Verb != Status && reply.Mode != Locked {
			t.Fatal("lost lock mode")
		}
	}
	owner.Close()
	terminal, err := owner.Wait(ctx)
	if err != nil || !terminal.Started || !terminal.Reaped || !terminal.Clean {
		t.Fatalf("terminal %+v %v", terminal, err)
	}
	if _, err := owner.Call(ctx, Request{Verb: Status, Ref: Ref{Epoch: 19}}); err == nil {
		t.Fatal("ended owner accepted work")
	}
}

func TestOwnerRefusesMalformedOrUncorrelatedReplies(t *testing.T) {
	for _, mode := range []string{"malformed", "wrong-sequence"} {
		t.Run(mode, func(t *testing.T) {
			owner := peer(t, mode)
			ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
			defer cancel()
			if _, err := owner.Call(ctx, Request{Verb: Status, Ref: Ref{Epoch: 19}}); err == nil {
				t.Fatal("invalid response accepted")
			}
			terminal, err := owner.Wait(ctx)
			if err != nil || !terminal.Started || !terminal.Reaped || terminal.Clean {
				t.Fatalf("terminal %+v %v", terminal, err)
			}
		})
	}
}

func TestOwnerCancelledBarrierCannotResume(t *testing.T) {
	owner := peer(t, "stall")
	ctx, cancel := context.WithTimeout(context.Background(), 80*time.Millisecond)
	defer cancel()
	if _, err := owner.Call(ctx, Request{Verb: State, Ref: Ref{Epoch: 19}, Value: Locked}); err == nil {
		t.Fatal("stalled barrier acknowledged")
	}
	wait, cancelWait := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancelWait()
	terminal, err := owner.Wait(wait)
	if err != nil || !terminal.Reaped || terminal.Clean {
		t.Fatalf("terminal %+v %v", terminal, err)
	}
}

func TestNativeChildMetadata(t *testing.T) {
	path := os.Getenv("FVWM_TEST_NATIVE_CLIPBOARD")
	if path == "" {
		t.Skip("actual native child joins the private-X checkpoint")
	}
	if os.Getenv("FVWM_TEST_PRIVATE_X") != "1" {
		t.Fatal("native test requires runner-owned private X")
	}
	config := Config{Path: path, Epoch: 37, Items: 4, Bytes: 32768, Total: 131072, Mapped: 196608, Locked: 131072, LifetimeMilliseconds: 10000}
	owner, err := Start(config, os.Environ())
	if err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	defer func() {
		owner.Close()
		facts, err := owner.Wait(ctx)
		if err != nil || !facts.Reaped || !facts.Clean {
			t.Errorf("native terminal %+v %v", facts, err)
		}
	}()
	reply, err := owner.Call(ctx, Request{Verb: Status, Ref: Ref{Epoch: 37}})
	if err != nil || reply.Mode != Unknown {
		t.Fatalf("native initial %+v %v", reply, err)
	}
	reply, err = owner.Call(ctx, Request{Verb: List, Ref: Ref{Epoch: 37}})
	if err != nil || reply.Result != Blocked {
		t.Fatalf("native history %+v %v", reply, err)
	}
	reply, err = owner.Call(ctx, Request{Verb: State, Ref: Ref{Epoch: 37}, Value: Locked})
	if err != nil || reply.Result != OK || reply.Mode != Locked {
		t.Fatalf("native barrier %+v %v", reply, err)
	}
}

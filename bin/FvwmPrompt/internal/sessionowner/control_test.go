// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"context"
	"encoding/binary"
	"encoding/json"
	"io"
	"net"
	"os"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
)

func TestPrivateControlRoundTripCASAndNoForeignCapability(t *testing.T) {
	env := serviceEnvironment(t, sessionservice.Defaults())
	control, err := startControl(env)
	if err != nil {
		t.Fatal(err)
	}
	defer control.close()
	cfg := defaults(Config{Environment: env})
	state, err := newServices(&cfg, &reporter{output: io.Discard})
	if err != nil {
		t.Fatal(err)
	}
	finished := make(chan struct{})
	go func() {
		defer close(finished)
		for i := 0; i < 3; i++ {
			request := <-control.requests
			_, reply := state.admit(request, cfg, false)
			request.reply <- reply
		}
	}()
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	status, err := sessionservice.RequestTo(ctx, control.path, control.capability, sessionservice.RequestMessage{Version: sessionservice.Version, Verb: "status"})
	if err != nil {
		t.Fatal(err)
	}
	configure := sessionservice.RequestMessage{Version: sessionservice.Version, Verb: "configure", Generation: status.Generation, Operation: status.Operation, Config: &sessionservice.Config{Power: "portable", Bus: "start", Notifications: "external", DND: true}}
	reply, err := sessionservice.RequestTo(ctx, control.path, control.capability, configure)
	if err != nil || reply.Outcome != "applied" || reply.Status.ActiveBusPolicy != "off" {
		t.Fatalf("%#v %v", reply, err)
	}
	if _, err := sessionservice.RequestTo(ctx, control.path, control.capability, configure); err == nil {
		t.Fatal("replayed control admitted")
	}
	<-finished
	if _, err := sessionservice.RequestTo(ctx, control.path, strings.Repeat("0", 64), sessionservice.RequestMessage{Version: sessionservice.Version, Verb: "status"}); err == nil {
		t.Fatal("foreign capability admitted")
	}
}

func TestControlDisconnectCancelsOnlyRequest(t *testing.T) {
	control, err := startControl(serviceEnvironment(t, sessionservice.Defaults()))
	if err != nil {
		t.Fatal(err)
	}
	defer control.close()
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	clientDone := make(chan error, 1)
	go func() {
		_, err := sessionservice.RequestTo(ctx, control.path, control.capability, sessionservice.RequestMessage{Version: sessionservice.Version, Verb: "status"})
		clientDone <- err
	}()
	request := <-control.requests
	cancel()
	if err := <-clientDone; err == nil {
		t.Fatal("cancelled client succeeded")
	}
	select {
	case <-request.ctx.Done():
	case <-time.After(time.Second):
		t.Fatal("disconnect did not cancel request")
	}
	if _, err := os.Lstat(control.path); err != nil {
		t.Fatal("disconnect removed owner endpoint")
	}
}

func TestControlBoundsConnectionsAndPreservesReplacementInode(t *testing.T) {
	control, err := startControl(serviceEnvironment(t, sessionservice.Defaults()))
	if err != nil {
		t.Fatal(err)
	}
	defer func() {
		if control.directory != nil {
			control.close()
		}
	}()
	connections := make([]net.Conn, 0, 8)
	for i := 0; i < 8; i++ {
		connection, err := net.Dial("unix", control.path)
		if err != nil {
			t.Fatal(err)
		}
		connections = append(connections, connection)
		data, _ := json.Marshal(sessionservice.Envelope{Capability: control.capability, Request: sessionservice.RequestMessage{Version: sessionservice.Version, Verb: "status"}})
		var header [4]byte
		binary.BigEndian.PutUint32(header[:], uint32(len(data)))
		connection.Write(header[:])
		connection.Write(data)
		select {
		case <-control.requests:
		case <-time.After(time.Second):
			t.Fatal("connection was not admitted")
		}
	}
	for _, connection := range connections {
		defer connection.Close()
	}
	extra, err := net.Dial("unix", control.path)
	if err != nil {
		t.Fatal(err)
	}
	defer extra.Close()
	extra.SetReadDeadline(time.Now().Add(time.Second))
	var one [1]byte
	if _, err := extra.Read(one[:]); err != io.EOF {
		t.Fatalf("ninth connection was not refused: %v", err)
	}
	path := control.path
	if err := os.Remove(path); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, []byte("replacement"), 0600); err != nil {
		t.Fatal(err)
	}
	control.close()
	control.directory = nil
	if data, err := os.ReadFile(path); err != nil || string(data) != "replacement" {
		t.Fatalf("replacement was removed: %q %v", data, err)
	}
}

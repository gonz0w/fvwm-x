// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"context"
	"encoding/json"
	"errors"
	"io"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
)

func serviceEnvironment(t *testing.T, config sessionservice.Config) []string {
	t.Helper()
	// Use a short explicitly private path; numbered testing.TempDir children
	// follow umask and need not satisfy the runtime-directory contract.
	directory, err := os.MkdirTemp("/tmp", "fvwm-sess-")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { os.RemoveAll(directory) })
	values := desktopsettings.Defaults()
	values["schema_version"] = desktopsettings.StateVersion
	values["session_bus"] = config.Bus
	values["session_notifications"] = config.Notifications
	values["notification_dnd"] = config.DND
	data, err := json.Marshal(values)
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(directory, "desktop-state.json"), data, 0600); err != nil {
		t.Fatal(err)
	}
	return append(isolatedEnvironment(), "HOME="+directory, "FVWM_USERDIR="+directory, "XDG_RUNTIME_DIR="+directory, "DISPLAY=:72")
}

func TestServiceChild(t *testing.T) {
	index := -1
	for i, arg := range os.Args {
		if arg == "--service" {
			index = i
			break
		}
	}
	if index < 0 {
		return
	}
	mode := os.Args[index+1]
	args := os.Args[index+2:]
	switch mode {
	case "hang":
		time.Sleep(5 * time.Second)
	case "hang-ready":
		file := os.NewFile(3, "ready")
		file.Write([]byte("R"))
		file.Close()
		time.Sleep(5 * time.Second)
	case "overflow":
		io.WriteString(os.Stdout, strings.Repeat("x", sessionservice.MaxFrame+1))
	case "daemon":
		sig := make(chan os.Signal, 1)
		signal.Notify(sig, syscall.SIGTERM)
		file := os.NewFile(3, "address")
		io.WriteString(file, "unix:path=/fixture/owned-bus\n")
		file.Close()
		<-sig
		if path := os.Getenv("FVWM_SESSION_TEST_DAEMON_STOP"); path != "" {
			os.WriteFile(path, []byte("stopped"), 0600)
		}
	case "helper", "disabled":
		reply := busReply{Version: 1, Available: mode == "helper", State: "unavailable", Detail: "missing_inherited_session_address"}
		if mode == "disabled" {
			reply.Detail = "optional_feature_disabled"
		} else if os.Getenv("DBUS_SESSION_BUS_ADDRESS") != "" {
			reply.State = "ready"
			reply.BusID = "fixture-bus"
			reply.ServerOwner = ":1.27"
			reply.Capabilities = []string{"body"}
			reply.Detail = "authenticated"
			if len(args) > 0 && args[0] == "notify" {
				if len(args) != 5 || args[3] != reply.ServerOwner || args[4] != reply.BusID {
					os.Exit(94)
				}
				reply.State = "sent"
				reply.NotificationID = 7
			}
			if len(args) > 0 && args[0] == "close" {
				if len(args) != 4 || args[1] != "7" || args[2] != reply.ServerOwner || args[3] != reply.BusID {
					os.Exit(94)
				}
				reply.State = "closed"
				reply.NotificationID = 7
			}
		}
		json.NewEncoder(os.Stdout).Encode(reply)
	default:
		os.Exit(93)
	}
	os.Exit(0)
}

func serviceCommand(mode string, args ...string) *exec.Cmd {
	return exec.Command(os.Args[0], append([]string{"-test.run=^TestServiceChild$", "--", "--service", mode}, args...)...)
}

func TestBusOffPreservesEnvironmentWithoutStartingAdapter(t *testing.T) {
	env := append(serviceEnvironment(t, sessionservice.Defaults()), "DBUS_SESSION_BUS_ADDRESS=autolaunch:")
	cfg := defaults(Config{Environment: env, Command: func(string, ...string) *exec.Cmd { t.Fatal("off policy started a process"); return nil }})
	state, err := newServices(&cfg, &reporter{output: io.Discard})
	if err != nil {
		t.Fatal(err)
	}
	if state.snapshot.ActiveBusPolicy != "off" || state.bus != nil || environmentMap(cfg.Environment)["DBUS_SESSION_BUS_ADDRESS"] != "autolaunch:" {
		t.Fatalf("%#v", state.snapshot)
	}
	if err := state.snapshot.Validate(); err != nil {
		t.Fatal(err)
	}
}

func TestBorrowedBusAndDisabledStartNeverCreateDaemon(t *testing.T) {
	for _, disabled := range []bool{false, true} {
		policy := "reuse"
		mode := "helper"
		address := "unix:path=/borrowed"
		if disabled {
			policy = "start"
			mode = "disabled"
			address = ""
		}
		env := append(serviceEnvironment(t, sessionservice.Config{Bus: policy, Notifications: "external"}), "DBUS_SESSION_BUS_ADDRESS="+address)
		calls := 0
		cfg := defaults(Config{Environment: env, BusHelperPath: "/fixture/helper", Command: func(path string, args ...string) *exec.Cmd {
			calls++
			if path != "/fixture/helper" {
				t.Fatal("borrowed/disabled mode created daemon")
			}
			return serviceCommand(mode, args...)
		}})
		state, err := newServices(&cfg, &reporter{output: io.Discard})
		if err != nil {
			t.Fatal(err)
		}
		if state.bus != nil || calls != 1 {
			t.Fatalf("unexpected ownership or calls: %d %#v", calls, state.snapshot)
		}
		if disabled {
			if state.snapshot.Items[2].State != "unavailable" {
				t.Fatal(state.snapshot)
			}
		} else if state.snapshot.Items[2].Ownership != "borrowed" || state.snapshot.Items[2].State != "ready" {
			t.Fatal(state.snapshot)
		}
	}
}

func TestOwnedBusAddressPrecedesApplicationsAndStopsOnlyOwnedChild(t *testing.T) {
	env := serviceEnvironment(t, sessionservice.Config{Bus: "start", Notifications: "external"})
	directory := environmentMap(env)["HOME"]
	daemon := filepath.Join(directory, "dbus-daemon")
	if err := os.WriteFile(daemon, []byte("injected test command"), 0700); err != nil {
		t.Fatal(err)
	}
	marker := filepath.Join(directory, "stopped")
	env = append(env, "PATH="+directory, "DBUS_SESSION_BUS_ADDRESS=", "FVWM_SESSION_TEST_DAEMON_STOP="+marker)
	cfg := defaults(Config{Environment: env, BusHelperPath: "/fixture/helper", Command: func(path string, args ...string) *exec.Cmd {
		if path == daemon {
			return serviceCommand("daemon", args...)
		}
		if path != "/fixture/helper" {
			t.Fatal(path)
		}
		return serviceCommand("helper", args...)
	}})
	state, err := newServices(&cfg, &reporter{output: io.Discard})
	if err != nil {
		t.Fatal(err)
	}
	if state.bus == nil {
		t.Fatalf("missing owned bus: %#v", state.snapshot)
	}
	defer state.bus.stop()
	if state.snapshot.Items[2].Ownership != "owned" || state.snapshot.Items[2].State != "ready" || environmentMap(cfg.Environment)["DBUS_SESSION_BUS_ADDRESS"] != "unix:path=/fixture/owned-bus" {
		t.Fatal(state.snapshot)
	}
	state.bus.stop()
	if !state.bus.reaped {
		t.Fatal("owned child was not reaped")
	}
	if data, err := os.ReadFile(marker); err != nil || string(data) != "stopped" {
		t.Fatalf("%q %v", data, err)
	}
	state.bus.stop() // Repeated cleanup cannot signal a reaped numeric PID.
}

func TestHelperDeadlineOutputAndUnsafeAddresses(t *testing.T) {
	for _, mode := range []string{"hang", "overflow"} {
		cfg := defaults(Config{BusHelperPath: "/fixture/helper", Command: func(string, ...string) *exec.Cmd { return serviceCommand(mode) }})
		ctx, cancel := context.WithTimeout(context.Background(), 80*time.Millisecond)
		if mode == "overflow" {
			cancel()
			ctx, cancel = context.WithTimeout(context.Background(), 2*time.Second)
		}
		_, err := serviceHelper(ctx, cfg, append(isolatedEnvironment(), "DBUS_SESSION_BUS_ADDRESS=unix:path=/fixture"), "probe")
		cancel()
		if err == nil {
			t.Fatal("unbounded helper admitted")
		}
		select {
		case cfg.serviceSlot <- struct{}{}:
			<-cfg.serviceSlot
		case <-time.After(time.Second):
			t.Fatal("helper did not reap/release admission")
		}
	}
	for _, address := range []string{"autolaunch:", "unix:path=/a;autolaunch:", "tcp:host=example"} {
		cfg := defaults(Config{BusHelperPath: "/fixture/helper", Command: func(string, ...string) *exec.Cmd { t.Fatal("unsafe bus address executed adapter"); return nil }})
		if _, err := serviceHelper(context.Background(), cfg, []string{"DBUS_SESSION_BUS_ADDRESS=" + address}, "probe"); err == nil {
			t.Fatal(address)
		}
	}
}

func TestServiceControlsCASDNDAndOwnedNotificationIDs(t *testing.T) {
	cfg := defaults(Config{Environment: serviceEnvironment(t, sessionservice.Config{Bus: "off", Notifications: "external"})})
	state, err := newServices(&cfg, &reporter{output: io.Discard})
	if err != nil {
		t.Fatal(err)
	}
	request := func(verb string) controlRequest {
		return controlRequest{ctx: context.Background(), message: sessionservice.RequestMessage{Version: 1, Verb: verb, Generation: state.snapshot.Generation, Operation: state.snapshot.Operation}, reply: make(chan sessionservice.Reply, 1)}
	}
	configure := request("configure")
	configure.message.Config = &sessionservice.Config{Bus: "start", Notifications: "external", DND: true}
	if _, reply := state.admit(configure, cfg, true); reply.Outcome != "busy" || state.snapshot.Operation != configure.message.Operation {
		t.Fatal("busy request consumed control identity")
	}
	start, reply := state.admit(configure, cfg, false)
	if start || reply.Outcome != "applied" || state.snapshot.ActiveBusPolicy != "off" {
		t.Fatalf("%#v", reply)
	}
	if _, reply := state.admit(configure, cfg, false); reply.Outcome != "stale" {
		t.Fatal(reply)
	}
	notify := request("notify")
	notify.message.Summary = "Quiet"
	if start, reply := state.admit(notify, cfg, false); start || reply.Outcome != "suppressed" {
		t.Fatal(reply)
	}
	closeRequest := request("close")
	closeRequest.message.NotificationID = 7
	if _, reply := state.admit(closeRequest, cfg, false); reply.Outcome != "rejected" {
		t.Fatal(reply)
	}
	state.adoptProbe(busReply{BusID: "original", ServerOwner: ":1.2"}, "borrowed")
	state.ids[7] = true
	before := state.snapshot.Items[3].Operation
	state.adoptProbe(busReply{BusID: "original", ServerOwner: ":1.3"}, "borrowed")
	if len(state.ids) != 0 || state.snapshot.Items[3].Operation == before {
		t.Fatal("replacement server inherited owned IDs")
	}
	job := serviceJob{request: request("reprobe"), probe: busReply{BusID: "replacement", ServerOwner: ":1.4"}, err: errors.New("changed bus identity"), outcome: "failed"}
	state.complete(job)
	if state.identity.BusID != "original" || state.snapshot.Items[2].State != "failed" {
		t.Fatal("replacement bus adopted")
	}
}

func TestOutboundNotificationUsesPinnedOwnerAndFiniteIDs(t *testing.T) {
	var lastArgs []string
	cfg := defaults(Config{BusHelperPath: "/fixture/helper", Environment: append(isolatedEnvironment(), "DBUS_SESSION_BUS_ADDRESS=unix:path=/fixture"), Command: func(path string, args ...string) *exec.Cmd {
		lastArgs = append([]string(nil), args...)
		return serviceCommand("helper", args...)
	}})
	identity := busReply{BusID: "fixture-bus", ServerOwner: ":1.27"}
	request := controlRequest{ctx: context.Background(), message: sessionservice.RequestMessage{Version: 1, Verb: "notify", Summary: "Plain ; $(text)", Body: "Body <text>"}}
	job := runServiceJob(request, cfg, identity, 0)
	if job.err != nil || job.outcome != "sent" || job.result.NotificationID != 7 || len(lastArgs) != 5 || lastArgs[1] != request.message.Summary || lastArgs[2] != request.message.Body {
		t.Fatalf("%#v args=%q", job, lastArgs)
	}
	request.message = sessionservice.RequestMessage{Version: 1, Verb: "close", NotificationID: 7}
	job = runServiceJob(request, cfg, identity, 1)
	if job.err != nil || job.outcome != "closed" || len(lastArgs) != 4 {
		t.Fatalf("%#v %q", job, lastArgs)
	}
	request.message = sessionservice.RequestMessage{Version: 1, Verb: "notify", Summary: "Capacity"}
	job = runServiceJob(request, cfg, identity, 32)
	if job.err == nil || len(lastArgs) != 1 || lastArgs[0] != "probe" {
		t.Fatal("capacity overflow sent a notification")
	}
	identity.BusID = "previous-bus"
	job = runServiceJob(request, cfg, identity, 0)
	if job.err == nil || len(lastArgs) != 1 || lastArgs[0] != "probe" {
		t.Fatal("bus replacement was treated as inherited readiness")
	}
}

func TestHelperDoesNotStartAfterCancellation(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	cfg := defaults(Config{BusHelperPath: "/fixture/helper", Command: func(string, ...string) *exec.Cmd { t.Fatal("cancelled context constructed a helper"); return nil }})
	if _, err := serviceHelper(ctx, cfg, []string{"DBUS_SESSION_BUS_ADDRESS=unix:path=/fixture"}, "probe"); !errors.Is(err, context.Canceled) {
		t.Fatal(err)
	}
	ctx, cancel = context.WithCancel(context.Background())
	defer cancel()
	var mutation *exec.Cmd
	cfg = defaults(Config{BusHelperPath: "/fixture/helper", Environment: append(isolatedEnvironment(), "DBUS_SESSION_BUS_ADDRESS=unix:path=/fixture"), Command: func(_ string, args ...string) *exec.Cmd {
		command := serviceCommand("helper", args...)
		if args[0] == "notify" {
			mutation = command
			cancel()
		}
		return command
	}})
	job := runServiceJob(controlRequest{ctx: ctx, message: sessionservice.RequestMessage{Verb: "notify", Summary: "Test"}}, cfg, busReply{}, 0)
	if !errors.Is(job.err, context.Canceled) || mutation == nil || mutation.Process != nil {
		t.Fatalf("cancelled action started: job=%#v command=%#v", job, mutation)
	}
}

func TestOwnerFinishCancelsAndReapsActiveHelper(t *testing.T) {
	reader, writer, err := os.Pipe()
	if err != nil {
		t.Fatal(err)
	}
	defer reader.Close()
	defer writer.Close()
	commands := make(chan *exec.Cmd, 1)
	cfg := defaults(Config{BusHelperPath: "/fixture/helper", Environment: append(isolatedEnvironment(), "DBUS_SESSION_BUS_ADDRESS=unix:path=/fixture"), Command: func(_ string, args ...string) *exec.Cmd {
		command := serviceCommand("hang-ready", args...)
		command.ExtraFiles = []*os.File{writer}
		commands <- command
		return command
	}})
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	jobs := make(chan serviceJob, 1)
	go func() {
		jobs <- runServiceJob(controlRequest{ctx: ctx, message: sessionservice.RequestMessage{Verb: "reprobe"}}, cfg, busReply{}, 0)
	}()
	command := <-commands
	reader.SetReadDeadline(time.Now().Add(2 * time.Second))
	var ready [1]byte
	if _, err := io.ReadFull(reader, ready[:]); err != nil || ready[0] != 'R' {
		t.Fatalf("helper never started: %q %v", ready, err)
	}
	finishServiceWork(cfg, cancel, jobs, &reporter{output: io.Discard})
	if command.ProcessState == nil || command.ProcessState.Success() {
		t.Fatal("owner returned before its active helper was killed/reaped")
	}
}

func TestLateProbeCannotReviveKnownOwnedBusExit(t *testing.T) {
	cfg := defaults(Config{Environment: serviceEnvironment(t, sessionservice.Defaults())})
	state, err := newServices(&cfg, &reporter{output: io.Discard})
	if err != nil {
		t.Fatal(err)
	}
	probe := busReply{BusID: "original", ServerOwner: ":1.3"}
	state.adoptProbe(probe, "owned")
	state.bus = &ownedBus{reaped: true}
	state.snapshot.Items[2].State = "failed"
	state.snapshot.Items[2].Detail = "known owned bus exit"
	state.snapshot.Items[3].State = "unavailable"
	reply := state.complete(serviceJob{probe: probe, result: busReply{NotificationID: 7}, request: controlRequest{message: sessionservice.RequestMessage{Verb: "notify"}}, outcome: "sent"})
	if reply.Error == "" || state.snapshot.Items[2].State != "failed" || state.snapshot.Items[2].Detail != "known owned bus exit" || state.snapshot.Items[3].State != "unavailable" || len(state.ids) != 0 {
		t.Fatalf("late probe revived terminal bus: %#v", reply)
	}
}

func TestKnownOwnedBusExitRefusesNewAdapterAdmission(t *testing.T) {
	cfg := defaults(Config{Environment: serviceEnvironment(t, sessionservice.Config{Bus: "off", Notifications: "external"})})
	state, err := newServices(&cfg, &reporter{output: io.Discard})
	if err != nil {
		t.Fatal(err)
	}
	probe := busReply{BusID: "original", ServerOwner: ":1.3"}
	state.adoptProbe(probe, "owned")
	state.bus = &ownedBus{reaped: true}
	state.ids[7] = true
	state.snapshot.Items[2].State = "failed"
	state.snapshot.Items[2].Detail = "known owned bus exit"
	state.snapshot.Items[3].State = "unavailable"
	request := func(verb string) controlRequest {
		return controlRequest{ctx: context.Background(), message: sessionservice.RequestMessage{Version: 1, Verb: verb, Generation: state.snapshot.Generation, Operation: state.snapshot.Operation}}
	}
	for _, verb := range []string{"reprobe", "notify", "close"} {
		call := request(verb)
		if verb == "notify" {
			call.message.Summary = "Test"
		}
		if verb == "close" {
			call.message.NotificationID = 7
		}
		operation := state.snapshot.Operation
		busAttempts, notificationAttempts := state.snapshot.Items[2].Attempts, state.snapshot.Items[3].Attempts
		start, reply := state.admit(call, cfg, false)
		if start || reply.Outcome != "unavailable" || reply.Error == "" || state.snapshot.Operation != operation || state.snapshot.Items[2].Attempts != busAttempts || state.snapshot.Items[3].Attempts != notificationAttempts {
			t.Fatalf("terminal bus admitted %s: %#v", verb, reply)
		}
		state.complete(serviceJob{probe: probe, result: busReply{NotificationID: 9}, request: call, outcome: "sent"})
		if state.snapshot.Items[2].State != "failed" || state.snapshot.Items[2].Detail != "known owned bus exit" || state.snapshot.Items[3].State != "unavailable" || state.ids[9] {
			t.Fatalf("admit/complete changed terminal status: %#v", state.snapshot)
		}
	}
	if start, reply := state.admit(request("status"), cfg, false); start || reply.Error != "" || reply.Status.Items[2].State != "failed" {
		t.Fatal("terminal status became unavailable to readers")
	}
	configure := request("configure")
	configure.message.Config = &sessionservice.Config{Bus: "start", Notifications: "external", DND: true}
	if start, reply := state.admit(configure, cfg, false); start || reply.Outcome != "applied" || state.snapshot.Items[2].State != "failed" || state.snapshot.ActiveBusPolicy != "off" {
		t.Fatalf("next-login configuration changed terminal bus: %#v", reply)
	}
	notify := request("notify")
	notify.message.Summary = "Quiet"
	if start, reply := state.admit(notify, cfg, false); start || reply.Outcome != "suppressed" || state.snapshot.Items[2].State != "failed" {
		t.Fatalf("DND was not quiet: %#v", reply)
	}
}

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
	"reflect"
	"runtime"
	"strings"
	"syscall"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
)

func TestProviderProcess(t *testing.T) {
	for i, arg := range os.Args {
		if arg != "--provider-fixture" {
			continue
		}
		switch os.Args[i+1] {
		case "wait":
			signals := make(chan os.Signal, 1)
			signal.Notify(signals, syscall.SIGTERM)
			if os.Getenv("FVWM_PROVIDER_READY") == "1" {
				file := os.NewFile(3, "ready")
				writeTestCoreReady(file)
				file.Close()
			}
			<-signals
		case "wm":
			signals := make(chan os.Signal, 1)
			signal.Notify(signals, syscall.SIGTERM)
			if os.Getenv("FVWM_PROVIDER_RESTARTED") == "1" {
				if os.Getenv(ReadyEnvironment) != "" {
					os.Exit(95)
				}
				os.WriteFile(os.Getenv("FVWM_PROVIDER_ENV")+"-restart", []byte("restarted"), 0600)
			} else {
				jsonData, _ := json.Marshal(os.Environ())
				if os.WriteFile(os.Getenv("FVWM_PROVIDER_ENV"), jsonData, 0600) != nil {
					os.Exit(96)
				}
				file := os.NewFile(3, "ready")
				writeTestCoreReady(file)
				file.Close()
				os.Unsetenv(ReadyEnvironment)
				syscall.Exec(os.Args[0], os.Args, append(os.Environ(), "FVWM_PROVIDER_RESTARTED=1"))
				os.Exit(97)
			}
			<-signals
		case "ok":
		case "fail":
			os.Exit(8)
		case "discovery":
			json.NewEncoder(os.Stdout).Encode(providerPlans{Poweroff: []string{"/fixture/native", "poweroff"}, Reboot: []string{"/fixture/native", "reboot"}, Locker: []string{"/fixture/locker"}})
		case "login":
			state := "ready"
			args := os.Args[i+2:]
			if len(args) > 0 && args[0] == "power-request" {
				if len(args) != 4 || args[1] != "poweroff" || args[2] != ":1.72" || args[3] != "fixture-system" {
					os.Exit(94)
				}
				state = "requested"
			}
			json.NewEncoder(os.Stdout).Encode(powerHelperReply{Version: 1, Kind: "power", Available: true, State: state, BusID: "fixture-system", ServerOwner: ":1.72", Capabilities: []sessionservice.PowerCapability{{Action: "poweroff", Permission: "yes"}, {Action: "reboot", Permission: "challenge"}, {Action: "suspend", Permission: "inhibitor-blocked"}}})
		default:
			os.Exit(93)
		}
		os.Exit(0)
	}
}
func providerCommand(mode string, args ...string) *exec.Cmd {
	return exec.Command(os.Args[0], append([]string{"-test.run=^TestProviderProcess$", "--", "--provider-fixture", mode}, args...)...)
}
func providerState(t *testing.T) *serviceState {
	t.Helper()
	cfg := defaults(Config{Environment: serviceEnvironment(t, sessionservice.Defaults())})
	state, err := newServices(&cfg, &reporter{output: io.Discard})
	if err != nil {
		t.Fatal(err)
	}
	state.snapshot.Power.State = "ready"
	state.snapshot.Power.Capabilities[0].Permission = "unknown"
	state.snapshot.Power.Capabilities[1].Permission = "unknown"
	state.plans = providerPlans{Poweroff: []string{"/fixture/native", "poweroff"}, Reboot: []string{"/fixture/native", "reboot"}, Locker: []string{"/fixture/locker"}}
	state.snapshot.Locker.State = "installed"
	return state
}
func providerRequest(state *serviceState, verb string) controlRequest {
	message := sessionservice.RequestMessage{Version: sessionservice.Version, Verb: verb, Generation: state.snapshot.Generation, Operation: state.snapshot.Operation}
	if sessionservice.IsPowerVerb(verb) {
		message.ProviderGeneration = state.snapshot.Power.Generation
	}
	return controlRequest{ctx: context.Background(), message: message, reply: make(chan sessionservice.Reply, 1)}
}

func TestPowerStaleNoReplayAndSecureSuspendRefusal(t *testing.T) {
	state := providerState(t)
	request := providerRequest(state, "poweroff")
	request.message.ProviderGeneration = strings.Repeat("f", 32)
	before := state.snapshot.Operation
	if start, reply := state.admit(request, Config{}, false); start || reply.Outcome != "stale" || state.snapshot.Operation != before {
		t.Fatal(reply)
	}
	request = providerRequest(state, "suspend")
	if start, reply := state.admit(request, Config{}, false); start || reply.Outcome != "unavailable" || state.snapshot.Operation != before {
		t.Fatal(reply)
	}
	request = providerRequest(state, "poweroff")
	if start, reply := state.admit(request, Config{}, false); !start {
		t.Fatal(reply)
	}
	calls := 0
	cfg := defaults(Config{Command: func(string, ...string) *exec.Cmd { calls++; return providerCommand("fail") }})
	job := runProviderJob(request, cfg, state.snapshot.Power, state.plans)
	if job.outcome != "outcome-unconfirmed" || calls != 1 {
		t.Fatalf("%#v calls=%d", job, calls)
	}
	state.complete(job)
	if start, reply := state.admit(request, cfg, false); start || reply.Outcome != "stale" || calls != 1 {
		t.Fatal(reply)
	}
	if start, reply := state.admit(providerRequest(state, "poweroff"), cfg, false); start || reply.Outcome != "unavailable" {
		t.Fatal("uncertain action admitted without a new probe", reply)
	}
}

func TestProviderFailureAndOwnedSessionBusDeathAreIndependent(t *testing.T) {
	state := providerState(t)
	state.ids[7] = true
	state.bus = &ownedBus{reaped: true}
	state.snapshot.Items[2].State = "failed"
	request := providerRequest(state, "poweroff")
	if start, reply := state.admit(request, Config{}, false); !start {
		t.Fatal(reply)
	}
	cfg := defaults(Config{Command: func(string, ...string) *exec.Cmd { return providerCommand("ok") }})
	reply := state.complete(runProviderJob(request, cfg, state.snapshot.Power, state.plans))
	if reply.Outcome != "requested" || !state.ids[7] || state.snapshot.Items[2].State != "failed" {
		t.Fatal(reply)
	}
	power := state.snapshot.Power
	state.complete(serviceJob{request: providerRequest(state, "notify"), err: errors.New("notification failure"), outcome: "failed"})
	if state.snapshot.Power.State != power.State || state.snapshot.Power.Generation != power.Generation {
		t.Fatal("notification failure changed power state")
	}
}

func TestProviderCancellationBeforeAndAfterStart(t *testing.T) {
	state := providerState(t)
	request := providerRequest(state, "poweroff")
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	request.ctx = ctx
	calls := 0
	cfg := defaults(Config{Command: func(string, ...string) *exec.Cmd { calls++; return providerCommand("ok") }})
	job := runProviderJob(request, cfg, state.snapshot.Power, state.plans)
	if !errors.Is(job.err, context.Canceled) || calls != 0 {
		t.Fatalf("%#v calls=%d", job, calls)
	}
	reader, writer, err := os.Pipe()
	if err != nil {
		t.Fatal(err)
	}
	defer reader.Close()
	defer writer.Close()
	ctx, cancel = context.WithCancel(context.Background())
	defer cancel()
	request.ctx = ctx
	cfg = defaults(Config{Environment: []string{"FVWM_PROVIDER_READY=1"}, Command: func(string, ...string) *exec.Cmd {
		calls++
		command := providerCommand("wait")
		command.ExtraFiles = []*os.File{writer}
		return command
	}})
	done := make(chan serviceJob, 1)
	go func() { done <- runProviderJob(request, cfg, state.snapshot.Power, state.plans) }()
	reader.SetReadDeadline(time.Now().Add(2 * time.Second))
	var ready [1]byte
	if _, err := io.ReadFull(reader, ready[:]); err != nil || ready[0] != 'R' {
		t.Fatalf("%q %v", ready, err)
	}
	cancel()
	job = <-done
	if job.outcome != "outcome-unconfirmed" || calls != 1 {
		t.Fatalf("%#v calls=%d", job, calls)
	}
	select {
	case cfg.serviceSlot <- struct{}{}:
		<-cfg.serviceSlot
	case <-time.After(time.Second):
		t.Fatal("actual Wait did not release helper lease")
	}
}

func TestLockerRetainedAfterRequesterCancellationAndDuplicateRefused(t *testing.T) {
	state := providerState(t)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	request := providerRequest(state, "lock")
	request.ctx = ctx
	if start, reply := state.admit(request, Config{}, false); !start {
		t.Fatal(reply)
	}
	if start, reply := state.admit(providerRequest(state, "lock"), Config{}, false); start || reply.Outcome != "busy" {
		t.Fatal("pending duplicate admitted", reply)
	}
	cfg := defaults(Config{Command: func(string, ...string) *exec.Cmd { return providerCommand("wait") }})
	job := runProviderJob(request, cfg, state.snapshot.Power, state.plans)
	if job.locker == nil {
		t.Fatal(job.err)
	}
	defer job.locker.command.Process.Kill()
	cancel()
	state.complete(job)
	if state.snapshot.Locker.State != "running" || state.snapshot.Locker.SecureReady {
		t.Fatal(state.snapshot.Locker)
	}
	if err := state.locker.command.Process.Signal(syscall.Signal(0)); err != nil {
		t.Fatal("request cancellation killed locker", err)
	}
	if start, reply := state.admit(providerRequest(state, "lock"), cfg, false); start || reply.Outcome != "busy" {
		t.Fatal("running duplicate admitted", reply)
	}
	select {
	case cfg.serviceSlot <- struct{}{}:
		<-cfg.serviceSlot
	default:
		t.Fatal("long-lived locker holds transient helper lease")
	}
	state.locker.command.Process.Kill()
	select {
	case <-state.locker.done:
	case <-time.After(time.Second):
		t.Fatal("locker missing actual Wait")
	}
	select {
	case <-state.locker.done:
		t.Fatal("duplicate Wait event")
	default:
	}
}

func TestLoginPowerUsesPinnedSystemProviderWithoutSessionBus(t *testing.T) {
	state := providerState(t)
	observation := state.snapshot.Power
	observation.Provider = "login1"
	cfg := defaults(Config{BusHelperPath: "/fixture/power", Environment: []string{"DBUS_SESSION_BUS_ADDRESS=autolaunch:"}, Command: func(_ string, args ...string) *exec.Cmd { return providerCommand("login", args...) }})
	power, _, err := loginPower(context.Background(), cfg, observation, "")
	if err != nil || power.Capabilities[1].Permission != "challenge" || power.Capabilities[2].Permission != "inhibitor-blocked" {
		t.Fatalf("%#v %v", power, err)
	}
	power, _, err = loginPower(context.Background(), cfg, power, "poweroff")
	if err != nil || power.State != "requested" {
		t.Fatalf("%#v %v", power, err)
	}
}

func TestDiscoveryDoesNotExecuteAndUsesAbsoluteRoots(t *testing.T) {
	directory := t.TempDir()
	for _, name := range []string{"systemctl", "shutdown", "xlock", "slock"} {
		if err := os.WriteFile(filepath.Join(directory, name), []byte("not an executable image"), 0700); err != nil {
			t.Fatal(err)
		}
	}
	var data strings.Builder
	if err := ProviderDiscovery([]string{"PATH=.:relative:" + directory}, &data); err != nil {
		t.Fatal(err)
	}
	var plans providerPlans
	if err := decodeProvider([]byte(data.String()), &plans); err != nil {
		t.Fatal(err)
	}
	if len(plans.Locker) != 1 || plans.Locker[0] != filepath.Join(directory, "xlock") {
		t.Fatal(plans)
	}
	if runtime.GOOS == "linux" && (len(plans.Poweroff) != 2 || plans.Poweroff[0] != filepath.Join(directory, "systemctl")) {
		t.Fatal(plans)
	}
	if (runtime.GOOS == "freebsd" || runtime.GOOS == "openbsd") && (len(plans.Poweroff) != 3 || plans.Poweroff[1] != "-p") {
		t.Fatal(plans)
	}
}

func TestParentRetainsLockerAcrossCoreExecAndExit(t *testing.T) {
	environment := serviceEnvironment(t, sessionservice.Defaults())
	marker := filepath.Join(environmentMap(environment)["HOME"], "environment.json")
	environment = append(environment, "FVWM_PROVIDER_ENV="+marker)
	signals := make(chan os.Signal, 1)
	lockerCommands := make(chan *exec.Cmd, 1)
	cfg := Config{WMPath: "/fixture/wm", SelfPath: "/fixture/self", Environment: environment, NoAutostart: true, Signals: signals, Command: func(path string, args ...string) *exec.Cmd {
		switch path {
		case "/fixture/wm":
			return providerCommand("wm")
		case "/fixture/self":
			return providerCommand("discovery")
		case "/fixture/locker":
			command := providerCommand("wait")
			lockerCommands <- command
			return command
		default:
			t.Errorf("unexpected child %s", path)
			return providerCommand("fail")
		}
	}}
	done := make(chan int, 1)
	go func() { done <- Run(cfg) }()
	finished := false
	defer func() {
		if !finished {
			select {
			case signals <- syscall.SIGTERM:
			default:
			}
			select {
			case <-done:
			case <-time.After(5 * time.Second):
				t.Error("parent fixture cleanup deadline")
			}
		}
	}()
	deadline := time.Now().Add(3 * time.Second)
	var inherited []string
	for time.Now().Before(deadline) {
		data, err := os.ReadFile(marker)
		if err == nil && json.Unmarshal(data, &inherited) == nil {
			break
		}
		time.Sleep(5 * time.Millisecond)
	}
	env := environmentMap(inherited)
	query := func(message sessionservice.RequestMessage) (sessionservice.Reply, error) {
		ctx, cancel := context.WithTimeout(context.Background(), time.Second)
		defer cancel()
		return sessionservice.RequestTo(ctx, env[sessionservice.SocketEnvironment], env[sessionservice.CapabilityEnvironment], message)
	}
	var status sessionservice.Reply
	for time.Now().Before(deadline) {
		var err error
		status, err = query(sessionservice.RequestMessage{Version: sessionservice.Version, Verb: "status"})
		if _, restartErr := os.Stat(marker + "-restart"); err == nil && restartErr == nil && status.Status.Locker.State == "installed" {
			break
		}
		time.Sleep(5 * time.Millisecond)
	}
	if status.Status.Locker.State != "installed" {
		t.Fatalf("initial async discovery failed: %#v", status)
	}
	reply, err := query(sessionservice.RequestMessage{Version: sessionservice.Version, Verb: "lock", Generation: status.Generation, Operation: status.Operation})
	if err != nil || reply.Status.Locker.State != "running" || reply.Status.Locker.SecureReady {
		t.Fatalf("%#v %v", reply, err)
	}
	command := <-lockerCommands
	defer command.Process.Kill()
	signals <- syscall.SIGTERM
	select {
	case code := <-done:
		finished = true
		if code != 0 {
			t.Fatalf("WM exit %d", code)
		}
	case <-time.After(3 * time.Second):
		t.Fatal("parent failed to exit independently of locker")
	}
	if err := command.Process.Signal(syscall.Signal(0)); err != nil {
		t.Fatal("WM exit killed locker", err)
	}
	command.Process.Kill()
	// The owner waiter, not this test, performs the sole Wait.
}

func TestLockerCancelledBeforeStartConstructsNoChild(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	cfg := defaults(Config{Command: func(string, ...string) *exec.Cmd { t.Fatal("cancelled locker constructed child"); return nil }})
	locker, err := startLocker(ctx, cfg, []string{"/fixture/locker"})
	if locker != nil || !errors.Is(err, context.Canceled) {
		t.Fatalf("%#v %v", locker, err)
	}
	ctx, cancel = context.WithCancel(context.Background())
	defer cancel()
	var command *exec.Cmd
	cfg = defaults(Config{Command: func(string, ...string) *exec.Cmd { command = providerCommand("wait"); cancel(); return command }})
	locker, err = startLocker(ctx, cfg, []string{"/fixture/locker"})
	if locker != nil || !errors.Is(err, context.Canceled) || command.Process != nil {
		t.Fatalf("%#v %v %#v", locker, err, command)
	}
}

func TestPowerPolicyChangeInvalidatesPresentedProvider(t *testing.T) {
	state := providerState(t)
	oldGeneration := state.snapshot.Power.Generation
	request := providerRequest(state, "configure")
	config := state.snapshot.Config
	config.Power = "login1"
	request.message.Config = &config
	if start, reply := state.admit(request, Config{}, false); start || reply.Outcome != "applied" {
		t.Fatal(reply)
	}
	if state.snapshot.Power.Provider != "login1" || state.snapshot.Power.Generation == oldGeneration || state.snapshot.Power.State != "unavailable" {
		t.Fatal(state.snapshot.Power)
	}
	request = providerRequest(state, "poweroff")
	request.message.ProviderGeneration = oldGeneration
	if start, reply := state.admit(request, Config{}, false); start || reply.Outcome != "stale" {
		t.Fatal(reply)
	}
}

func TestPendingPowerProbeCannotAuthorizeReplacementProvider(t *testing.T) {
	state := providerState(t)
	state.snapshot.Config.Power = "login1"
	state.snapshot.Power.Provider = "login1"
	state.snapshot.Power.BusID = "old-system"
	state.snapshot.Power.ServerOwner = ":1.10"
	state.snapshot.Power.Capabilities[0].Permission = "yes"
	state.snapshot.Power.Capabilities[1].Permission = "yes"
	state.locker = &ownedLocker{}
	state.snapshot.Locker.State = "running"
	state.ids[7] = true
	items := append([]sessionservice.Item(nil), state.snapshot.Items...)
	locker := state.snapshot.Locker
	probe := providerRequest(state, "power-probe")
	if start, reply := state.admit(probe, Config{}, false); !start {
		t.Fatal(reply)
	}
	_, pending := state.admit(providerRequest(state, "status"), Config{}, true)
	if pending.Error != "" || pending.Status.Power.State != "unavailable" || pending.Status.Power.BusID != "" || pending.Status.Power.ServerOwner != "" {
		t.Fatal(pending)
	}
	for _, capability := range pending.Status.Power.Capabilities {
		if capability.Permission != "na" {
			t.Fatal("pending probe retained request capability", pending)
		}
	}
	confirmation := providerRequest(state, "poweroff")
	confirmation.message.Generation = pending.Generation
	confirmation.message.Operation = pending.Operation
	confirmation.message.ProviderGeneration = pending.Status.Power.Generation
	if start, reply := state.admit(confirmation, Config{}, true); start || reply.Outcome != "unavailable" {
		t.Fatal("pending provider admitted a power request", reply)
	}
	replacement := unavailablePower("login1", pending.Status.Power.Generation, "replacement provider")
	replacement.State = "ready"
	replacement.BusID = "new-system"
	replacement.ServerOwner = ":1.20"
	replacement.Capabilities[0].Permission = "no"
	replacement.Capabilities[1].Permission = "yes"
	final := state.complete(serviceJob{request: probe, power: replacement, plans: state.plans, outcome: "ready"})
	if final.Error != "" || final.Operation == pending.Operation || final.Status.Power.Generation == pending.Status.Power.Generation {
		t.Fatal("probe result retained intermediate tokens", final)
	}
	starts := 0
	dispatch := func(request controlRequest) sessionservice.Reply {
		start, reply := state.admit(request, Config{}, false)
		if start {
			starts++
			t.Error("stale confirmation reached child dispatch")
		}
		return reply
	}
	if reply := dispatch(confirmation); reply.Outcome != "stale" || starts != 0 {
		t.Fatal(reply)
	}
	confirmation.message.Operation = final.Operation
	if reply := dispatch(confirmation); reply.Outcome != "stale" || starts != 0 {
		t.Fatal("pending provider generation was accepted with current parent CAS", reply)
	}
	if start, reply := state.admit(providerRequest(state, "poweroff"), Config{}, false); start || reply.Outcome != "denied" {
		t.Fatal("old permission survived replacement", reply)
	}
	if start, reply := state.admit(providerRequest(state, "reboot"), Config{}, false); !start {
		t.Fatal("fresh replacement capability refused", reply)
	}
	if state.snapshot.Power.BusID != "new-system" || state.snapshot.Power.ServerOwner != ":1.20" || !reflect.DeepEqual(items, state.snapshot.Items) || state.snapshot.Locker != locker || !state.ids[7] {
		t.Fatal("probe changed independent observations", state.snapshot)
	}
}

func TestPowerProbeIdentityFailureRefusesResult(t *testing.T) {
	for _, failAt := range []int{1, 2} {
		t.Run(string(rune('0'+failAt)), func(t *testing.T) {
			state := providerState(t)
			state.ids[7] = true
			items := append([]sessionservice.Item(nil), state.snapshot.Items...)
			locker := state.snapshot.Locker
			probe := providerRequest(state, "power-probe")
			if start, reply := state.admit(probe, Config{}, false); !start {
				t.Fatal(reply)
			}
			pending := providerRequest(state, "poweroff")
			replacement := state.snapshot.Power
			replacement.State = "ready"
			replacement.Capabilities = append([]sessionservice.PowerCapability(nil), replacement.Capabilities...)
			replacement.Capabilities[0].Permission = "unknown"
			calls := 0
			reply := state.completeProviderWithIdentity(serviceJob{request: probe, power: replacement, plans: state.plans, outcome: "ready"}, func(int) (string, error) {
				calls++
				if calls == failAt {
					return "", errors.New("fixture entropy failure")
				}
				return strings.Repeat("e", 32), nil
			})
			if reply.Outcome != "failed" || reply.Error == "" || state.snapshot.Power.State != "failed" || state.snapshot.Power.Capabilities[0].Permission != "na" {
				t.Fatal(reply)
			}
			if start, reply := state.admit(pending, Config{}, false); start || reply.Outcome != "unavailable" {
				t.Fatal("entropy failure exposed a request-capable result", reply)
			}
			if !reflect.DeepEqual(items, state.snapshot.Items) || state.snapshot.Locker != locker || !state.ids[7] {
				t.Fatal("entropy failure changed independent observations", state.snapshot)
			}
		})
	}
}

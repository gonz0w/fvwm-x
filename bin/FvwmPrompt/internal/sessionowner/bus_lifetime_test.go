// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"bytes"
	"context"
	"errors"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
	"golang.org/x/sys/unix"
)

// This is a protocol peer, not evidence of actual daemon waitpid. The native
// guardian suite exercises the actual child; these real exec/pipe tests exercise
// the parent's receipt parsing, independent Wait, ownership and failure unwind.
func TestBusGuardianChild(t *testing.T) {
	index := -1
	for i, arg := range os.Args {
		if arg == "--guardian" {
			index = i
			break
		}
	}
	if index < 0 {
		return
	}
	mode := os.Args[index+1]
	if os.Getenv(ReadyEnvironment) != "" || os.Getenv(LeaseEnvironment) != "" || os.Getenv(BusLifetimeEnvironment) != "" {
		os.Exit(91)
	}
	flags, err := unix.FcntlInt(3, unix.F_GETFL, 0)
	if err != nil || flags&unix.O_NONBLOCK == 0 {
		os.Exit(92)
	}
	life := os.NewFile(3, "lifetime")
	defer life.Close()
	address := "unix:path=/fixture/owned-bus\n"
	switch mode {
	case "lifetime", "unknown", "passive", "waited-passive", "absent-passive", "absent", "cleanup-pending":
		io.WriteString(os.Stdout, address)
		if mode == "unknown" {
			time.Sleep(20 * time.Millisecond)
			os.Exit(37)
		}
		if strings.HasSuffix(mode, "passive") {
			time.Sleep(20 * time.Millisecond)
			if mode == "passive" {
				io.WriteString(os.Stdout, "U\n")
			} else if mode == "waited-passive" {
				io.WriteString(os.Stdout, "W 7\n")
			}
			os.Stdout.Close()
		}
		io.Copy(io.Discard, life)
		if strings.HasSuffix(mode, "passive") || mode == "cleanup-pending" {
			// No extra receipt after U. Retain the passive Wait owner until
			// the test's explicit sentinel models actual child reap.
			for {
				if _, err := os.Stat(os.Getenv("FVWM_TEST_GUARDIAN_RELEASE")); err == nil {
					os.Exit(0)
				}
				time.Sleep(5 * time.Millisecond)
			}
		}
		if path := os.Getenv("FVWM_SESSION_TEST_DAEMON_STOP"); path != "" {
			os.WriteFile(path, []byte("stopped"), 0600)
		}
		if mode != "absent" {
			io.WriteString(os.Stdout, "W 0\n")
		}
	case "stderr-descendant":
		child := exec.Command(os.Args[0], "-test.run=^TestGuardianStderrDescendant$", "--", "--stderr-descendant")
		child.Env, child.Stderr = os.Environ(), os.Stderr
		unix.CloseOnExec(3)
		if err := child.Start(); err != nil {
			os.Exit(94)
		}
		io.WriteString(os.Stdout, address)
		io.Copy(io.Discard, life)
		// Ordinary failure unwind still owns the one actual child Wait. The
		// regression deliberately kills this guardian while its child lives.
		child.Wait()
	case "stall":
		io.Copy(io.Discard, life)
		io.WriteString(os.Stdout, "W 143\n")
	case "raw":
		io.WriteString(os.Stdout, os.Getenv("FVWM_TEST_GUARDIAN_OUTPUT"))
	case "address-then-raw":
		io.WriteString(os.Stdout, address)
		io.Copy(io.Discard, life)
		io.WriteString(os.Stdout, os.Getenv("FVWM_TEST_GUARDIAN_OUTPUT"))
	default:
		os.Exit(93)
	}
	os.Exit(0)
}

func guardianCommand(mode string, args ...string) *exec.Cmd {
	return exec.Command(os.Args[0], append([]string{"-test.run=^TestBusGuardianChild$", "--", "--guardian", mode}, args...)...)
}

func guardianConfig(t *testing.T, mode string) Config {
	t.Helper()
	directory := t.TempDir()
	if err := os.WriteFile(filepath.Join(directory, "dbus-daemon"), []byte("injected owned test executable"), 0700); err != nil {
		t.Fatal(err)
	}
	return defaults(Config{Environment: append(isolatedEnvironment(), "PATH="+directory), BusHelperPath: "/fixture/helper", Command: func(path string, args ...string) *exec.Cmd {
		if path != "/fixture/helper" || len(args) != 2 || args[0] != "bus-guardian" || args[1] != filepath.Join(directory, "dbus-daemon") {
			t.Fatalf("arbitrary guardian launch: %q %q", path, args)
		}
		return guardianCommand(mode, args...)
	}})
}

func endedTestBus() *ownedBus {
	bus := &ownedBus{terminal: make(chan struct{})}
	bus.end()
	return bus
}

func TestGuardianReceiptCanonicalExitAndBounds(t *testing.T) {
	for _, line := range []string{"W 0", "W 1", "W 143", "W 255", "U"} {
		if _, valid := parseDaemonReceipt(line); !valid {
			t.Fatalf("valid receipt refused: %q", line)
		}
	}
	for _, line := range []string{"W -1", "W 256", "W 01", "W +1", "W  1", "W 1 ", "W", "U ", "W 1\r", "", "W 999999999999999999999"} {
		if _, valid := parseDaemonReceipt(line); valid {
			t.Fatalf("malformed receipt accepted: %q", line)
		}
	}
	for _, length := range []int{4096, 4097} {
		line, err := readBusLine(strings.NewReader(strings.Repeat("x", length)+"\n"), 4096)
		if (err == nil) != (length == 4096) || err == nil && len(line) != length {
			t.Fatalf("line bound %d: %d %v", length, len(line), err)
		}
	}
}

func TestGuardianAddressAndDaemonReceiptAreSeparateFromProxyWait(t *testing.T) {
	cfg := guardianConfig(t, "lifetime")
	bus, address, err := startBus(context.Background(), cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer bus.stop()
	if address != "unix:path=/fixture/owned-bus" || bus.ended() || bus.observations().guardianWaitObserved {
		t.Fatalf("address was not a live startup observation: %q %#v", address, bus.observations())
	}
	flags, err := unix.FcntlInt(bus.lifetime.Fd(), unix.F_GETFL, 0)
	if err != nil || flags&unix.O_NONBLOCK == 0 {
		t.Fatalf("exec reset parent writer nonblocking: %d %v", flags, err)
	}
	if facts := bus.stop(); facts.receipt.kind != daemonReceiptWaited || facts.receipt.exit != 0 || !facts.guardianWaitObserved {
		t.Fatalf("lost independent receipt/Wait: %#v", facts)
	}
	bus.stop()
}

func TestGuardianAbsentMalformedAndExtraReceiptsNeverForgeDaemonWait(t *testing.T) {
	for _, output := range []string{"", "W 0", "W 01\n", "W 256\n", "W 0\nextra", "W 0\nW 0\n", "U\n ", "unix:path=/second\n", strings.Repeat("x", 5000)} {
		t.Run(output[:min(len(output), 20)], func(t *testing.T) {
			cfg := guardianConfig(t, "address-then-raw")
			cfg.Environment = append(cfg.Environment, "FVWM_TEST_GUARDIAN_OUTPUT="+output)
			bus, _, err := startBus(context.Background(), cfg)
			if err != nil {
				t.Fatal(err)
			}
			facts := bus.stop()
			if facts.receipt.kind != daemonReceiptInvalid || !facts.guardianWaitObserved || facts.guardianWaitError != nil {
				t.Fatalf("proxy success forged daemon Wait: %#v", facts)
			}
		})
	}
}

func TestGuardianFailedStartupUnwindsLifetimeAndRetainsActualWait(t *testing.T) {
	for _, output := range []string{"W 2\n", "U\n", "unix:path=/bad;autolaunch:\n", "garbage\n", strings.Repeat("x", 4097) + "\n"} {
		cfg := guardianConfig(t, "raw")
		cfg.Environment = append(cfg.Environment, "FVWM_TEST_GUARDIAN_OUTPUT="+output)
		bus, address, err := startBus(context.Background(), cfg)
		if err == nil || address != "" || bus == nil {
			t.Fatalf("failed startup lost owner: address=%q bus=%v err=%v", address, bus, err)
		}
		if facts := bus.observations(); !facts.guardianWaitObserved {
			t.Fatalf("actual guardian Wait not observed after failure: %#v", facts)
		}
		if _, err := bus.lifetime.Write([]byte("x")); !errors.Is(err, os.ErrClosed) {
			t.Fatalf("startup failure kept parent writer: %v", err)
		}
	}
	cfg := guardianConfig(t, "stall")
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Millisecond)
	defer cancel()
	bus, _, err := startBus(ctx, cfg)
	if err == nil || bus == nil || !bus.observations().guardianWaitObserved {
		t.Fatalf("stalled address was not unwound: bus=%v err=%v", bus, err)
	}
}

func TestGuardianUnknownExitAndUnconfirmedReceiptFenceImmediately(t *testing.T) {
	for _, mode := range []string{"unknown", "passive", "waited-passive", "absent-passive"} {
		t.Run(mode, func(t *testing.T) {
			cfg := guardianConfig(t, mode)
			release := filepath.Join(t.TempDir(), "release")
			cfg.Environment = append(cfg.Environment, "FVWM_TEST_GUARDIAN_RELEASE="+release)
			bus, _, _ := startBus(context.Background(), cfg)
			if bus == nil {
				t.Fatal("started guardian owner lost")
			}
			defer func() {
				os.WriteFile(release, nil, 0600)
				bus.stop()
			}()
			select {
			case <-bus.terminal:
			case <-time.After(time.Second):
				t.Fatal("terminal state waited for proxy Wait")
			}
			if mode == "unknown" {
				facts := bus.stop()
				if facts.receipt.kind == daemonReceiptWaited || !facts.guardianWaitObserved || exitCode(facts.guardianWaitError) != 37 {
					t.Fatalf("proxy crash forged daemon Wait: %#v", facts)
				}
				return
			}
			facts := bus.stopWithin(30 * time.Millisecond)
			want := daemonReceiptUnconfirmed
			if mode == "waited-passive" {
				want = daemonReceiptWaited
			} else if mode == "absent-passive" {
				want = daemonReceiptInvalid
			}
			if facts.receipt.kind != want || facts.guardianWaitObserved || !strings.Contains(facts.description(), "outstanding actual owner preserved") {
				t.Fatalf("U conflated with proxy Wait: %#v", facts)
			}
			if err := bus.command.Process.Signal(syscall.Signal(0)); err != nil {
				t.Fatal("cleanup killed passive owner", err)
			}
			os.WriteFile(release, nil, 0600)
			select {
			case <-bus.guardianDone:
			case <-time.After(time.Second):
				t.Fatal("passive fake owner did not finish actual Wait")
			}
		})
	}
}

func TestCancelledGuardianNeverStartsAndFailedExecClosesPipes(t *testing.T) {
	cfg := guardianConfig(t, "lifetime")
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	cfg.Command = func(string, ...string) *exec.Cmd { t.Fatal("cancelled startup constructed guardian"); return nil }
	if bus, _, err := startBus(ctx, cfg); bus != nil || !errors.Is(err, context.Canceled) {
		t.Fatalf("cancelled guardian started: %v %v", bus, err)
	}
	var attempted *exec.Cmd
	cfg.Command = func(string, ...string) *exec.Cmd {
		attempted = exec.Command(filepath.Join(t.TempDir(), "missing"))
		return attempted
	}
	if bus, _, err := startBus(context.Background(), cfg); bus != nil || err == nil {
		t.Fatalf("missing guardian started: %v %v", bus, err)
	}
	if _, err := attempted.ExtraFiles[0].Read(make([]byte, 1)); !errors.Is(err, os.ErrClosed) {
		t.Fatalf("failed exec leaked inherited lifetime reader: %v", err)
	}
	if file, ok := attempted.Stdout.(*os.File); !ok {
		t.Fatal("stdout was not an explicit file")
	} else if _, err := file.Write([]byte("x")); !errors.Is(err, os.ErrClosed) {
		t.Fatalf("failed exec leaked stdout writer: %v", err)
	}
}

func TestBusLossFencesAlreadyCompletedProbeAndPreservesProviders(t *testing.T) {
	cfg := defaults(Config{Environment: serviceEnvironment(t, sessionservice.Config{Power: "portable", Bus: "off", Notifications: "external"})})
	state, err := newServices(&cfg, &reporter{output: io.Discard})
	if err != nil {
		t.Fatal(err)
	}
	probe := busReply{BusID: "original", ServerOwner: ":1.3"}
	state.adoptProbe(probe, "owned")
	state.bus = endedTestBus()
	state.ids[7] = true
	state.snapshot.Power.State, state.snapshot.Locker.State = "ready", "running"
	state.snapshot.Power.Detail, state.snapshot.Locker.Detail = "independent power", "independent locker"
	job := serviceJob{probe: probe, result: busReply{NotificationID: 8}, request: controlRequest{message: sessionservice.RequestMessage{Verb: "notify"}}, outcome: "sent"}
	if reply := state.complete(job); reply.Outcome != "failed" || len(state.ids) != 0 || state.snapshot.Items[2].State != "failed" || state.snapshot.Items[3].State != "unavailable" {
		t.Fatalf("completion beat terminal select: %#v", reply)
	}
	if state.snapshot.Power.Detail != "independent power" || state.snapshot.Locker.Detail != "independent locker" {
		t.Fatal("bus loss changed independent provider ownership")
	}
}

func TestBusLifetimeReadinessRequiresExactCombination(t *testing.T) {
	for _, lease := range []bool{false, true} {
		for _, ack := range []string{"R", "RL", "RB", "RLB", "RBL", "", "RLB\n", "RLBB"} {
			reader, writer, err := os.Pipe()
			if err != nil {
				t.Fatal(err)
			}
			io.WriteString(writer, ack)
			writer.Close()
			err = waitReady(reader, time.Second, lease, true)
			reader.Close()
			want := "RB"
			if lease {
				want = "RLB"
			}
			if (err == nil) != (ack == want) {
				t.Fatalf("lease=%v ack=%q err=%v", lease, ack, err)
			}
		}
	}
}

func TestOwnedBusNoAutostartStillPassesFixedLifetimeSlot(t *testing.T) {
	for _, realLease := range []bool{false, true} {
		env := serviceEnvironment(t, sessionservice.Config{Power: "portable", Bus: "start", Notifications: "external"})
		directory := environmentMap(env)["HOME"]
		os.WriteFile(filepath.Join(directory, "dbus-daemon"), []byte("injected"), 0700)
		var output bytes.Buffer
		var wm *exec.Cmd
		cfg := Config{Environment: append(env, "PATH="+directory), NoAutostart: true, WMPath: "/wm", SelfPath: "/self", BusHelperPath: "/fixture/helper", Diagnostics: &output,
			Command: func(path string, args ...string) *exec.Cmd {
				if path == "/wm" {
					wm = childCommand("wm-ready")
					return wm
				}
				if path == "/self" && len(args) == 1 && args[0] == ProviderWorkerArgument {
					return providerCommand("discovery")
				}
				if path != "/fixture/helper" {
					t.Fatal("no-autostart constructed acquisition worker/app", path, args)
				}
				if args[0] == "bus-guardian" {
					return guardianCommand("lifetime")
				}
				return serviceCommand("helper", args...)
			},
		}
		if !realLease {
			cfg.Lock = func([]string) (io.Closer, error) { return io.NopCloser(strings.NewReader("")), nil }
		}
		if code := Run(cfg); code != 0 {
			t.Fatalf("status=%d logs=%s", code, output.String())
		}
		if len(wm.ExtraFiles) != 3 || wm.ExtraFiles[2] == nil || (wm.ExtraFiles[1] != nil) != realLease || !strings.Contains(output.String(), "core startup acknowledged") || !strings.Contains(output.String(), "daemon Wait confirmed status=0; guardian Wait observed status=0") {
			t.Fatalf("slots/readiness/cleanup incorrect files=%v logs=%s", wm.ExtraFiles, output.String())
		}
	}
}

func TestGuardianReadDeadlineClearedAfterAddress(t *testing.T) {
	cfg := guardianConfig(t, "lifetime")
	ctx, cancel := context.WithTimeout(context.Background(), 200*time.Millisecond)
	defer cancel()
	bus, _, err := startBus(ctx, cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer bus.stop()
	<-ctx.Done()
	// Startup cancellation/deadline cannot later retire a live authenticated
	// bus. The lifetime pipe alone controls the daemon after address acquisition.
	select {
	case <-bus.terminal:
		t.Fatal("startup deadline remained on live receipt reader")
	case <-time.After(30 * time.Millisecond):
	}
	if facts := bus.stop(); facts.receipt.kind != daemonReceiptWaited {
		t.Fatalf("terminal reader expired after address: %#v", facts)
	}
}

func TestOwnedBusAuthenticationFailureDoesNotPublishAddress(t *testing.T) {
	env := serviceEnvironment(t, sessionservice.Config{Power: "portable", Bus: "start", Notifications: "external"})
	directory := environmentMap(env)["HOME"]
	if err := os.WriteFile(filepath.Join(directory, "dbus-daemon"), []byte("injected"), 0700); err != nil {
		t.Fatal(err)
	}
	cfg := defaults(Config{Environment: append(env, "PATH="+directory, "DBUS_SESSION_BUS_ADDRESS="), BusHelperPath: "/fixture/helper", Command: func(_ string, args ...string) *exec.Cmd {
		if args[0] == "bus-guardian" {
			return guardianCommand("lifetime")
		}
		if len(args) == 1 && args[0] == "probe" {
			// An empty address permits the compiled-feature probe; the
			// explicit address handshake is deliberately rejected below.
			return serviceCommand("auth-failure")
		}
		t.Fatal("authentication failure triggered mutation", args)
		return nil
	}})
	state, err := newServices(&cfg, &reporter{output: io.Discard})
	if err != nil || state.bus == nil {
		t.Fatalf("failed handshake lost ownership: state=%v err=%v", state, err)
	}
	if state.snapshot.Items[2].State != "failed" || environmentMap(cfg.Environment)["DBUS_SESSION_BUS_ADDRESS"] != "" || state.identity.BusID != "" {
		t.Fatalf("unauthenticated address was published: %#v env=%q", state.snapshot, cfg.Environment)
	}
	if facts := state.bus.observations(); !facts.guardianWaitObserved || facts.receipt.kind != daemonReceiptWaited {
		t.Fatalf("handshake failure did not unwind owner: %#v", facts)
	}
}

func TestBusCoreChild(t *testing.T) {
	ack := ""
	for i, arg := range os.Args {
		if arg == "--bus-core" && i+1 < len(os.Args) {
			ack = os.Args[i+1]
			break
		}
	}
	if ack == "" {
		return
	}
	flags, err := unix.FcntlInt(5, unix.F_GETFL, 0)
	var info unix.Stat_t
	if err != nil || flags&unix.O_NONBLOCK == 0 || flags&unix.O_ACCMODE == unix.O_RDONLY || unix.Fstat(5, &info) != nil || info.Mode&unix.S_IFMT != unix.S_IFIFO || os.Getenv(BusLifetimeEnvironment) != "5" {
		os.Exit(95)
	}
	if ack == "pipe-failed" {
		if os.Getenv(ReadyEnvironment) != "" {
			os.Exit(96)
		}
	} else {
		if os.Getenv(ReadyEnvironment) != "3" {
			os.Exit(97)
		}
		ready := os.NewFile(3, "ready")
		io.WriteString(ready, strings.TrimPrefix(ack, "ack:"))
		ready.Close()
	}
	os.Unsetenv(ReadyEnvironment)
	os.Unsetenv(LeaseEnvironment)
	os.Unsetenv(BusLifetimeEnvironment)
	unix.CloseOnExec(5)
	time.Sleep(300 * time.Millisecond)
	os.Exit(0)
}

func TestOwnedBusOldMixedAndMissingAcknowledgmentsSuppressScan(t *testing.T) {
	for _, ack := range []string{"RB", "R", "RL", "RLB", "RBL", "", "pipe-failed"} {
		t.Run(ack, func(t *testing.T) {
			env := serviceEnvironment(t, sessionservice.Config{Power: "portable", Bus: "start", Notifications: "external"})
			directory := environmentMap(env)["HOME"]
			os.WriteFile(filepath.Join(directory, "dbus-daemon"), []byte("injected"), 0700)
			marker := filepath.Join(directory, "scan")
			var output bytes.Buffer
			var wm *exec.Cmd
			cfg := Config{Environment: append(env, "PATH="+directory, "FVWM_SESSION_TEST_SCAN_MARKER="+marker), WMPath: "/wm", SelfPath: "/self", BusHelperPath: "/fixture/helper", Diagnostics: &output,
				Lock: func([]string) (io.Closer, error) { return io.NopCloser(strings.NewReader("")), nil },
				Command: func(path string, args ...string) *exec.Cmd {
					if path == "/wm" {
						wm = exec.Command(os.Args[0], "-test.run=^TestBusCoreChild$", "--", "--bus-core", "ack:"+ack)
						if ack == "pipe-failed" {
							wm = exec.Command(os.Args[0], "-test.run=^TestBusCoreChild$", "--", "--bus-core", ack)
						}
						return wm
					}
					if path == "/self" && len(args) == 1 {
						switch args[0] {
						case ProviderWorkerArgument:
							return providerCommand("discovery")
						case WorkerArgument:
							return childCommand("scan")
						}
					}
					if path != "/fixture/helper" {
						t.Fatal(path)
					}
					if args[0] == "bus-guardian" {
						return guardianCommand("lifetime")
					}
					return serviceCommand("helper", args...)
				},
			}
			if ack == "pipe-failed" {
				cfg.readyPipe = func() (*os.File, *os.File, error) { return nil, nil, errors.New("fixture pipe exhaustion") }
			}
			if code := Run(cfg); code != 0 {
				t.Fatalf("WM status=%d logs=%s", code, output.String())
			}
			data, _ := os.ReadFile(marker)
			want := ""
			if ack == "RB" {
				want = "scan\n"
			}
			if string(data) != want || len(wm.ExtraFiles) != 3 || wm.ExtraFiles[1] != nil || wm.ExtraFiles[2] == nil || (wm.ExtraFiles[0] == nil) != (ack == "pipe-failed") {
				t.Fatalf("ack=%q scan=%q want=%q slots=%v logs=%s", ack, data, want, wm.ExtraFiles, output.String())
			}
			if ack != "RB" && !strings.Contains(output.String(), "bus lifetime handoff unconfirmed") {
				t.Fatal(output.String())
			}
		})
	}
}

func TestBusLifetimeBothWriterCloseOrders(t *testing.T) {
	for _, parentFirst := range []bool{false, true} {
		cfg := guardianConfig(t, "lifetime")
		bus, _, err := startBus(context.Background(), cfg)
		if err != nil {
			t.Fatal(err)
		}
		fd, err := unix.FcntlInt(bus.lifetime.Fd(), unix.F_DUPFD_CLOEXEC, 3)
		if err != nil {
			bus.stop()
			t.Fatal(err)
		}
		coreWriter := os.NewFile(uintptr(fd), "fixture core lifetime holder")
		if parentFirst {
			facts := bus.stopWithin(30 * time.Millisecond)
			if facts.guardianWaitObserved {
				t.Fatalf("parent close retired live second holder: %#v", facts)
			}
			if err := bus.command.Process.Signal(syscall.Signal(0)); err != nil {
				t.Fatal("holder did not keep guardian alive", err)
			}
			coreWriter.Close()
		} else {
			coreWriter.Close()
			if bus.ended() {
				t.Fatal("second holder close retired live parent")
			}
			if facts := bus.stop(); facts.receipt.kind != daemonReceiptWaited {
				t.Fatalf("final writer did not retire bus: %#v", facts)
			}
		}
		select {
		case <-bus.guardianDone:
		case <-time.After(time.Second):
			t.Fatal("last writer close did not finish guardian")
		}
	}
}

func TestOwnedBusCleanupHasReservedDiagnosticSlot(t *testing.T) {
	var output bytes.Buffer
	report := reporter{output: &output, busTerminalReserved: true}
	for i := 0; i < 100; i++ {
		report.write("ordinary %d", i)
	}
	report.terminal("WM termination unconfirmed")
	report.busTerminal("owned session bus cleanup: daemon Wait unconfirmed; guardian Wait unconfirmed")
	report.busTerminal("duplicate bus terminal")
	lines := strings.Split(strings.TrimSuffix(output.String(), "\n"), "\n")
	if len(lines) != maxDiagnostics || !strings.Contains(lines[maxDiagnostics-2], "WM termination unconfirmed") || !strings.Contains(lines[maxDiagnostics-1], "owned session bus cleanup:") {
		t.Fatal(output.String())
	}
}

func TestReleasedLifetimeCannotBeHandedOffBeforeTerminalObservation(t *testing.T) {
	reader, writer, err := busLifetimePipe()
	if err != nil {
		t.Fatal(err)
	}
	defer reader.Close()
	bus := &ownedBus{lifetime: writer, reader: reader, terminal: make(chan struct{}), receiptDone: make(chan struct{}), guardianDone: make(chan struct{})}
	if bus.lifetimeForHandoff() != writer {
		t.Fatal("live writer was not eligible for handoff")
	}
	// Keep terminal observation pending deliberately: release is independently
	// synchronous and cannot rely on the reader goroutine reaching its defer.
	bus.stopWithin(time.Nanosecond)
	if bus.ended() || bus.lifetimeForHandoff() != nil {
		t.Fatal("released writer depended on terminal observation for handoff refusal")
	}
	if _, err := writer.Write([]byte("x")); !errors.Is(err, os.ErrClosed) {
		t.Fatalf("cleanup did not actually close the parent writer: %v", err)
	}
}

func TestFailedAuthenticationCleanupTimeoutStillLaunchesWMWithoutLifetime(t *testing.T) {
	env := serviceEnvironment(t, sessionservice.Config{Power: "portable", Bus: "start", Notifications: "external"})
	directory := environmentMap(env)["HOME"]
	if err := os.WriteFile(filepath.Join(directory, "dbus-daemon"), []byte("injected"), 0700); err != nil {
		t.Fatal(err)
	}
	release := filepath.Join(directory, "guardian-release")
	var guardian, wm *exec.Cmd
	var output bytes.Buffer
	defer func() {
		// Only the fake guardian's explicit sentinel permits its exit. Run's
		// one actual Wait goroutine remains the owner throughout this test.
		if err := os.WriteFile(release, nil, 0600); err != nil {
			t.Error(err)
		}
		if guardian != nil && guardian.Process != nil {
			deadline := time.Now().Add(time.Second)
			for time.Now().Before(deadline) {
				if errors.Is(guardian.Process.Signal(syscall.Signal(0)), os.ErrProcessDone) {
					return
				}
				time.Sleep(5 * time.Millisecond)
			}
			t.Error("released fake guardian actual Wait was not observed")
		}
	}()
	cfg := Config{Environment: append(env, "PATH="+directory, "DBUS_SESSION_BUS_ADDRESS=", "FVWM_TEST_GUARDIAN_RELEASE="+release), BusHelperPath: "/fixture/helper", WMPath: "/wm", SelfPath: "/self", NoAutostart: true, Diagnostics: &output,
		Lock: func([]string) (io.Closer, error) { return io.NopCloser(strings.NewReader("")), nil },
		Command: func(path string, args ...string) *exec.Cmd {
			if path == "/wm" {
				if guardian == nil || guardian.Process.Signal(syscall.Signal(0)) != nil {
					t.Fatal("cleanup timeout did not preserve the outstanding actual guardian")
				}
				wm = childCommand("wm-ready")
				return wm
			}
			if path == "/self" && len(args) == 1 && args[0] == ProviderWorkerArgument {
				return providerCommand("discovery")
			}
			if path != "/fixture/helper" {
				t.Fatal("failed authentication launched unrelated command", path, args)
			}
			if args[0] == "bus-guardian" {
				guardian = guardianCommand("cleanup-pending")
				return guardian
			}
			return serviceCommand("auth-failure", args...)
		},
	}
	if code := Run(cfg); code != 0 {
		t.Fatalf("failed optional bootstrap broke login: status=%d logs=%s", code, output.String())
	}
	if wm == nil || wm.ProcessState == nil || len(wm.ExtraFiles) != 1 || environmentMap(wm.Env)[BusLifetimeEnvironment] != "" || environmentMap(wm.Env)["DBUS_SESSION_BUS_ADDRESS"] != "" {
		t.Fatalf("failed bus adopted closed FD5 or owned address: wm=%v logs=%s", wm, output.String())
	}
	if !strings.Contains(output.String(), "core startup acknowledged") || !strings.Contains(output.String(), "guardian Wait unconfirmed; outstanding actual owner preserved") || strings.Contains(output.String(), "daemon Wait confirmed") {
		t.Fatal(output.String())
	}
}

func TestGuardianStderrDescendant(t *testing.T) {
	found := false
	for _, arg := range os.Args {
		found = found || arg == "--stderr-descendant"
	}
	if !found {
		return
	}
	lockPath := os.Getenv("FVWM_TEST_STDERR_LOCK")
	file, err := os.OpenFile(lockPath, os.O_CREATE|os.O_RDWR, 0600)
	if err != nil || unix.Flock(int(file.Fd()), unix.LOCK_EX|unix.LOCK_NB) != nil {
		os.Exit(95)
	}
	if err := os.WriteFile(lockPath+".ready", nil, 0600); err != nil {
		os.Exit(96)
	}
	// This owned child holds its inherited FD2 until explicit test release,
	// with a finite fallback even if the parent assertion process disappears.
	deadline := time.Now().Add(10 * time.Second)
	for time.Now().Before(deadline) {
		if _, err := os.Stat(os.Getenv("FVWM_TEST_STDERR_RELEASE")); err == nil {
			file.Close()
			os.Exit(0)
		}
		time.Sleep(5 * time.Millisecond)
	}
	file.Close()
	os.Exit(97)
}

func TestGuardianWaitDoesNotWaitForDescendantStderr(t *testing.T) {
	cfg := guardianConfig(t, "stderr-descendant")
	directory := t.TempDir()
	lockPath, release := filepath.Join(directory, "child-lock"), filepath.Join(directory, "child-release")
	observer, err := os.OpenFile(lockPath, os.O_CREATE|os.O_RDWR, 0600)
	if err != nil {
		t.Fatal(err)
	}
	cfg.Environment = append(cfg.Environment, "FVWM_TEST_STDERR_LOCK="+lockPath, "FVWM_TEST_STDERR_RELEASE="+release)
	var bus *ownedBus
	defer func() {
		// Release the actual owned descendant on every assertion path. Its
		// flock is an independently observable live-descriptor witness, not
		// a manufactured child or guardian Wait result.
		if err := os.WriteFile(release, nil, 0600); err != nil {
			t.Error(err)
		}
		if bus != nil {
			bus.stopWithin(time.Second)
		}
		deadline := time.Now().Add(time.Second)
		for {
			if err := unix.Flock(int(observer.Fd()), unix.LOCK_EX|unix.LOCK_NB); err == nil {
				unix.Flock(int(observer.Fd()), unix.LOCK_UN)
				break
			}
			if time.Now().After(deadline) {
				t.Error("released owned descendant retained its live descriptor")
				break
			}
			time.Sleep(5 * time.Millisecond)
		}
		observer.Close()
	}()
	bus, _, err = startBus(context.Background(), cfg)
	if err != nil {
		t.Fatal(err)
	}
	waitRecoveryMarker(t, lockPath+".ready")
	if err := bus.command.Process.Kill(); err != nil {
		t.Fatal(err)
	}
	select {
	case <-bus.guardianDone:
	case <-time.After(time.Second):
		t.Fatal("actual guardian Wait was delayed by a surviving descendant's FD2")
	}
	facts := bus.observations()
	if !facts.guardianWaitObserved || exitCode(facts.guardianWaitError) != 128+int(syscall.SIGKILL) || facts.receipt.kind == daemonReceiptWaited {
		t.Fatalf("guardian SIGKILL was not an independent actual Wait: %#v", facts)
	}
	if err := unix.Flock(int(observer.Fd()), unix.LOCK_EX|unix.LOCK_NB); err != unix.EWOULDBLOCK && err != unix.EAGAIN {
		if err == nil {
			unix.Flock(int(observer.Fd()), unix.LOCK_UN)
		}
		t.Fatalf("descendant was not still alive holding inherited FD2: %v", err)
	}
}

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
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
	"testing"
	"time"

	"golang.org/x/sys/unix"
)

func TestLocalDisplayKeyPreservesLiteralIdentity(t *testing.T) {
	for _, tc := range []struct{ input, want string }{
		{":0", ":0"}, {":0.0", ":0"}, {":123.0", ":123"},
		{":001.0", ":001"}, {":0.1", ":0.1"}, {":0.00", ":0.00"},
		{"host:0.0", "host:0.0"}, {"unix:0.0", "unix:0.0"},
		{"localhost:0.0", "localhost:0.0"}, {":-1.0", ":-1.0"},
		{":.0", ":.0"}, {":1x.0", ":1x.0"}, {":1.0 ", ":1.0 "},
		{"", ""}, {":1.0.0", ":1.0.0"},
	} {
		if got := localDisplayKey(tc.input); got != tc.want {
			t.Errorf("key(%q)=%q want %q", tc.input, got, tc.want)
		}
	}
}

func recoveryEnvironment(t *testing.T) []string {
	t.Helper()
	directory := filepath.Join(t.TempDir(), "runtime")
	if err := os.Mkdir(directory, 0700); err != nil {
		t.Fatal(err)
	}
	return append(isolatedEnvironment(), "DISPLAY=:84.0", "XDG_RUNTIME_DIR="+directory)
}

func TestLocalScreenZeroUsesOnePersistentLock(t *testing.T) {
	env := recoveryEnvironment(t)
	lock, err := acquireLock(env)
	if err != nil {
		t.Fatal(err)
	}
	defer lock.Close()
	if _, ok := lock.(*os.File); !ok {
		t.Fatal("production lock is not an inheritable file")
	}
	if duplicate, err := acquireLock(displayEnvironment(env, ":84")); err == nil {
		duplicate.Close()
		t.Fatal("implicit and explicit screen zero admitted two owners")
	}
	other, err := acquireLock(displayEnvironment(env, ":84.1"))
	if err != nil {
		t.Fatal("different screen refused", err)
	}
	other.Close()
}

func TestLeaseReadinessRequiresPositiveAcknowledgment(t *testing.T) {
	for _, data := range []string{"RL", "R", "", "X", "RLL", "R\x00"} {
		t.Run(data, func(t *testing.T) {
			reader, writer, err := os.Pipe()
			if err != nil {
				t.Fatal(err)
			}
			defer reader.Close()
			io.WriteString(writer, data)
			writer.Close()
			if err := waitReady(reader, time.Second, true, false); (err == nil) != (data == "RL") {
				t.Fatalf("ack %q: %v", data, err)
			}
		})
	}
}

// These modes use only this test image and owned children. Markers are written
// after signal handlers and descriptor checks, so stop tests do not race exec.
func TestRecoveryChild(t *testing.T) {
	mode := ""
	for i, arg := range os.Args {
		if arg == "--recovery" && i+1 < len(os.Args) {
			mode = os.Args[i+1]
			break
		}
	}
	if mode == "" {
		return
	}
	signals := make(chan os.Signal, 16)
	signal.Notify(signals, syscall.SIGTERM, syscall.SIGINT, syscall.SIGHUP)
	marker := os.Getenv("FVWM_RECOVERY_MARKER")
	if mode == "application" {
		marker += "-app"
	}
	if mode == "scan-stalled" {
		marker += "-scan"
	}
	if mode == "stalled-ready" || mode == "orderly-ready" {
		file := os.NewFile(3, "ready")
		writeTestCoreReady(file)
		file.Close()
	}
	if strings.HasPrefix(mode, "lease-") {
		wantReady := "3"
		if mode == "lease-no-ready" {
			wantReady = ""
		}
		if os.Getenv(ReadyEnvironment) != wantReady || os.Getenv(LeaseEnvironment) != "4" || os.Getenv("DISPLAY") != ":84.0" {
			os.Exit(90)
		}
		var info unix.Stat_t
		if unix.Fstat(4, &info) != nil || info.Mode&unix.S_IFMT != unix.S_IFREG || unix.Flock(4, unix.LOCK_EX|unix.LOCK_NB) != nil {
			os.Exit(91)
		}
		if mode == "lease-no-ready" {
			os.Exit(0)
		}
		ready := os.NewFile(3, "ready")
		ack := "RL"
		switch mode {
		case "lease-legacy":
			ack = "R"
		case "lease-invalid":
			ack = "RX"
		case "lease-missing":
			ack = ""
		}
		io.WriteString(ready, ack)
		ready.Close()
		if mode != "lease-hold" {
			// Allow the readiness and scan goroutines to finish before WM exit.
			time.Sleep(300 * time.Millisecond)
			os.Exit(0)
		}
	}
	if marker != "" {
		if err := os.WriteFile(marker+"-ready", []byte("ready"), 0600); err != nil {
			os.Exit(92)
		}
	}
	for {
		sig := <-signals
		if marker != "" {
			file, err := os.OpenFile(marker, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0600)
			if err != nil {
				os.Exit(93)
			}
			io.WriteString(file, sig.String()+"\n")
			file.Close()
		}
		if mode == "orderly" || mode == "orderly-ready" {
			os.Exit(23)
		}
		if mode == "lease-hold" {
			os.Exit(0)
		}
	}
}

func recoveryCommand(mode string) *exec.Cmd {
	return exec.Command(os.Args[0], "-test.run=^TestRecoveryChild$", "--", "--recovery", mode)
}

func waitRecoveryMarker(t *testing.T, path string) {
	t.Helper()
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) {
		if _, err := os.Stat(path); err == nil {
			return
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatalf("child marker unavailable: %s", path)
}

func TestOwnedFileLeaseRequiresRLBeforeScan(t *testing.T) {
	for _, mode := range []string{"lease-ready", "lease-legacy", "lease-invalid", "lease-missing"} {
		t.Run(mode, func(t *testing.T) {
			marker := filepath.Join(t.TempDir(), "scan")
			env := append(recoveryEnvironment(t), "FVWM_SESSION_TEST_SCAN_MARKER="+marker, LeaseEnvironment+"=99")
			var wm *exec.Cmd
			cfg := Config{DisableServices: true, WMPath: "/wm", SelfPath: "/self", Environment: env, ReadyTimeout: time.Second,
				Command: func(path string, args ...string) *exec.Cmd {
					if path == "/wm" {
						wm = recoveryCommand(mode)
						return wm
					}
					return childCommand("scan")
				},
			}
			if code := Run(cfg); code != 0 {
				t.Fatalf("WM status %d", code)
			}
			data, _ := os.ReadFile(marker)
			want := ""
			if mode == "lease-ready" {
				want = "scan\n"
			}
			if string(data) != want || len(wm.ExtraFiles) != 2 {
				t.Fatalf("scan=%q want=%q inherited=%d", data, want, len(wm.ExtraFiles))
			}
		})
	}
}

func TestNoAutostartStillAcknowledgesInheritedLease(t *testing.T) {
	var output bytes.Buffer
	cfg := Config{DisableServices: true, NoAutostart: true, WMPath: "/wm", Environment: recoveryEnvironment(t), Diagnostics: &output,
		Command: func(path string, args ...string) *exec.Cmd {
			if path != "/wm" {
				t.Fatal("no-autostart constructed acquisition worker")
			}
			return recoveryCommand("lease-ready")
		},
	}
	if Run(cfg) != 0 || !strings.Contains(output.String(), "core startup acknowledged") {
		t.Fatal(output.String())
	}
}

func TestReadinessPipeFailureStillLaunchesWMWithLease(t *testing.T) {
	var output bytes.Buffer
	var wm *exec.Cmd
	cfg := Config{DisableServices: true, WMPath: "/wm", Environment: recoveryEnvironment(t), Diagnostics: &output,
		readyPipe: func() (*os.File, *os.File, error) { return nil, nil, errors.New("fixture descriptor exhaustion") },
		Command: func(path string, args ...string) *exec.Cmd {
			if path != "/wm" {
				t.Fatal("pipe failure started acquisition")
			}
			wm = recoveryCommand("lease-no-ready")
			return wm
		},
	}
	if code := Run(cfg); code != 0 || !strings.Contains(output.String(), "readiness pipe: fixture descriptor exhaustion") || strings.Contains(output.String(), "core startup acknowledged") || strings.Contains(output.String(), "autostart scan started") {
		t.Fatalf("status=%d output=%q", code, output.String())
	}
	if len(wm.ExtraFiles) != 2 || wm.ExtraFiles[0] != nil || wm.ExtraFiles[1] == nil {
		t.Fatal("failed pipe did not reserve closed FD3 and lease FD4", wm.ExtraFiles)
	}
}

func TestInheritedLeaseSurvivesParentDescriptorClose(t *testing.T) {
	env := recoveryEnvironment(t)
	marker := filepath.Join(t.TempDir(), "lease")
	lock, err := acquireLock(env)
	if err != nil {
		t.Fatal(err)
	}
	defer lock.Close()
	reader, writer, err := readinessPipe()
	if err != nil {
		t.Fatal(err)
	}
	defer reader.Close()
	defer writer.Close()
	command := recoveryCommand("lease-hold")
	command.Env = append(env, ReadyEnvironment+"=3", LeaseEnvironment+"=4", "FVWM_RECOVERY_MARKER="+marker)
	command.ExtraFiles = []*os.File{writer, lock.(*os.File)}
	if err := command.Start(); err != nil {
		t.Fatal(err)
	}
	waited := false
	defer func() {
		if !waited {
			command.Process.Kill()
			command.Wait()
		}
	}()
	writer.Close()
	if err := waitReady(reader, time.Second, true, false); err != nil {
		t.Fatal(err)
	}
	waitRecoveryMarker(t, marker+"-ready")
	lock.Close() // Never LOCK_UN: the child owns the same open-file description.
	if duplicate, err := acquireLock(displayEnvironment(env, ":84")); err == nil {
		duplicate.Close()
		t.Fatal("closing the parent copy released the child's lease")
	}
	command.Process.Signal(syscall.SIGTERM)
	err = command.Wait()
	waited = true
	if err != nil {
		t.Fatal(err)
	}
	again, err := acquireLock(env)
	if err != nil {
		t.Fatal("actual child exit did not release lease", err)
	}
	again.Close()
}

func TestStopCoalescesAndReapsOnlyDirectWM(t *testing.T) {
	for _, mode := range []string{"orderly", "stalled"} {
		t.Run(mode, func(t *testing.T) {
			marker := filepath.Join(t.TempDir(), "wm")
			independent := recoveryCommand("independent")
			independent.Env = append(isolatedEnvironment(), "FVWM_RECOVERY_MARKER="+marker+"-app")
			if err := independent.Start(); err != nil {
				t.Fatal(err)
			}
			defer func() { independent.Process.Kill(); independent.Wait() }()
			waitRecoveryMarker(t, marker+"-app-ready")
			signals := make(chan os.Signal, 16)
			commands := make(chan *exec.Cmd, 1)
			cfg := Config{DisableServices: true, NoAutostart: true, WMPath: "/wm", Environment: append(isolatedEnvironment(), "FVWM_RECOVERY_MARKER="+marker), Signals: signals, StopGrace: 200 * time.Millisecond, StopWait: time.Second,
				Lock: func([]string) (io.Closer, error) { return io.NopCloser(strings.NewReader("")), nil },
				Command: func(string, ...string) *exec.Cmd {
					command := recoveryCommand(mode)
					commands <- command
					return command
				},
			}
			done := make(chan int, 1)
			go func() { done <- Run(cfg) }()
			wm := <-commands
			defer func() {
				if wm != nil && wm.Process != nil {
					wm.Process.Kill()
				}
			}()
			waitRecoveryMarker(t, marker+"-ready")
			signals <- syscall.SIGTERM
			waitRecoveryMarker(t, marker)
			// Keep presenting signals across the original grace deadline. A
			// resetting timer would postpone Kill until this stream stops.
			ticker := time.NewTicker(15 * time.Millisecond)
			defer ticker.Stop()
			deadline := time.NewTimer(900 * time.Millisecond)
			defer deadline.Stop()
			for {
				select {
				case code := <-done:
					want := 23
					if mode == "stalled" {
						want = 128 + int(syscall.SIGKILL)
					}
					if code != want || wm.ProcessState == nil {
						t.Fatalf("status=%d want=%d state=%v", code, want, wm.ProcessState)
					}
					data, _ := os.ReadFile(marker)
					if len(strings.Split(strings.TrimSpace(string(data)), "\n")) != 1 {
						t.Fatalf("stop signal rebroadcast: %q", data)
					}
					if err := independent.Process.Signal(syscall.Signal(0)); err != nil {
						t.Fatal("independent child was terminated", err)
					}
					if data, _ := os.ReadFile(marker + "-app"); len(data) != 0 {
						t.Fatalf("independent child was signaled: %q", data)
					}
					return
				case <-ticker.C:
					select {
					case signals <- syscall.SIGHUP:
					default:
					}
				case <-deadline.C:
					t.Fatal("repeated signals extended the fixed shutdown deadline")
				}
			}
		})
	}
}

func TestStalledStopPreservesLaunchedApplication(t *testing.T) {
	marker := filepath.Join(t.TempDir(), "wm")
	signals := make(chan os.Signal, 1)
	applications := make(chan *exec.Cmd, 1)
	cores := make(chan *exec.Cmd, 1)
	cfg := Config{DisableServices: true, WMPath: "/wm", SelfPath: "/self", Environment: append(isolatedEnvironment(), "FVWM_RECOVERY_MARKER="+marker), Signals: signals, StopGrace: 100 * time.Millisecond, StopWait: time.Second,
		Lock: func([]string) (io.Closer, error) { return io.NopCloser(strings.NewReader("")), nil },
		Command: func(path string, args ...string) *exec.Cmd {
			switch path {
			case "/wm":
				command := recoveryCommand("stalled-ready")
				cores <- command
				return command
			case "/self":
				return childCommand("scan-app")
			default:
				command := recoveryCommand("application")
				applications <- command
				return command
			}
		},
	}
	done := make(chan int, 1)
	go func() { done <- Run(cfg) }()
	wm := <-cores
	defer func() { wm.Process.Kill() }()
	waitRecoveryMarker(t, marker+"-app-ready")
	app := <-applications
	defer func() { app.Process.Kill() }() // Run's application waiter owns Wait.
	signals <- syscall.SIGTERM
	select {
	case code := <-done:
		if code != 128+int(syscall.SIGKILL) {
			t.Fatalf("actual WM status %d", code)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("stalled WM shutdown did not finish")
	}
	if err := app.Process.Signal(syscall.Signal(0)); err != nil {
		t.Fatal("autostart application was terminated", err)
	}
	if data, _ := os.ReadFile(marker + "-app"); len(data) != 0 {
		t.Fatalf("autostart application was signaled: %q", data)
	}
}

func TestCancelledScanDoesNotStartNewWorker(t *testing.T) {
	for _, duringConstruction := range []bool{false, true} {
		ctx, cancel := context.WithCancel(context.Background())
		var command *exec.Cmd
		cfg := Config{Environment: isolatedEnvironment(), Command: func(string, ...string) *exec.Cmd {
			if !duringConstruction {
				t.Fatal("cancelled scan constructed a worker")
			}
			command = childCommand("scan")
			cancel()
			return command
		}}
		if !duringConstruction {
			cancel()
		}
		_, err := scanWorker(ctx, cfg)
		cancel()
		if !errors.Is(err, context.Canceled) || command != nil && command.Process != nil {
			t.Fatalf("cancelled scan started: command=%#v err=%v", command, err)
		}
	}
}

func TestWMExitDrainsAndReapsActiveAcquisition(t *testing.T) {
	marker := filepath.Join(t.TempDir(), "wm")
	type children struct{ wm, worker *exec.Cmd }
	commands := make(chan children, 1)
	var wm *exec.Cmd
	cfg := Config{DisableServices: true, WMPath: "/wm", SelfPath: "/self", Environment: append(isolatedEnvironment(), "FVWM_RECOVERY_MARKER="+marker), ScanTimeout: 10 * time.Second,
		Lock: func([]string) (io.Closer, error) { return io.NopCloser(strings.NewReader("")), nil },
		Command: func(path string, args ...string) *exec.Cmd {
			if path == "/wm" {
				wm = recoveryCommand("orderly-ready")
				return wm
			}
			worker := recoveryCommand("scan-stalled")
			// Acquisition is constructed after the acknowledged WM Start;
			// this channel publishes that initialized process handle.
			commands <- children{wm, worker}
			return worker
		},
	}
	done := make(chan int, 1)
	go func() { done <- Run(cfg) }()
	var owned children
	select {
	case owned = <-commands:
	case <-time.After(3 * time.Second):
		t.Fatal("acknowledged WM did not start acquisition")
	}
	defer func() { owned.wm.Process.Kill() }()
	defer func() {
		if owned.worker.Process != nil {
			owned.worker.Process.Kill() // scanWorker's waiter owns the sole Wait.
		}
	}()
	waitRecoveryMarker(t, marker+"-scan-ready")
	// This is an ordinary owned-WM exit, independent of the parent's explicit
	// stop branch. Run must cancel, terminate and actually reap this scan.
	if err := owned.wm.Process.Signal(syscall.SIGTERM); err != nil {
		t.Fatal(err)
	}
	select {
	case code := <-done:
		if code != 23 {
			t.Fatalf("WM exit status %d", code)
		}
		if owned.worker.ProcessState == nil {
			t.Fatalf("active acquisition was not actually reaped: %v", owned.worker.ProcessState)
		}
		status, ok := owned.worker.ProcessState.Sys().(syscall.WaitStatus)
		if !ok || !status.Signaled() || status.Signal() != syscall.SIGKILL {
			t.Fatalf("active acquisition was not terminated by its owner: %v", owned.worker.ProcessState)
		}
	case <-time.After(3 * time.Second):
		t.Fatal("bounded acquisition shutdown did not finish")
	}
}

func TestClosedSignalsDoNotStopWM(t *testing.T) {
	signals := make(chan os.Signal)
	close(signals)
	cfg := Config{DisableServices: true, NoAutostart: true, WMPath: "/wm", Signals: signals,
		Lock:    func([]string) (io.Closer, error) { return io.NopCloser(strings.NewReader("")), nil },
		Command: func(string, ...string) *exec.Cmd { return childCommand("app") },
	}
	if code := Run(cfg); code != 7 {
		t.Fatalf("closed signal channel changed actual status: %d", code)
	}
}

func TestAwaitWMExitRequiresAnActualWaitResult(t *testing.T) {
	actual := errors.New("actual Wait failure")
	for _, tc := range []struct {
		name string
		done chan error
		want bool
	}{
		{"pending", make(chan error), false},
		{"closed-empty", make(chan error), false},
		{"actual", make(chan error, 1), true},
		{"closed-actual", make(chan error, 1), true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			if tc.want {
				tc.done <- actual
			}
			if strings.HasPrefix(tc.name, "closed") {
				close(tc.done)
			}
			err, confirmed := awaitWMExit(tc.done, 10*time.Millisecond)
			if confirmed != tc.want || confirmed && !errors.Is(err, actual) {
				t.Fatalf("err=%v confirmed=%v want=%v", err, confirmed, tc.want)
			}
		})
	}
}

func TestTerminalDiagnosticHasOneReservedBoundedSlot(t *testing.T) {
	var output bytes.Buffer
	report := reporter{output: &output}
	for i := 0; i < 100; i++ {
		report.write("ordinary %d", i)
	}
	report.terminal("WM termination unconfirmed\n%s", strings.Repeat("x", 1024))
	report.terminal("duplicate terminal")
	lines := strings.Split(strings.TrimSuffix(output.String(), "\n"), "\n")
	if len(lines) != maxDiagnostics || !strings.Contains(lines[maxDiagnostics-1], "WM termination unconfirmed") || len(lines[maxDiagnostics-1])+1 > maxDiagnosticBytes {
		t.Fatalf("reserved terminal diagnostic lost or unbounded: %q", output.String())
	}
}

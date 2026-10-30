// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"bytes"
	"context"
	"encoding/json"
	"io"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"reflect"
	"strings"
	"syscall"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"golang.org/x/sys/unix"
)

func TestEnvironmentPreservesPresentValues(t *testing.T) {
	input := []string{"DISPLAY=:9", "XDG_CURRENT_DESKTOP=Mine", "XDG_SESSION_TYPE=", "SESSION_MANAGER=external", ReadyEnvironment + "=99"}
	got := Environment(input)
	if strings.Join(got, "\n") != strings.Join(input[:4], "\n") {
		t.Fatalf("changed existing environment: %q", got)
	}
	got = Environment([]string{"DISPLAY=:9"})
	if !reflect.DeepEqual(got, []string{"DISPLAY=:9", "XDG_CURRENT_DESKTOP=FVWM3:FVWM", "XDG_SESSION_TYPE=x11"}) {
		t.Fatal(got)
	}
}

func TestReadyPipeOutcomes(t *testing.T) {
	for _, tc := range []struct {
		name, data  string
		close, want bool
	}{
		{"ready", "R", true, true}, {"empty", "", true, false},
		{"wrong", "X", true, false}, {"extra", "RR", true, false},
		{"timeout", "", false, false},
	} {
		t.Run(tc.name, func(t *testing.T) {
			r, w, err := os.Pipe()
			if err != nil {
				t.Fatal(err)
			}
			defer r.Close()
			defer w.Close()
			if tc.data != "" {
				if _, err = io.WriteString(w, tc.data); err != nil {
					t.Fatal(err)
				}
			}
			if tc.close {
				w.Close()
			}
			if err = waitReady(r, 20*time.Millisecond); (err == nil) != tc.want {
				t.Fatalf("ready: %v", err)
			}
		})
	}
}

// The test executable is the only child image used here. Its private modes let
// tests observe real exec, pipes and signals without depending on shell tools.
func TestSessionChild(t *testing.T) {
	mode := ""
	for i, arg := range os.Args {
		if arg == "--" && i+1 < len(os.Args) {
			mode = os.Args[i+1]
			break
		}
	}
	if mode == "" {
		return
	}
	switch mode {
	case "wm-ready", "wm-restart", "wm-wrong", "wm-eof", "wm-silent":
		if mode == "wm-restart" && os.Getenv("FVWM_SESSION_TEST_RESTARTED") == "1" {
			if os.Getenv(ReadyEnvironment) != "" {
				os.Exit(91)
			}
		} else {
			flags, err := unix.FcntlInt(3, unix.F_GETFL, 0)
			if err != nil || flags&unix.O_NONBLOCK == 0 {
				os.Exit(94)
			}
			f := os.NewFile(3, "ready")
			os.Unsetenv(ReadyEnvironment)
			if mode == "wm-ready" || mode == "wm-restart" {
				f.Write([]byte("R"))
			}
			if mode == "wm-wrong" {
				f.Write([]byte("X"))
			}
			if mode != "wm-silent" {
				f.Close()
			}
			if mode == "wm-restart" {
				env := append(os.Environ(), "FVWM_SESSION_TEST_RESTARTED=1")
				syscall.Exec(os.Args[0], os.Args, env)
				os.Exit(92)
			}
		}
		sig := make(chan os.Signal, 1)
		signal.Notify(sig, syscall.SIGTERM)
		select {
		case <-sig:
		case <-time.After(time.Second):
		}
		os.Exit(0)
	case "scan":
		if marker := os.Getenv("FVWM_SESSION_TEST_SCAN_MARKER"); marker != "" {
			f, _ := os.OpenFile(marker, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0600)
			if f != nil {
				f.WriteString("scan\n")
				f.Close()
			}
		}
		io.WriteString(os.Stdout, `{"Launches":[],"Diagnostics":[]}`)
	case "scan-hang":
		time.Sleep(5 * time.Second)
	case "scan-overflow":
		io.WriteString(os.Stdout, strings.Repeat("x", 512*1024+1))
	case "scan-app":
		scan := xdgmenu.AutostartScan{Launches: []xdgmenu.AutostartLaunch{{Filename: "app.desktop", Path: "/fixture/app.desktop", Name: "App", Argv: []string{"/fixture/app", "literal ; $(text)"}}}}
		data, _ := json.Marshal(scan)
		os.Stdout.Write(data)
	case "app":
		os.Exit(7)
	case "wm-no-ready":
		time.Sleep(time.Second)
		os.Exit(95)
	default:
		os.Exit(93)
	}
	os.Exit(0)
}

func childCommand(mode string, args ...string) *exec.Cmd {
	cmd := exec.Command(os.Args[0], append([]string{"-test.run=^TestSessionChild$", "--", mode}, args...)...)
	return cmd
}

func isolatedEnvironment() []string {
	// Preserve race instrumentation while avoiding its default one-second
	// exit delay, which would turn helper exit ordering into a timing race.
	return append(os.Environ(), "SESSION_MANAGER=", "PATH=/usr/bin:/bin",
		"GORACE="+strings.TrimSpace(os.Getenv("GORACE")+" atexit_sleep_ms=0"))
}

func TestExternalSessionAndUnavailableLockExecOriginal(t *testing.T) {
	for _, external := range []bool{true, false} {
		var gotPath string
		var gotArgs, gotEnv []string
		lockCalled := false
		cfg := Config{DisableServices: true, WMPath: "/fixed/fvwm3", SelfPath: "/fixed/fvwm-session", Args: []string{"-d", ":77", "-f", "a ; b"}, Environment: []string{"DISPLAY=:9"}, Diagnostics: io.Discard,
			Exec:    func(path string, args, env []string) error { gotPath = path; gotArgs = args; gotEnv = env; return nil },
			Lock:    func([]string) (io.Closer, error) { lockCalled = true; return nil, os.ErrPermission },
			Command: func(string, ...string) *exec.Cmd { t.Fatal("unexpected child"); return nil },
		}
		if external {
			cfg.Environment = append(cfg.Environment, "SESSION_MANAGER=external")
		}
		if code := Run(cfg); code != 0 || gotPath != cfg.WMPath || !reflect.DeepEqual(gotArgs, append([]string{cfg.WMPath}, cfg.Args...)) {
			t.Fatalf("exec %d %q %q", code, gotPath, gotArgs)
		}
		if environmentMap(gotEnv)["XDG_CURRENT_DESKTOP"] != "FVWM3:FVWM" {
			t.Fatal(gotEnv)
		}
		if lockCalled == external {
			t.Fatalf("external=%v lock called=%v", external, lockCalled)
		}
		wantDisplay := ":77"
		if external {
			wantDisplay = ":9"
		}
		if got := environmentMap(gotEnv)["DISPLAY"]; got != wantDisplay {
			t.Fatalf("external=%v DISPLAY=%q", external, got)
		}
	}
}

func TestUncertainDisplayExecsOriginalWithoutOwning(t *testing.T) {
	for _, args := range [][]string{{"--future-option", "-d", ":target"}, {"-f"}, {"--help"}} {
		called := false
		cfg := Config{DisableServices: true, WMPath: "/fixed/fvwm3", SelfPath: "/fixed/fvwm-session", Args: args, Environment: []string{"DISPLAY=:inherited"},
			Lock:    func([]string) (io.Closer, error) { t.Fatal("uncertain invocation acquired lock"); return nil, nil },
			Command: func(string, ...string) *exec.Cmd { t.Fatal("uncertain invocation started child"); return nil },
			Exec: func(path string, argv, env []string) error {
				called = true
				if path != "/fixed/fvwm3" || !reflect.DeepEqual(argv, append([]string{path}, args...)) || environmentMap(env)["DISPLAY"] != ":inherited" {
					t.Fatalf("%q %q %q", path, argv, env)
				}
				return nil
			},
		}
		if Run(cfg) != 0 || !called {
			t.Fatal("missing direct-core fallback")
		}
	}
}

func TestOwnedWMScansOnceOnlyAfterReady(t *testing.T) {
	for _, mode := range []string{"wm-ready", "wm-restart", "wm-wrong", "wm-eof", "wm-silent"} {
		t.Run(mode, func(t *testing.T) {
			marker := filepath.Join(t.TempDir(), "scans")
			env := append(isolatedEnvironment(), "FVWM_SESSION_TEST_SCAN_MARKER="+marker)
			var diagnostics bytes.Buffer
			cfg := Config{DisableServices: true, WMPath: "/wm", SelfPath: "/self", Environment: env, Diagnostics: &diagnostics, ReadyTimeout: 500 * time.Millisecond, ScanTimeout: 2 * time.Second,
				Lock: func([]string) (io.Closer, error) { return io.NopCloser(strings.NewReader("")), nil },
				Command: func(path string, args ...string) *exec.Cmd {
					m := "scan"
					if path == "/wm" {
						m = mode
					}
					return childCommand(m, args...)
				},
			}
			if code := Run(cfg); code != 0 {
				t.Fatalf("code=%d %s", code, diagnostics.String())
			}
			data, _ := os.ReadFile(marker)
			want := ""
			if mode == "wm-ready" || mode == "wm-restart" {
				want = "scan\n"
			}
			if string(data) != want {
				t.Fatalf("scans=%q want=%q %s", data, want, diagnostics.String())
			}
		})
	}
}

func TestWorkerRefusesTimeoutAndOverflow(t *testing.T) {
	for _, mode := range []string{"scan-hang", "scan-overflow"} {
		t.Run(mode, func(t *testing.T) {
			timeout, want := 40*time.Millisecond, "deadline/cancellation"
			if mode == "scan-overflow" {
				timeout = 2 * time.Second
				want = "excessive"
			}
			cfg := Config{DisableServices: true, SelfPath: "/self", Environment: isolatedEnvironment(), ScanTimeout: timeout, Command: func(string, ...string) *exec.Cmd { return childCommand(mode) }}
			if _, err := scanWorker(context.Background(), cfg); err == nil || !strings.Contains(err.Error(), want) {
				t.Fatalf("expected %s refusal, got %v", want, err)
			}
		})
	}
}

func TestMissingWMFailsWithoutScan(t *testing.T) {
	var diagnostics bytes.Buffer
	cfg := Config{DisableServices: true, WMPath: filepath.Join(t.TempDir(), "missing"), SelfPath: "/self", Environment: isolatedEnvironment(), Diagnostics: &diagnostics, Lock: func([]string) (io.Closer, error) { return io.NopCloser(strings.NewReader("")), nil }}
	if Run(cfg) == 0 || !strings.Contains(diagnostics.String(), "WM start failed") {
		t.Fatal(diagnostics.String())
	}
}

func TestApplicationStartExitFailureAndLiteralArguments(t *testing.T) {
	for _, missing := range []bool{false, true} {
		var diagnostics bytes.Buffer
		var appArgs, coreArgs []string
		var wmCommand, workerCommand, appCommand *exec.Cmd
		cfg := Config{DisableServices: true, WMPath: "/wm", SelfPath: "/self", Args: []string{"-f", "-d", "--display", ":27"}, Environment: append(isolatedEnvironment(), "DISPLAY=:invalid-inherited", "XAUTHORITY=/fixture/auth"), Diagnostics: &diagnostics,
			Lock: func(env []string) (io.Closer, error) {
				if environmentMap(env)["DISPLAY"] != ":27" {
					t.Fatalf("wrong lock display: %q", env)
				}
				return io.NopCloser(strings.NewReader("")), nil
			},
			Command: func(path string, args ...string) *exec.Cmd {
				switch path {
				case "/wm":
					coreArgs = append([]string(nil), args...)
					wmCommand = childCommand("wm-ready", args...)
					return wmCommand
				case "/self":
					workerCommand = childCommand("scan-app", args...)
					return workerCommand
				default:
					appArgs = append([]string(nil), args...)
					if missing {
						appCommand = exec.Command("/no-such-fvwm-session-application")
						return appCommand
					}
					appCommand = childCommand("app", args...)
					return appCommand
				}
			},
		}
		if Run(cfg) != 0 {
			t.Fatal(diagnostics.String())
		}
		if !reflect.DeepEqual(appArgs, []string{"literal ; $(text)"}) {
			t.Fatal(appArgs)
		}
		if !reflect.DeepEqual(coreArgs, cfg.Args) {
			t.Fatalf("rewritten core args: %q", coreArgs)
		}
		for _, command := range []*exec.Cmd{wmCommand, workerCommand, appCommand} {
			if command == nil {
				t.Fatal("missing child")
			}
			env := environmentMap(command.Env)
			if env["DISPLAY"] != ":27" || env["XAUTHORITY"] != "/fixture/auth" {
				t.Fatalf("wrong child environment: %q", command.Env)
			}
		}
		if missing {
			if !strings.Contains(diagnostics.String(), "failed: app.desktop:") {
				t.Fatal(diagnostics.String())
			}
		} else if !strings.Contains(diagnostics.String(), "started: app.desktop") || !strings.Contains(diagnostics.String(), "exited: app.desktop: status 7") {
			t.Fatal(diagnostics.String())
		}
	}
}

func TestSignalsForwardOnlyToDirectWM(t *testing.T) {
	var diagnostics bytes.Buffer
	signals := make(chan os.Signal, 1)
	var wm *exec.Cmd
	cfg := Config{DisableServices: true, WMPath: "/wm", SelfPath: "/self", Environment: isolatedEnvironment(), Diagnostics: &diagnostics, Signals: signals, NoAutostart: true,
		Lock: func([]string) (io.Closer, error) { return io.NopCloser(strings.NewReader("")), nil },
		Command: func(path string, args ...string) *exec.Cmd {
			if path != "/wm" {
				t.Fatal("unexpected scan/app")
			}
			wm = childCommand("wm-no-ready", args...)
			return wm
		},
	}
	// A queued shutdown is forwarded as soon as the owned WM exists. The
	// helper has no ready pipe in this mode; signal termination is observable.
	signals <- syscall.SIGTERM
	if code := Run(cfg); code != 128+int(syscall.SIGTERM) {
		t.Fatalf("%d %s", code, diagnostics.String())
	}
	if wm.ProcessState == nil {
		t.Fatal("WM was not reaped")
	}
}

func TestDiagnosticsAreFiniteAndSingleLine(t *testing.T) {
	var output bytes.Buffer
	r := reporter{output: &output}
	for i := 0; i < 100; i++ {
		r.write("entry\n%s", strings.Repeat("x", 1024))
	}
	lines := strings.Split(strings.TrimSuffix(output.String(), "\n"), "\n")
	if len(lines) != 64 {
		t.Fatal(len(lines))
	}
	for _, line := range lines {
		if len(line)+1 > 512 {
			t.Fatal(len(line))
		}
	}
}

func TestWorkerJSONIsCompleteBeforeExposure(t *testing.T) {
	for _, data := range []string{"", `null`, `{"Launches":[],"extra":1}`, `{"Launches":[]} {}`, `{"Launches":[{"Filename":"x.desktop","Argv":["/app"]}]}`, strings.Repeat(" ", 512*1024+1)} {
		if _, err := decodeScan([]byte(data)); err == nil {
			t.Fatalf("accepted %.100q", data)
		}
	}
}

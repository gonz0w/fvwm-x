// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package sessionowner supervises only its direct WM and acquisition worker.
// Applications are independent children: a successful Start is not readiness.
package sessionowner

import (
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"strings"
	"syscall"
	"time"
	"unicode"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"golang.org/x/sys/unix"
)

const (
	ReadyEnvironment   = "FVWM_SESSION_READY_FD"
	WorkerArgument     = "--_session-scan"
	readyTimeout       = 15 * time.Second
	scanTimeout        = 5 * time.Second
	maxDiagnostics     = 64
	maxDiagnosticBytes = 512
)

// Command, Exec, Lock and deadlines allow real child/pipe regression tests;
// the installed CLI supplies fixed paths and the production defaults only.
type Config struct {
	WMPath, SelfPath          string
	Args, Environment         []string
	NoAutostart               bool
	Signals                   <-chan os.Signal
	Diagnostics               io.Writer
	Command                   func(string, ...string) *exec.Cmd
	Exec                      func(string, []string, []string) error
	Lock                      func([]string) (io.Closer, error)
	ReadyTimeout, ScanTimeout time.Duration
}

func environmentMap(environment []string) map[string]string {
	result := make(map[string]string)
	for _, entry := range environment {
		if key, value, ok := strings.Cut(entry, "="); ok {
			result[key] = value
		}
	}
	return result
}

// Environment preserves even explicitly empty values. Private readiness state
// supplied to the wrapper must never leak into an external session or Restart.
func Environment(input []string) []string {
	result := make([]string, 0, len(input)+2)
	current, session := false, false
	for _, entry := range input {
		key, _, _ := strings.Cut(entry, "=")
		if key == ReadyEnvironment {
			continue
		}
		current = current || key == "XDG_CURRENT_DESKTOP"
		session = session || key == "XDG_SESSION_TYPE"
		result = append(result, entry)
	}
	if !current {
		result = append(result, "XDG_CURRENT_DESKTOP=FVWM3:FVWM")
	}
	if !session {
		result = append(result, "XDG_SESSION_TYPE=x11")
	}
	return result
}

type reporter struct {
	output io.Writer
	count  int
}

func (r *reporter) write(format string, args ...any) {
	if r.count >= maxDiagnostics {
		return
	}
	r.count++
	message := strings.Map(func(c rune) rune {
		if unicode.IsControl(c) {
			return ' '
		}
		return c
	}, fmt.Sprintf(format, args...))
	// %q escaping is unnecessary after removing controls. Keep the entire line
	// bounded, including its stable prefix and newline.
	const prefix = "fvwm-session: "
	if len(message) > maxDiagnosticBytes-len(prefix)-1 {
		message = message[:maxDiagnosticBytes-len(prefix)-1]
	}
	fmt.Fprintln(r.output, prefix+message)
}

func waitReady(reader *os.File, timeout time.Duration) error {
	if err := reader.SetReadDeadline(time.Now().Add(timeout)); err != nil {
		return err
	}
	data, err := io.ReadAll(io.LimitReader(reader, 2))
	if err != nil {
		return fmt.Errorf("core startup acknowledgment: %w", err)
	}
	if string(data) != "R" {
		return errors.New("missing or malformed core startup acknowledgment")
	}
	return nil
}

func readinessPipe() (*os.File, *os.File, error) {
	reader, writer, err := os.Pipe()
	if err != nil {
		return nil, nil, err
	}
	// os/exec calls File.Fd, which restores blocking mode for os.Pipe files.
	// Re-wrap a duplicate already marked nonblocking: NewFile then preserves
	// that caller-selected mode when exec retrieves its descriptor.
	fd, err := unix.FcntlInt(writer.Fd(), unix.F_DUPFD_CLOEXEC, 3)
	writer.Close()
	if err == nil {
		err = unix.SetNonblock(fd, true)
	}
	if err != nil {
		reader.Close()
		if fd >= 0 {
			unix.Close(fd)
		}
		return nil, nil, err
	}
	return reader, os.NewFile(uintptr(fd), "session readiness writer"), nil
}

func exitCode(err error) int {
	if err == nil {
		return 0
	}
	var exited *exec.ExitError
	if errors.As(err, &exited) {
		if status, ok := exited.Sys().(syscall.WaitStatus); ok && status.Signaled() {
			return 128 + int(status.Signal())
		}
		if exited.ExitCode() >= 0 {
			return exited.ExitCode()
		}
	}
	return 1
}

func defaults(cfg Config) Config {
	if cfg.Diagnostics == nil {
		cfg.Diagnostics = io.Discard
	}
	if cfg.Command == nil {
		cfg.Command = exec.Command
	}
	if cfg.Exec == nil {
		cfg.Exec = syscall.Exec
	}
	if cfg.Lock == nil {
		cfg.Lock = acquireLock
	}
	if cfg.ReadyTimeout <= 0 {
		cfg.ReadyTimeout = readyTimeout
	}
	if cfg.ScanTimeout <= 0 {
		cfg.ScanTimeout = scanTimeout
	}
	cfg.Environment = Environment(cfg.Environment)
	return cfg
}

type scanResult struct {
	scan xdgmenu.AutostartScan
	err  error
}
type appExit struct {
	name string
	err  error
}

// Run returns the direct WM's exit status. There is no WM retry or replacement
// adoption. On logout it neither waits indefinitely for nor signals apps.
func Run(input Config) int {
	cfg := defaults(input)
	report := reporter{output: cfg.Diagnostics}
	plainExec := func() int {
		if err := cfg.Exec(cfg.WMPath, append([]string{cfg.WMPath}, cfg.Args...), cfg.Environment); err != nil {
			report.write("WM exec failed: %v", err)
			return 1
		}
		return 0 // A real successful Exec never returns.
	}
	if environmentMap(cfg.Environment)["SESSION_MANAGER"] != "" {
		return plainExec()
	}
	inheritedDisplay := environmentMap(cfg.Environment)["DISPLAY"]
	display, err := effectiveDisplay(cfg.Args, inheritedDisplay)
	if err != nil {
		report.write("autostart unavailable: %v", err)
		return plainExec()
	}
	if display != inheritedDisplay {
		cfg.Environment = displayEnvironment(cfg.Environment, display)
	}
	lock, err := cfg.Lock(cfg.Environment)
	if err != nil {
		report.write("autostart unavailable: session owner lock: %v", err)
		return plainExec()
	}
	defer lock.Close()

	wm := cfg.Command(cfg.WMPath, cfg.Args...)
	wm.Env, wm.Stdin, wm.Stdout, wm.Stderr = cfg.Environment, os.Stdin, os.Stdout, os.Stderr
	// A separate process group prevents a terminal's broadcast signal from
	// hitting independent applications. Forward only to the direct owned WM.
	wm.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	var reader, writer *os.File
	if !cfg.NoAutostart {
		reader, writer, err = readinessPipe()
		if err != nil {
			if reader != nil {
				reader.Close()
				writer.Close()
			}
			report.write("autostart unavailable: readiness pipe: %v", err)
			cfg.NoAutostart = true
		} else {
			defer reader.Close()
			wm.ExtraFiles = []*os.File{writer}
			wm.Env = append(append([]string(nil), cfg.Environment...), ReadyEnvironment+"=3")
		}
	}
	if err = wm.Start(); err != nil {
		if writer != nil {
			writer.Close()
		}
		report.write("WM start failed: %v", err)
		return 1
	}
	if writer != nil {
		writer.Close()
	}
	report.write("core started pid=%d", wm.Process.Pid)
	wmDone := make(chan error, 1)
	go func() { wmDone <- wm.Wait() }()
	var ready <-chan error
	if !cfg.NoAutostart {
		result := make(chan error, 1)
		ready = result
		go func() { result <- waitReady(reader, cfg.ReadyTimeout) }()
	}
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	var scanning <-chan scanResult
	apps := make(chan appExit, xdgmenu.MaxAutostartEntries)
	shuttingDown := false
	forward := func(sig os.Signal) {
		if sig == nil {
			return
		}
		shuttingDown = true
		ready = nil
		cancel()
		if err := wm.Process.Signal(sig); err != nil && !errors.Is(err, os.ErrProcessDone) {
			report.write("WM signal failed: %v", err)
		}
	}
	for {
		select {
		case err = <-wmDone:
			cancel()
			if scanning != nil {
				<-scanning
			} // Cancellation requests termination; the worker waiter owns reap.
			return exitCode(err)
		case sig, open := <-cfg.Signals:
			if !open {
				cfg.Signals = nil
				continue
			}
			forward(sig)
		case err = <-ready:
			ready = nil // At most one scan, including after a same-PID WM exec.
			if err != nil {
				report.write("autostart unavailable: %v", err)
				continue
			}
			report.write("core startup acknowledged")
			result := make(chan scanResult, 1)
			scanning = result
			go func() { scan, err := scanWorker(ctx, cfg); result <- scanResult{scan, err} }()
		case result := <-scanning:
			scanning = nil
			if shuttingDown {
				continue
			}
			if result.err != nil {
				report.write("autostart unavailable: %v", result.err)
				continue
			}
			for _, diagnostic := range result.scan.Diagnostics {
				report.write("skipped: %s", diagnostic)
			}
		launches:
			for _, launch := range result.scan.Launches {
				select {
				case err := <-wmDone:
					cancel()
					return exitCode(err)
				case sig, open := <-cfg.Signals:
					if !open {
						cfg.Signals = nil
					} else {
						forward(sig)
					}
					if shuttingDown {
						break launches
					}
				default:
				}
				app := cfg.Command(launch.Argv[0], launch.Argv[1:]...)
				app.Env, app.Dir = cfg.Environment, launch.Directory
				app.Stdin, app.Stdout, app.Stderr = nil, os.Stdout, os.Stderr
				app.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
				if err := app.Start(); err != nil {
					report.write("failed: %s: %v", launch.Filename, err)
					continue
				}
				report.write("started: %s", launch.Filename)
				// The scan cap bounds both waiter goroutines and result records to
				// 256 for this process's lifetime; no ongoing producer can add more.
				go func(name string, command *exec.Cmd) { apps <- appExit{name, command.Wait()} }(launch.Filename, app)
			}
		case result := <-apps:
			report.write("exited: %s: status %d", result.name, exitCode(result.err))
		}
	}
}

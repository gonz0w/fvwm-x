// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package sessionowner retains direct WM, helper and manual-locker observations.
// Applications remain independent. A process Start never establishes readiness,
// and an admitted locker is never terminated by session lifecycle cleanup.
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

	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"golang.org/x/sys/unix"
)

const (
	ReadyEnvironment   = "FVWM_SESSION_READY_FD"
	LeaseEnvironment   = "FVWM_SESSION_LEASE_FD"
	WorkerArgument     = "--_session-scan"
	readyTimeout       = 15 * time.Second
	scanTimeout        = 5 * time.Second
	stopGrace          = 5 * time.Second
	stopWait           = time.Second
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
	StopGrace, StopWait       time.Duration
	BusHelperPath             string
	DisableServices           bool
	serviceSlot               chan struct{}
	readyPipe                 func() (*os.File, *os.File, error)
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

// Environment preserves even explicitly empty values. Private startup state
// supplied to the wrapper must never leak into an external session or apps.
func Environment(input []string) []string {
	result := make([]string, 0, len(input)+2)
	current, session := false, false
	for _, entry := range input {
		key, _, _ := strings.Cut(entry, "=")
		if key == ReadyEnvironment || key == LeaseEnvironment || key == sessionservice.SocketEnvironment || key == sessionservice.CapabilityEnvironment {
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
	final  bool
}

func (r *reporter) write(format string, args ...any) {
	if r.count >= maxDiagnostics-1 {
		return
	}
	r.count++
	r.line(format, args...)
}

// Reserve one of the 64 lines for a terminal result even if application or
// optional-service observations exhausted the ordinary diagnostic budget.
func (r *reporter) terminal(format string, args ...any) {
	if r.final {
		return
	}
	r.final = true
	r.line(format, args...)
}

func (r *reporter) line(format string, args ...any) {
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

func waitReady(reader *os.File, timeout time.Duration, lease bool) error {
	if err := reader.SetReadDeadline(time.Now().Add(timeout)); err != nil {
		return err
	}
	expected := "R"
	if lease {
		expected = "RL"
	}
	data, err := io.ReadAll(io.LimitReader(reader, int64(len(expected)+1)))
	if err != nil {
		return fmt.Errorf("core startup acknowledgment: %w", err)
	}
	if string(data) != expected {
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
	if cfg.serviceSlot == nil {
		cfg.serviceSlot = make(chan struct{}, 1)
	}
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
	if cfg.readyPipe == nil {
		cfg.readyPipe = readinessPipe
	}
	if cfg.ReadyTimeout <= 0 {
		cfg.ReadyTimeout = readyTimeout
	}
	if cfg.ScanTimeout <= 0 {
		cfg.ScanTimeout = scanTimeout
	}
	if cfg.StopGrace <= 0 {
		cfg.StopGrace = stopGrace
	}
	if cfg.StopWait <= 0 {
		cfg.StopWait = stopWait
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

// Only receipt of the owner's actual Wait result confirms termination. A
// closed empty channel is not a successful Wait, nor is expiration a reap.
func awaitWMExit(done <-chan error, timeout time.Duration) (error, bool) {
	timer := time.NewTimer(timeout)
	defer timer.Stop()
	select {
	case err, received := <-done:
		return err, received
	case <-timer.C:
		// Prefer an already observed result at the deadline boundary.
		select {
		case err, received := <-done:
			return err, received
		default:
			return nil, false
		}
	}
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
	// Close only this copy. An explicit LOCK_UN would release the description
	// shared with a live core, defeating parent-loss recovery.
	defer lock.Close()
	lease, hasLease := lock.(*os.File)
	wmTerminationUnconfirmed := false
	var services *serviceState
	var controls <-chan controlRequest
	var busDone <-chan error
	ctx, cancel := context.WithCancel(context.Background())
	var scanning <-chan scanResult
	scanReapUnconfirmed := false
	var jobDone <-chan serviceJob
	var cancelServiceJob context.CancelFunc
	if !cfg.DisableServices {
		services, err = newServices(&cfg, &report)
		if err != nil {
			report.write("session services unavailable: %v", err)
		} else {
			if services.bus != nil {
				defer func() {
					if !wmTerminationUnconfirmed {
						services.bus.stop()
					}
				}()
				busDone = services.bus.done
			}
			control, controlErr := startControl(cfg.Environment)
			if controlErr != nil {
				report.write("session control unavailable: %v", controlErr)
			} else {
				defer control.close()
				controls = control.requests
				cfg.Environment = append(cfg.Environment, sessionservice.SocketEnvironment+"="+control.path, sessionservice.CapabilityEnvironment+"="+control.capability)
			}
		}
	}
	defer func() {
		if cancelServiceJob != nil {
			cancelServiceJob()
		}
		finishScanWork(cancel, scanning, &report)
		if scanReapUnconfirmed {
			report.terminal("%v", errScanReapUnconfirmed)
		}
		finishServiceWork(cfg, cancel, jobDone, &report)
	}()
	setState := func(index int, value, message string) {
		if services != nil {
			services.snapshot.Items[index].State = value
			services.snapshot.Items[index].Detail = detail(message)
		}
	}

	wm := cfg.Command(cfg.WMPath, cfg.Args...)
	wm.Env, wm.Stdin, wm.Stdout, wm.Stderr = cfg.Environment, os.Stdin, os.Stdout, os.Stderr
	// A separate process group prevents a terminal's broadcast signal from
	// hitting independent applications. Forward only to the direct owned WM.
	wm.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	var reader, writer *os.File
	wantReady := hasLease || !cfg.NoAutostart || services != nil
	if wantReady {
		reader, writer, err = cfg.readyPipe()
		if err != nil {
			if reader != nil {
				reader.Close()
			}
			if writer != nil {
				writer.Close()
			}
			report.write("autostart unavailable: readiness pipe: %v", err)
			if hasLease {
				// Preserve login under pipe/resource failure. Reserve FD3 as
				// closed so the still-shared lease remains FD4, without claiming
				// an acknowledged handoff or starting one-shot applications.
				wm.ExtraFiles = []*os.File{nil, lease}
				wm.Env = append(append([]string(nil), cfg.Environment...), LeaseEnvironment+"=4")
			}
			if !cfg.NoAutostart {
				setState(1, "unavailable", "startup acknowledgment pipe is unavailable")
				setState(4, "unavailable", "autostart disabled after startup pipe failure")
			}
			cfg.NoAutostart = true
			wantReady = false
			setState(0, "unavailable", "startup acknowledgment pipe is unavailable")
		} else {
			defer reader.Close()
			wm.ExtraFiles = []*os.File{writer}
			wm.Env = append(append([]string(nil), cfg.Environment...), ReadyEnvironment+"=3")
			if hasLease {
				wm.ExtraFiles = append(wm.ExtraFiles, lease)
				wm.Env = append(wm.Env, LeaseEnvironment+"=4")
			}
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
	if wantReady {
		result := make(chan error, 1)
		ready = result
		go func() { result <- waitReady(reader, cfg.ReadyTimeout, hasLease) }()
	}
	apps := make(chan appExit, xdgmenu.MaxAutostartEntries)
	shuttingDown := false
	var graceTimer *time.Timer
	var graceDone <-chan time.Time
	defer func() {
		if graceTimer != nil {
			graceTimer.Stop()
		}
	}()
	forward := func(sig os.Signal) {
		if sig == nil || shuttingDown {
			return
		}
		shuttingDown = true
		ready = nil
		cancel()
		if reader != nil {
			reader.Close()
		}
		graceTimer = time.NewTimer(cfg.StopGrace)
		graceDone = graceTimer.C
		report.write("WM stop requested; cooperative grace started")
		if err := wm.Process.Signal(sig); err != nil && !errors.Is(err, os.ErrProcessDone) {
			report.write("WM signal failed: %v", err)
		}
	}
	pendingProviderProbe := false
	startJob := func(request controlRequest) {
		result := make(chan serviceJob, 1)
		jobDone = result
		jobCtx, jobCancel := context.WithCancel(ctx)
		stopDisconnect := context.AfterFunc(request.ctx, jobCancel)
		cancelServiceJob = func() { stopDisconnect(); jobCancel() }
		request.ctx = jobCtx
		identity, count := services.identity, len(services.ids)
		power, plans := services.snapshot.Power, services.plans
		go func() {
			if request.message.Verb == "power-probe" || sessionservice.IsPowerVerb(request.message.Verb) || request.message.Verb == "lock" {
				result <- runProviderJob(request, cfg, power, plans)
			} else {
				result <- runServiceJob(request, cfg, identity, count)
			}
		}()
	}
	for {
		if pendingProviderProbe && jobDone == nil && !shuttingDown {
			pendingProviderProbe = false
			request := controlRequest{ctx: ctx, message: sessionservice.RequestMessage{Version: sessionservice.Version, Verb: "power-probe", Generation: services.snapshot.Generation, Operation: services.snapshot.Operation}, reply: make(chan sessionservice.Reply, 1)}
			if start, _ := services.admit(request, cfg, false); start {
				startJob(request)
			}
		}
		var lockerDone <-chan error
		if services != nil && services.locker != nil {
			lockerDone = services.locker.done
		}
		select {
		case lockerErr := <-lockerDone:
			services.locker = nil
			services.snapshot.Locker.State = "exited"
			services.snapshot.Locker.Detail = detail(fmt.Sprintf("observed locker exited with status %d; secure readiness remains unconfirmed", exitCode(lockerErr)))
		case request, open := <-controls:
			if !open {
				controls = nil
				continue
			}
			if services == nil {
				continue
			}
			if shuttingDown {
				request.reply <- services.reply("unavailable", "session is stopping", 0)
				continue
			}
			start, reply := services.admit(request, cfg, jobDone != nil)
			if !start {
				request.reply <- reply
				continue
			}
			startJob(request)
		case job := <-jobDone:
			jobDone = nil
			cancelServiceJob()
			cancelServiceJob = nil
			job.request.reply <- services.complete(job)
		case err := <-busDone:
			busDone = nil
			services.bus.reaped = true
			setState(2, "failed", fmt.Sprintf("owned session bus exited with status %d; no automatic replacement", exitCode(err)))
			setState(3, "unavailable", "session bus is unavailable")
			services.ids = make(map[uint32]bool)
		case err = <-wmDone:
			cancel()
			setState(0, "stopped", "owned core exited")
			// The bounded final acquisition barrier observes worker cleanup.
			return exitCode(err)
		case <-graceDone:
			graceDone = nil
			report.write("WM stop grace expired; termination requested")
			// Process.Kill addresses the same directly owned handle as Signal;
			// no PID lookup or process-group broadcast can reach applications.
			if killErr := wm.Process.Kill(); killErr != nil && !errors.Is(killErr, os.ErrProcessDone) {
				report.write("WM termination request failed: %v", killErr)
			}
			if waitErr, confirmed := awaitWMExit(wmDone, cfg.StopWait); confirmed {
				setState(0, "stopped", "owned core exit observed after termination request")
				return exitCode(waitErr)
			}
			wmTerminationUnconfirmed = true
			report.terminal("WM termination unconfirmed after stop deadline; owned session bus preserved if present")
			return 1
		case sig, open := <-cfg.Signals:
			if !open {
				cfg.Signals = nil
				continue
			}
			forward(sig)
		case err = <-ready:
			ready = nil // At most one scan, including after a same-PID WM exec.
			if err != nil {
				setState(0, "unavailable", err.Error())
				if !cfg.NoAutostart {
					setState(1, "unavailable", err.Error())
					setState(4, "unavailable", "autostart disabled without core startup acknowledgment")
				}
				report.write("autostart unavailable: %v", err)
				continue
			}
			setState(0, "ready", "core startup acknowledged; modules/apps have separate readiness")
			report.write("core startup acknowledged")
			if services != nil {
				pendingProviderProbe = true
			}
			if cfg.NoAutostart {
				continue
			}
			setState(1, "starting", "bounded autostart acquisition")
			if services != nil {
				services.snapshot.Items[1].Attempts++
			}
			result := make(chan scanResult, 1)
			scanning = result
			report.write("autostart scan started")
			go func() { scan, err := scanWorker(ctx, cfg); result <- scanResult{scan, err} }()
		case result := <-scanning:
			scanning = nil
			scanReapUnconfirmed = errors.Is(result.err, errScanReapUnconfirmed)
			if shuttingDown {
				continue
			}
			if result.err != nil {
				setState(1, "failed", result.err.Error())
				setState(4, "unavailable", "autostart acquisition failed; no applications launched")
				report.write("autostart unavailable: %v", result.err)
				continue
			}
			setState(1, "stopped", "bounded autostart acquisition completed")
			if len(result.scan.Launches) == 0 {
				setState(4, "stopped", "no autostart applications selected")
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
					if services != nil {
						services.snapshot.Items[4].Failed++
					}
					report.write("failed: %s: %v", launch.Filename, err)
					continue
				}
				if services != nil {
					services.snapshot.Items[4].Started++
				}
				setState(4, "enabled", "independent application launch observations; no readiness/termination ownership")
				report.write("started: %s", launch.Filename)
				// The scan cap bounds both waiter goroutines and result records to
				// 256 for this process's lifetime; no ongoing producer can add more.
				go func(name string, command *exec.Cmd) { apps <- appExit{name, command.Wait()} }(launch.Filename, app)
			}
		case result := <-apps:
			if services != nil {
				services.snapshot.Items[4].Exited++
				if services.snapshot.Items[4].Exited == services.snapshot.Items[4].Started {
					setState(4, "stopped", "all observed independent application children exited")
				}
			}
			report.write("exited: %s: status %d", result.name, exitCode(result.err))
		}
	}
}

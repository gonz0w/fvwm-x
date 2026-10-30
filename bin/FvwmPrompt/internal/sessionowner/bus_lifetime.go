// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
	"golang.org/x/sys/unix"
)

type daemonReceiptKind uint8

const (
	daemonReceiptAbsent daemonReceiptKind = iota
	daemonReceiptWaited
	daemonReceiptUnconfirmed
	daemonReceiptInvalid
)

type daemonReceipt struct {
	kind daemonReceiptKind
	exit int
	err  error
}

type busCleanup struct {
	receipt              daemonReceipt
	guardianWaitObserved bool
	guardianWaitError    error
}

func (facts busCleanup) description() string {
	daemon := "daemon Wait unconfirmed"
	switch facts.receipt.kind {
	case daemonReceiptWaited:
		daemon = fmt.Sprintf("daemon Wait confirmed status=%d", facts.receipt.exit)
	case daemonReceiptUnconfirmed:
		daemon = "daemon Wait unconfirmed (guardian U receipt)"
	case daemonReceiptInvalid:
		daemon = fmt.Sprintf("daemon Wait unconfirmed (invalid guardian receipt: %v)", facts.receipt.err)
	}
	if facts.guardianWaitObserved {
		return fmt.Sprintf("%s; guardian Wait observed status=%d", daemon, exitCode(facts.guardianWaitError))
	}
	return daemon + "; guardian Wait unconfirmed; outstanding actual owner preserved"
}

// The guardian owns the daemon, while this parent owns only its own lifetime
// writer and the guardian's actual Wait. Neither a proxy exit nor a timeout is
// evidence of daemon reaping. terminal fences adapters before complete receipt
// validation, including when an untrusted guardian leaves stdout open.
type ownedBus struct {
	command          *exec.Cmd
	lifetime         *os.File
	reader           *os.File
	terminal         chan struct{}
	receiptDone      chan struct{}
	guardianDone     chan struct{}
	endOnce          sync.Once
	releaseOnce      sync.Once
	cleanupOnce      sync.Once
	mu               sync.Mutex
	facts            busCleanup
	lifetimeReleased bool
}

func (bus *ownedBus) end() { bus.endOnce.Do(func() { close(bus.terminal) }) }

func (bus *ownedBus) ended() bool {
	if bus == nil {
		return false
	}
	select {
	case <-bus.terminal:
		return true
	default:
		return false
	}
}

// Handoff eligibility is a separate ownership fact from terminal receipt/Wait
// observation. Failure cleanup can release this writer before the receipt
// goroutine has finished; a closed file must never become a WM ExtraFile.
func (bus *ownedBus) lifetimeForHandoff() *os.File {
	if bus == nil {
		return nil
	}
	bus.mu.Lock()
	defer bus.mu.Unlock()
	if bus.lifetimeReleased || bus.ended() {
		return nil
	}
	return bus.lifetime
}

func (bus *ownedBus) observations() busCleanup {
	bus.mu.Lock()
	defer bus.mu.Unlock()
	return bus.facts
}

// The deadline bounds observation, not a kernel-blocked call. Never signal the
// guardian: after U it may still be the only actual owner able to reap a child.
func (bus *ownedBus) stop() busCleanup {
	return bus.stopWithin(4 * time.Second)
}

func (bus *ownedBus) stopWithin(timeout time.Duration) busCleanup {
	if bus == nil {
		return busCleanup{}
	}
	bus.cleanupOnce.Do(func() {
		bus.releaseOnce.Do(func() {
			bus.mu.Lock()
			bus.lifetimeReleased = true
			bus.mu.Unlock()
			bus.lifetime.Close()
		})
		timer := time.NewTimer(timeout)
		defer timer.Stop()
		receipt, guardian := bus.receiptDone, bus.guardianDone
		for receipt != nil || guardian != nil {
			select {
			case <-receipt:
				receipt = nil
			case <-guardian:
				guardian = nil
			case <-timer.C:
				// Closing only our reader retires an outstanding read goroutine;
				// the real Wait owner remains intact, without a signal or retry.
				bus.reader.Close()
				return
			}
		}
	})
	return bus.observations()
}

// File.Fd resets the runtime's os.Pipe nonblocking mode. As for readiness,
// duplicate and re-wrap a caller-selected nonblocking descriptor before exec.
func stableNonblocking(file *os.File, name string) (*os.File, error) {
	fd, err := unix.FcntlInt(file.Fd(), unix.F_DUPFD_CLOEXEC, 3)
	file.Close()
	if err == nil {
		err = unix.SetNonblock(fd, true)
	}
	if err != nil {
		if fd >= 0 {
			unix.Close(fd)
		}
		return nil, err
	}
	return os.NewFile(uintptr(fd), name), nil
}

func busLifetimePipe() (*os.File, *os.File, error) {
	reader, writer, err := os.Pipe()
	if err != nil {
		return nil, nil, err
	}
	reader, err = stableNonblocking(reader, "session bus lifetime reader")
	if err != nil {
		writer.Close()
		return nil, nil, err
	}
	writer, err = stableNonblocking(writer, "session bus lifetime writer")
	if err != nil {
		reader.Close()
		return nil, nil, err
	}
	return reader, writer, nil
}

func parseDaemonReceipt(line string) (daemonReceipt, bool) {
	if line == "U" {
		return daemonReceipt{kind: daemonReceiptUnconfirmed}, true
	}
	if !strings.HasPrefix(line, "W ") {
		return daemonReceipt{}, false
	}
	number := strings.TrimPrefix(line, "W ")
	code, err := strconv.Atoi(number)
	if err != nil || code < 0 || code > 255 || strconv.Itoa(code) != number {
		return daemonReceipt{}, false
	}
	return daemonReceipt{kind: daemonReceiptWaited, exit: code}, true
}

func readBusLine(reader io.Reader, max int) (string, error) {
	data := make([]byte, 0, max)
	var one [1]byte
	for {
		n, err := reader.Read(one[:])
		if n != 0 {
			if one[0] == '\n' {
				return string(data), nil
			}
			if len(data) == max {
				return "", errors.New("guardian record exceeds limit")
			}
			data = append(data, one[0])
		}
		if err != nil {
			return "", err
		}
	}
}

type busAddress struct {
	value string
	err   error
}

func validBusAddress(value string) bool {
	return value != "" && sessionservice.Plain(value, 4096) && strings.HasPrefix(value, "unix:") && !strings.Contains(value, ";")
}

func (bus *ownedBus) readReceipts(address chan<- busAddress) {
	defer close(bus.receiptDone)
	defer bus.reader.Close()
	receipt := daemonReceipt{kind: daemonReceiptInvalid, err: errors.New("missing guardian terminal receipt")}
	defer func() {
		bus.mu.Lock()
		bus.facts.receipt = receipt
		bus.mu.Unlock()
		bus.end()
	}()
	line, err := readBusLine(bus.reader, 4096)
	if err != nil {
		receipt.err = err
		address <- busAddress{err: err}
		return
	}
	terminal, isTerminal := parseDaemonReceipt(line)
	if !isTerminal {
		if !validBusAddress(line) {
			receipt.err = errors.New("malformed guardian bus address")
			address <- busAddress{err: receipt.err}
			return
		}
		// The address deadline must not expire later while waiting for an idle
		// live bus. Keep the explicit stdout reader until terminal validation.
		if err := bus.reader.SetReadDeadline(time.Time{}); err != nil {
			receipt.err = err
			address <- busAddress{err: err}
			return
		}
		address <- busAddress{value: line}
		line, err = readBusLine(bus.reader, 5)
		if err != nil {
			receipt.err = err
			return
		}
		terminal, isTerminal = parseDaemonReceipt(line)
	} else {
		address <- busAddress{err: errors.New("guardian ended before bus address")}
	}
	bus.end()
	if !isTerminal {
		receipt.err = errors.New("malformed guardian terminal receipt")
		return
	}
	var extra [1]byte
	n, err := bus.reader.Read(extra[:])
	if n != 0 || err != io.EOF {
		receipt.err = errors.New("extra or incomplete guardian output")
		return
	}
	receipt = terminal
}

func startBus(ctx context.Context, cfg Config) (*ownedBus, string, error) {
	if err := ctx.Err(); err != nil {
		return nil, "", err
	}
	if !filepath.IsAbs(cfg.BusHelperPath) || filepath.Clean(cfg.BusHelperPath) != cfg.BusHelperPath {
		return nil, "", errors.New("optional session bus helper is not installed")
	}
	path, err := daemonPath(cfg.Environment)
	if err != nil {
		return nil, "", err
	}
	lifeReader, lifeWriter, err := busLifetimePipe()
	if err != nil {
		return nil, "", err
	}
	defer lifeReader.Close()
	reader, writer, err := os.Pipe()
	if err != nil {
		lifeWriter.Close()
		return nil, "", err
	}
	defer writer.Close()
	fail := func(err error) (*ownedBus, string, error) {
		reader.Close()
		lifeWriter.Close()
		return nil, "", err
	}
	command := cfg.Command(cfg.BusHelperPath, "bus-guardian", path)
	command.Env = Environment(cfg.Environment)
	command.ExtraFiles = []*os.File{lifeReader}
	// Cmd.StdoutPipe closes its reader at Wait; an io.Writer would add a
	// copier to Wait. An explicit file preserves terminal evidence separately.
	command.Stdout = writer
	// A nil stderr gives exec a direct null-device file. io.Discard would
	// create a copier pipe, allowing an inherited daemon FD2 to delay the
	// guardian's Wait result even after the guardian itself was reaped.
	command.Stderr = nil
	command.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := ctx.Err(); err != nil {
		return fail(err)
	}
	if err := command.Start(); err != nil {
		return fail(err)
	}
	writer.Close()
	bus := &ownedBus{command: command, lifetime: lifeWriter, reader: reader, terminal: make(chan struct{}), receiptDone: make(chan struct{}), guardianDone: make(chan struct{})}
	go func() {
		err := command.Wait()
		bus.mu.Lock()
		bus.facts.guardianWaitObserved = true
		bus.facts.guardianWaitError = err
		bus.mu.Unlock()
		bus.end()
		close(bus.guardianDone)
	}()
	deadline := time.Now().Add(5 * time.Second)
	if end, ok := ctx.Deadline(); ok && end.Before(deadline) {
		deadline = end
	}
	if err := reader.SetReadDeadline(deadline); err != nil {
		bus.mu.Lock()
		bus.facts.receipt = daemonReceipt{kind: daemonReceiptInvalid, err: err}
		bus.mu.Unlock()
		close(bus.receiptDone)
		bus.end()
		reader.Close()
		bus.stop()
		return bus, "", err
	}
	address := make(chan busAddress, 1)
	go bus.readReceipts(address)
	select {
	case result := <-address:
		if result.err == nil && !bus.ended() {
			return bus, result.value, nil
		}
		bus.stop()
		if result.err != nil {
			return bus, "", result.err
		}
		return bus, "", errors.New("guardian ended during bus startup")
	case <-ctx.Done():
		bus.stop()
		return bus, "", ctx.Err()
	}
}

// Called by the event loop and before any admission/completion, so a terminal
// event cannot lose a select race to a stale adapter result. Power/locker state
// is independently owned and is deliberately untouched.
func (state *serviceState) fenceBusLoss() {
	if state.bus == nil || !state.bus.ended() || state.busLost {
		return
	}
	state.busLost = true
	state.snapshot.Items[2].State = "failed"
	state.snapshot.Items[2].Detail = "owned session bus ended or guardian ownership was lost; no automatic replacement"
	state.snapshot.Items[3].State = "unavailable"
	state.snapshot.Items[3].Detail = "session bus is unavailable"
	state.ids = make(map[uint32]bool)
}

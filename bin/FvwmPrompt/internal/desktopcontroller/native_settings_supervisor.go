// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"bufio"
	"errors"
	"io"
	"os"
	"path/filepath"
	"sync"
	"syscall"
	"time"

	"golang.org/x/sys/unix"
)

type nativeEvent struct {
	run     string
	result  *nativeResult
	failure string
}

type nativeTerminal struct {
	run    string
	reaped bool
}

// One supervisor owns the child slot and is the sole Wait4/signal owner.
// I/O workers own only pipes and immutable bounded events. The controller never
// calls Start, Wait, Write or Signal on its dispatch loop.
type nativeSupervisor struct {
	run      string
	input    chan string
	events   chan nativeEvent
	fault    chan nativeEvent
	terminal chan nativeTerminal
	stop     chan struct{}
	done     chan struct{}
	once     sync.Once
}

func startNativeSupervisor(path, run string) *nativeSupervisor {
	s := &nativeSupervisor{run: run, input: make(chan string, 1),
		events: make(chan nativeEvent, 16), fault: make(chan nativeEvent, 1), terminal: make(chan nativeTerminal, 1),
		stop: make(chan struct{}), done: make(chan struct{})}
	go s.supervise(path)
	return s
}
func (s *nativeSupervisor) close() { s.once.Do(func() { close(s.stop) }) }
func (s *nativeSupervisor) event(event nativeEvent) {
	event.run = s.run
	select {
	case s.fault <- event:
	default:
	}
}

func (s *nativeSupervisor) supervise(path string) {
	defer close(s.done)
	// A failed launch has no process to reap and can release the slot.
	reaped := true
	defer func() { s.terminal <- nativeTerminal{s.run, reaped} }()
	if !filepath.IsAbs(path) {
		s.event(nativeEvent{failure: "helper-unavailable"})
		return
	}
	inR, inW, err := os.Pipe()
	if err != nil {
		s.event(nativeEvent{failure: "helper-unavailable"})
		return
	}
	defer inR.Close()
	defer inW.Close()
	outR, outW, err := os.Pipe()
	if err != nil {
		s.event(nativeEvent{failure: "helper-unavailable"})
		return
	}
	defer outR.Close()
	defer outW.Close()
	null, err := os.OpenFile(os.DevNull, os.O_RDWR, 0)
	if err != nil {
		s.event(nativeEvent{failure: "helper-unavailable"})
		return
	}
	defer null.Close()
	select {
	case <-s.stop:
		return
	default:
	}
	process, err := os.StartProcess(path, []string{path}, &os.ProcAttr{
		Env: os.Environ(), Files: []*os.File{inR, outW, null},
		Sys: &syscall.SysProcAttr{Setpgid: true}})
	if err != nil {
		s.event(nativeEvent{failure: "helper-unavailable"})
		return
	}
	defer process.Release()
	reaped = false
	inR.Close()
	outW.Close()
	ioDone := make(chan struct{})
	ioFailure := make(chan error, 2)
	readerDone := make(chan struct{})
	var workers sync.WaitGroup
	workers.Add(2)
	go func() {
		defer workers.Done()
		defer close(readerDone)
		reader := bufio.NewReaderSize(outR, nativeFrameLimit)
		for {
			line, err := reader.ReadSlice('\n')
			if err != nil {
				select {
				case ioFailure <- err:
				case <-ioDone:
				}
				return
			}
			result, err := nativeResultLine(string(line))
			if err != nil {
				select {
				case ioFailure <- err:
				case <-ioDone:
				}
				return
			}
			event := nativeEvent{run: s.run, result: &result}
			select {
			case s.events <- event:
			case <-ioDone:
				return
			}
		}
	}()
	go func() {
		defer workers.Done()
		for {
			select {
			case line := <-s.input:
				if _, err := io.WriteString(inW, line); err != nil {
					select {
					case ioFailure <- err:
					case <-ioDone:
					}
					return
				}
			case <-ioDone:
				return
			}
		}
	}()
	// Always unblock owned readers/writers and join them before terminal reap
	// delivery. No child descriptor or worker survives a released slot.
	defer func() {
		inW.Close()
		outR.Close()
		close(ioDone)
		workers.Wait()
	}()
	ticker := time.NewTicker(250 * time.Millisecond)
	defer ticker.Stop()
	stopping := false
	var stopped time.Time
	stage := 0
	stop := s.stop
	failures := ioFailure
	for {
		var status unix.WaitStatus
		pid, waitErr := unix.Wait4(process.Pid, &status, unix.WNOHANG, nil)
		if pid == process.Pid {
			reaped = true
			// Wait can win the race with the final RESULT/EOF reader. Allow
			// one bounded controller polling interval to retain cleanup/loss
			// observations before joining descriptors and publishing terminal.
			select {
			case <-readerDone:
			case <-time.After(500 * time.Millisecond):
			}
			return
		}
		if waitErr != nil && !errors.Is(waitErr, unix.EINTR) {
			// ECHILD is not confirmed retirement. Never reuse the slot when
			// some other reaper or an inherited disposition hid actual Wait.
			s.event(nativeEvent{failure: "stop-unknown"})
			return
		}
		select {
		case <-stop:
			stop = nil
			if !stopping {
				stopping = true
				ticker.Reset(10 * time.Millisecond)
				stopped = time.Now()
				inW.Close()
			}
		case <-failures:
			failures = nil
			if !stopping {
				s.event(nativeEvent{failure: "helper-io"})
				stopping = true
				ticker.Reset(10 * time.Millisecond)
				stopped = time.Now()
				inW.Close()
			}
		case now := <-ticker.C:
			if stopping {
				elapsed := now.Sub(stopped)
				if stage == 0 && elapsed >= 500*time.Millisecond {
					unix.Kill(process.Pid, unix.SIGTERM)
					stage = 1
				} else if stage == 1 && elapsed >= time.Second {
					unix.Kill(process.Pid, unix.SIGKILL)
					stage = 2
				} else if stage == 2 && elapsed >= 1500*time.Millisecond {
					s.event(nativeEvent{failure: "stop-unknown"})
					stage = 3
				}
			}
		}
	}
}

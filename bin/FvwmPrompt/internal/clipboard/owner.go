// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package clipboard

import (
	"bufio"
	"context"
	"errors"
	"io"
	"os"
	"sync"
	"syscall"
	"time"
)

type call struct {
	ctx     context.Context
	request Request
	reply   chan callResult
}
type callResult struct {
	reply Reply
	err   error
}
type Terminal struct {
	Started, Reaped, Clean bool
}

// Client owns one child for its entire session. It is never restarted or its
// slot reused, including after uncertain Wait. Only supervise performs I/O,
// signals and process Wait. The reader has one bounded metadata mailbox.
type Client struct {
	epoch    uint64
	calls    chan call
	stop     chan struct{}
	done     chan struct{}
	once     sync.Once
	terminal Terminal // published only by closing done
}

func Start(config Config, environment []string) (*Client, error) {
	if err := config.Validate(); err != nil {
		return nil, err
	}
	client := &Client{epoch: config.Epoch, calls: make(chan call, 1), stop: make(chan struct{}), done: make(chan struct{})}
	go client.supervise(config, append([]string(nil), environment...))
	return client, nil
}

func (c *Client) Close()                { c.once.Do(func() { close(c.stop) }) }
func (c *Client) Done() <-chan struct{} { return c.done }
func (c *Client) Wait(ctx context.Context) (Terminal, error) {
	select {
	case <-c.done:
		return c.terminal, nil
	case <-ctx.Done():
		return Terminal{}, ctx.Err()
	}
}

func (c *Client) Call(ctx context.Context, request Request) (Reply, error) {
	if err := request.Validate(c.epoch); err != nil {
		return Reply{}, err
	}
	if err := ctx.Err(); err != nil {
		return Reply{}, err
	}
	result := make(chan callResult, 1)
	select {
	case c.calls <- call{ctx, request, result}:
	case <-c.done:
		return Reply{}, errors.New("clipboard owner ended")
	case <-ctx.Done():
		return Reply{}, ctx.Err()
	default:
		return Reply{}, errors.New("clipboard control is busy")
	}
	select {
	case value := <-result:
		return value.reply, value.err
	case <-ctx.Done():
		// A submitted control may already be on the wire. Never accept a late
		// pause response as a newer barrier or continue with uncertain state.
		c.Close()
		return Reply{}, ctx.Err()
	case <-c.done:
		return Reply{}, errors.New("clipboard owner ended")
	}
}

func (c *Client) supervise(config Config, environment []string) {
	defer close(c.done)
	inR, inW, err := os.Pipe()
	if err != nil {
		return
	}
	defer inR.Close()
	defer inW.Close()
	outR, outW, err := os.Pipe()
	if err != nil {
		return
	}
	defer outR.Close()
	defer outW.Close()
	null, err := os.OpenFile(os.DevNull, os.O_RDWR, 0)
	if err != nil {
		return
	}
	defer null.Close()
	select {
	case <-c.stop:
		return
	default:
	}
	process, err := os.StartProcess(config.Path, config.arguments(), &os.ProcAttr{Env: environment, Files: []*os.File{inR, outW, null}, Sys: &syscall.SysProcAttr{Setpgid: true}})
	if err != nil {
		return
	}
	c.terminal.Started = true
	inR.Close()
	outW.Close()
	type waited struct {
		state *os.ProcessState
		err   error
	}
	waitedChild := make(chan waited, 1)
	go func() { state, err := process.Wait(); waitedChild <- waited{state, err} }()
	frames := make(chan callResult, 1)
	readerDone := make(chan struct{})
	readerStop := make(chan struct{})
	go func() {
		defer close(readerDone)
		reader := bufio.NewReaderSize(outR, replyLimit)
		for {
			line, err := reader.ReadSlice('\n')
			value := callResult{err: err}
			if err == nil {
				value.reply, value.err = parseReply(line)
			}
			select {
			case frames <- value:
			case <-readerStop:
				return
			}
			if value.err != nil {
				return
			}
		}
	}()
	defer func() { close(readerStop); outR.Close(); <-readerDone }()
	var active *call
	var sequence uint64
	var response <-chan time.Time
	var responseTimer *time.Timer
	defer func() {
		if responseTimer != nil {
			responseTimer.Stop()
		}
	}()
	stop := c.stop
	var stopTimer *time.Timer
	var stopTick <-chan time.Time
	stopping, forced, badIO := false, false, false
	stage := 0
	beginStop := func() {
		if stopping {
			return
		}
		stopping = true
		stop = nil
		inW.Close() // EOF asks native owner to wipe and drain first.
		stopTimer = time.NewTimer(2500 * time.Millisecond)
		stopTick = stopTimer.C
	}
	defer func() {
		if stopTimer != nil {
			stopTimer.Stop()
		}
	}()
	failActive := func(err error) {
		if active != nil {
			active.reply <- callResult{err: err}
			active = nil
		}
		response = nil
		if responseTimer != nil {
			responseTimer.Stop()
		}
	}
	for {
		select {
		case request := <-c.calls:
			if stopping || active != nil || request.ctx.Err() != nil {
				request.reply <- callResult{err: errors.New("clipboard control unavailable")}
				continue
			}
			if sequence == ^uint64(0) {
				request.reply <- callResult{err: errors.New("clipboard sequence exhausted")}
				beginStop()
				continue
			}
			sequence++
			active = &request
			// os.Pipe's pollable write end enforces the bound even if a child
			// stops reading. A frame fits below PIPE_BUF on supported targets.
			if err := inW.SetWriteDeadline(time.Now().Add(200 * time.Millisecond)); err != nil {
				failActive(errors.New("clipboard write deadline unavailable"))
				badIO = true
				beginStop()
				continue
			}
			line := request.request.line(sequence)
			count, err := io.WriteString(inW, line)
			if err != nil || count != len(line) {
				failActive(errors.New("clipboard control write failed"))
				badIO = true
				beginStop()
				continue
			}
			responseTimer = time.NewTimer(1500 * time.Millisecond)
			response = responseTimer.C
		case frame := <-frames:
			if frame.err != nil {
				if !stopping || !errors.Is(frame.err, io.EOF) {
					badIO = true
				}
				failActive(errors.New("clipboard metadata stream ended"))
				beginStop()
				continue
			}
			if active == nil || frame.reply.Sequence != sequence || frame.reply.Ref.Epoch != c.epoch {
				badIO = true
				failActive(errors.New("clipboard response identity mismatch"))
				beginStop()
				continue
			}
			active.reply <- frame
			active = nil
			response = nil
			responseTimer.Stop()
			if frame.reply.Result == Fatal {
				badIO = true
				beginStop()
			}
		case <-response:
			badIO = true
			failActive(errors.New("clipboard response deadline expired"))
			beginStop()
		case <-stop:
			failActive(errors.New("clipboard owner stopping"))
			beginStop()
		case <-stopTick:
			forced = true
			if stage == 0 {
				process.Signal(syscall.SIGTERM)
				stage++
				stopTimer.Reset(500 * time.Millisecond)
			} else if stage == 1 {
				process.Kill()
				stage++
				stopTimer.Reset(time.Second)
			} else {
				// Wait remains owned by its goroutine; this terminal cannot
				// claim reap/clean or grant another child slot.
				failActive(errors.New("clipboard reap unconfirmed"))
				return
			}
		case result := <-waitedChild:
			failActive(errors.New("clipboard child exited"))
			c.terminal.Reaped = result.err == nil && result.state != nil
			c.terminal.Clean = c.terminal.Reaped && result.state.Success() && stopping && !forced && !badIO
			return
		}
	}
}

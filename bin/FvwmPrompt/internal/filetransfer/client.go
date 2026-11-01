// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filetransfer

import (
	"bufio"
	"encoding/json"
	"errors"
	"io"
	"os"
	"os/exec"
	"strconv"
	"sync"
	"sync/atomic"
	"time"
)

// Client has one browser owner. Send is nonblocking; queue overflow retires the
// helper rather than dropping a cancel/release or admitting ambiguous work.
// Done closes only after actual Wait and pipe goroutines have retired.
type Client struct {
	Events      <-chan Event
	Done        <-chan struct{}
	events      chan Event
	done        chan struct{}
	input       *os.File
	output      *os.File
	writes      chan []byte
	stop        chan struct{}
	once        sync.Once
	cmd         *exec.Cmd
	next        uint64
	queuedBytes atomic.Int64
}

func Start(path, socket, token string) (*Client, error) {
	inputRead, input, err := os.Pipe()
	if err != nil {
		return nil, err
	}
	output, outputWrite, err := os.Pipe()
	if err != nil {
		inputRead.Close()
		input.Close()
		return nil, err
	}
	cmd := exec.Command(path, "--form-socket", socket, "--token", token)
	cmd.Stdin, cmd.Stdout, cmd.Stderr = inputRead, outputWrite, io.Discard
	err = cmd.Start()
	inputRead.Close()
	outputWrite.Close()
	if err != nil {
		input.Close()
		output.Close()
		return nil, err
	}
	c := &Client{input: input, output: output, writes: make(chan []byte, MaxQueue), stop: make(chan struct{}), events: make(chan Event, MaxQueue), done: make(chan struct{}), cmd: cmd}
	c.Events, c.Done = c.events, c.done
	readerDone := make(chan struct{})
	var workers sync.WaitGroup
	workers.Add(2)
	go func() {
		defer workers.Done()
		defer close(readerDone)
		defer c.Stop()
		reader := bufio.NewReaderSize(output, MaxFrame+1)
		for {
			line, err := reader.ReadSlice('\n')
			if err != nil {
				return
			}
			event, err := Decode(line[:len(line)-1])
			if err != nil {
				return
			}
			if c.queuedBytes.Add(int64(event.byteCount)) > 1024*1024 {
				return
			}
			select {
			case c.events <- event:
			case <-c.stop:
				return
			default:
				return
			}
		}
	}()
	go func() {
		defer workers.Done()
		for {
			select {
			case data := <-c.writes:
				// A helper that stops reading cannot strand browser shutdown.
				if _, err := input.Write(data); err != nil {
					c.Stop()
					return
				}
			case <-c.stop:
				return
			}
		}
	}()
	go func() {
		_ = cmd.Wait()
		c.input.Close()
		timer := time.NewTimer(time.Second)
		select {
		case <-readerDone:
		case <-timer.C:
			c.Stop()
		}
		timer.Stop()
		workers.Wait()
		c.Stop()
		close(c.done)
	}()
	return c, nil
}

func (c *Client) Send(command Command) (string, error) {
	select {
	case <-c.stop:
		return "", errors.New("file transfer helper unavailable")
	default:
	}
	if c.next == ^uint64(0) {
		return "", errors.New("transfer command identity exhausted")
	}
	c.next++
	command.Version, command.ID = 1, strconv.FormatUint(c.next, 10)
	data, err := json.Marshal(command)
	if err != nil || len(data) > MaxFrame {
		return "", errors.New("transfer command exceeds limit")
	}
	select {
	case c.writes <- append(data, '\n'):
		return command.ID, nil
	default:
		c.Stop()
		return "", errors.New("file transfer command queue full")
	}
}

// Closing stdin is the normal EOF retirement path. Kill is an escalation only;
// callers still retain this client and its runtime until Done confirms Wait.
func (c *Client) Stop() {
	c.once.Do(func() {
		close(c.stop)
		c.input.Close()
		c.output.Close()
		go func() {
			timer := time.NewTimer(2 * time.Second)
			defer timer.Stop()
			select {
			case <-c.done:
			case <-timer.C:
				_ = c.cmd.Process.Kill()
			}
		}()
	})
}
func (c *Client) Close() { c.Stop(); <-c.Done }

// Received releases the queued byte budget after the browser handles an event.
func (c *Client) Received(event Event) { c.queuedBytes.Add(-int64(event.byteCount)) }

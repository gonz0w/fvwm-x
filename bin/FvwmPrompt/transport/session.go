// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package transport owns the newline-delimited FvwmMFL connection. It uses
// only the standard library so other first-party Go helpers can reuse it.
package transport

import (
	"bufio"
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"strings"
	"sync"
	"time"
	"unicode/utf8"
)

// These are the current core command limit and MFL output queue limit.
const MaxCommand = 1000
const MaxRecord = 256 * 1024
const WriteTimeout = 5 * time.Second

type Session struct {
	conn      net.Conn
	writeMu   sync.Mutex
	closeOnce sync.Once
	done      chan struct{}
}

func New(conn net.Conn) *Session {
	return &Session{conn: conn, done: make(chan struct{})}
}

func (s *Session) Done() <-chan struct{} { return s.done }

// Close unblocks reads and writes. Callers retain ownership of their UI.
func (s *Session) Close() {
	s.closeOnce.Do(func() {
		close(s.done)
		s.conn.Close()
	})
}

// Send preserves order and completes the entire frame before returning.
// Failure closes the session: retrying a partially written command is unsafe.
func (s *Session) Send(command string) error {
	if len(command) > MaxCommand || strings.ContainsAny(command, "\x00\r\n") {
		return fmt.Errorf("command must be one line of at most %d bytes", MaxCommand)
	}
	if command == "" {
		return nil
	}
	s.writeMu.Lock()
	defer s.writeMu.Unlock()
	select {
	case <-s.done:
		return net.ErrClosed
	default:
	}
	if err := s.conn.SetWriteDeadline(time.Now().Add(WriteTimeout)); err != nil {
		s.Close()
		return err
	}
	data := []byte(command + "\n")
	for len(data) != 0 {
		n, err := s.conn.Write(data)
		if n < 0 || n > len(data) {
			err = io.ErrShortWrite
			n = 0
		}
		data = data[n:]
		if err == nil && n == 0 {
			err = io.ErrNoProgress
		}
		if err != nil {
			s.Close()
			return err
		}
	}
	return nil
}

func splitRecord(data []byte, atEOF bool) (int, []byte, error) {
	if index := bytes.IndexByte(data, '\n'); index >= 0 {
		return index + 1, data[:index], nil
	}
	if atEOF && len(data) != 0 {
		return 0, nil, io.ErrUnexpectedEOF
	}
	return 0, nil, nil
}

// Read delivers complete JSON objects in order, with no unbounded queue.
// The callback must return promptly; Close interrupts the underlying read.
// An idle connection has no deadline. Incomplete records are size-bounded.
func (s *Session) Read(receive func(json.RawMessage) error) error {
	defer s.Close()
	scanner := bufio.NewScanner(s.conn)
	scanner.Buffer(make([]byte, 4096), MaxRecord)
	scanner.Split(splitRecord)
	for scanner.Scan() {
		line := scanner.Bytes()
		object := bytes.TrimLeft(line, " \t\r")
		if len(object) == 0 || object[0] != '{' || !utf8.Valid(line) || !json.Valid(line) {
			return errors.New("invalid MFL JSON object")
		}
		// Own the bytes; Scanner reuses its buffer on the next read.
		if err := receive(append(json.RawMessage(nil), line...)); err != nil {
			return err
		}
	}
	return scanner.Err()
}

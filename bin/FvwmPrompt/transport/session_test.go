// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package transport

import (
	"bytes"
	"encoding/json"
	"errors"
	"io"
	"net"
	"strings"
	"sync"
	"testing"
	"time"
)

type stubConn struct {
	bytes.Buffer
	chunk    int
	writeErr error
	zero     bool
	deadline time.Time
	closed   bool
}

func (c *stubConn) Write(p []byte) (int, error) {
	if c.zero {
		return 0, nil
	}
	if c.chunk > 0 && len(p) > c.chunk {
		p = p[:c.chunk]
	}
	n, _ := c.Buffer.Write(p)
	return n, c.writeErr
}
func (c *stubConn) Close() error                       { c.closed = true; return nil }
func (c *stubConn) LocalAddr() net.Addr                { return nil }
func (c *stubConn) RemoteAddr() net.Addr               { return nil }
func (c *stubConn) SetDeadline(time.Time) error        { return nil }
func (c *stubConn) SetReadDeadline(time.Time) error    { return nil }
func (c *stubConn) SetWriteDeadline(d time.Time) error { c.deadline = d; return nil }

func TestCompleteWritesAndValidation(t *testing.T) {
	c := &stubConn{chunk: 2}
	s := New(c)
	if err := s.Send("Echo one"); err != nil {
		t.Fatal(err)
	}
	if err := s.Send("Echo two"); err != nil {
		t.Fatal(err)
	}
	if c.String() != "Echo one\nEcho two\n" {
		t.Fatal(c.String())
	}
	if time.Until(c.deadline) <= 0 || time.Until(c.deadline) > WriteTimeout {
		t.Fatal(c.deadline)
	}
	for _, command := range []string{"Echo\x00bad", "Echo\nsecond", "Echo\rbad", strings.Repeat("x", MaxCommand+1)} {
		if err := s.Send(command); err == nil || c.closed {
			t.Fatal("local rejection must retain session", err)
		}
	}
	if err := s.Send(strings.Repeat("x", MaxCommand)); err != nil {
		t.Fatal(err)
	}
	s.Close()
	if !errors.Is(s.Send("Echo closed"), net.ErrClosed) {
		t.Fatal("closed send")
	}
}

func TestFailedWritesCloseWithoutRetry(t *testing.T) {
	for _, c := range []*stubConn{{chunk: 2, writeErr: io.ErrClosedPipe}, {zero: true}} {
		s := New(c)
		if err := s.Send("Echo partial"); err == nil || !c.closed {
			t.Fatal("failed write retained", err)
		}
		before := c.String()
		if !errors.Is(s.Send("Echo retry"), net.ErrClosed) || c.String() != before {
			t.Fatal("retried partial frame")
		}
	}
}

func TestRecords(t *testing.T) {
	for _, tc := range []struct {
		input   string
		count   int
		failure bool
	}{
		{"{\"one\":1}\n{\"two\":2}\n", 2, false},
		{"", 0, false}, {"{}", 0, true}, {"{}\n{", 1, true},
		{"[]\n", 0, true}, {"{bad}\n", 0, true}, {"\n", 0, true},
		{"{\"text\":\"\xff\"}\n", 0, true}, {"  {}\n", 1, false},
		{"\u00a0{}\n", 0, true}, {"\f{}\n", 0, true}, {" \t{}\r\n", 1, false},
		{"{\"text\":\"" + strings.Repeat("x", MaxRecord-12) + "\"}\n", 1, false},
		{"{\"text\":\"" + strings.Repeat("x", MaxRecord) + "\"}\n", 0, true},
	} {
		c := &stubConn{}
		c.Buffer.WriteString(tc.input)
		count := 0
		err := New(c).Read(func(record json.RawMessage) error { count++; return nil })
		if count != tc.count || (err != nil) != tc.failure || !c.closed {
			t.Fatalf("length %d: count %d, error %v", len(tc.input), count, err)
		}
	}
}

func TestFragmentedRecordsAndOwnedBytes(t *testing.T) {
	client, peer := net.Pipe()
	s := New(client)
	go func() {
		defer peer.Close()
		for _, chunk := range []string{"{", "\"one\":", "1}\n{\"two\":2}\n"} {
			peer.Write([]byte(chunk))
		}
	}()
	var records []json.RawMessage
	if err := s.Read(func(record json.RawMessage) error { records = append(records, record); return nil }); err != nil {
		t.Fatal(err)
	}
	if len(records) != 2 || string(records[0]) != "{\"one\":1}" || string(records[1]) != "{\"two\":2}" {
		t.Fatal(records)
	}
}

func TestCloseInterruptsIdleReadAndBlockedWrite(t *testing.T) {
	client, peer := net.Pipe()
	defer peer.Close()
	s := New(client)
	results := make(chan error, 2)
	go func() { results <- s.Read(func(json.RawMessage) error { return nil }) }()
	go func() { results <- s.Send("Echo blocked") }()
	s.Close()
	for i := 0; i < 2; i++ {
		select {
		case <-results:
		case <-time.After(time.Second):
			t.Fatal("operation retained")
		}
	}
}

func TestConcurrentCommandsRemainWhole(t *testing.T) {
	client, peer := net.Pipe()
	s := New(client)
	defer s.Close()
	var wg sync.WaitGroup
	for i := 0; i < 32; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			if err := s.Send("Echo ordered"); err != nil {
				t.Error(err)
			}
		}()
	}
	read := make(chan []byte, 1)
	go func() { data, _ := io.ReadAll(peer); read <- data }()
	wg.Wait()
	s.Close()
	data := <-read
	peer.Close()
	if string(data) != strings.Repeat("Echo ordered\n", 32) {
		t.Fatal("interleaved commands")
	}
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"strings"
	"time"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/transport"
)

func requestID(text string) (string, error) {
	if len(text)+len("request ") > transport.MaxCommand ||
		strings.ContainsAny(text, "\x00\r\n") || !utf8.ValidString(text) {
		return "", errors.New("request must be one bounded UTF-8 JSON line")
	}
	decoder := json.NewDecoder(strings.NewReader(text))
	start, err := decoder.Token()
	if err != nil || start != json.Delim('{') {
		return "", errors.New("request must be a JSON object")
	}
	seen := make(map[string]bool)
	id := ""
	for decoder.More() {
		token, err := decoder.Token()
		key, ok := token.(string)
		if err != nil || !ok || seen[key] {
			return "", errors.New("request has duplicate or invalid fields")
		}
		seen[key] = true
		var value json.RawMessage
		if decoder.Decode(&value) != nil {
			return "", errors.New("invalid request JSON")
		}
		if key == "id" && json.Unmarshal(value, &id) != nil {
			return "", errors.New("request id must be a string")
		}
	}
	if end, err := decoder.Token(); err != nil || end != json.Delim('}') {
		return "", errors.New("invalid request JSON")
	}
	var extra any
	if decoder.Decode(&extra) != io.EOF {
		return "", errors.New("request must contain exactly one object")
	}
	if len(id) == 0 || len(id) > 64 {
		return "", errors.New("request id must contain 1 to 64 ASCII identifier bytes")
	}
	for _, c := range id {
		if !(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_' || c == '-' || c == '.') {
			return "", errors.New("invalid request id")
		}
	}
	return id, nil
}

var errReplyComplete = errors.New("request reply complete")

func requestOnce(o options, output io.Writer, getenv func(string) string) error {
	id, err := requestID(o.request)
	if err != nil {
		return err
	}
	connection, err := net.DialTimeout("unix", socketPath(o.socket, getenv, os.Getuid()), transport.WriteTimeout)
	if err != nil {
		return fmt.Errorf("unable to connect to FvwmMFL: %w", err)
	}
	session := transport.New(connection)
	finished := make(chan error, 1)
	var reply json.RawMessage
	go func() {
		finished <- session.Read(func(record json.RawMessage) error {
			var envelope struct {
				Reply json.RawMessage `json:"reply"`
			}
			if json.Unmarshal(record, &envelope) != nil {
				return errors.New("invalid MFL JSON")
			}
			if len(envelope.Reply) == 0 {
				return nil
			}
			var details struct {
				Version int     `json:"version"`
				ID      *string `json:"id"`
				Status  string  `json:"status"`
			}
			if json.Unmarshal(envelope.Reply, &details) != nil || details.Version != 1 ||
				(details.ID != nil && *details.ID != id) ||
				(details.Status != "ok" && details.Status != "error") ||
				details.ID == nil && details.Status != "error" {
				return errors.New("invalid or mismatched request reply")
			}
			reply = append(json.RawMessage(nil), record...)
			return errReplyComplete
		})
	}()
	joined := false
	defer func() {
		session.Close()
		if !joined {
			<-finished
		}
	}()
	if err := session.Send("request " + o.request); err != nil {
		return err
	}
	// MFL's domain wait is five seconds; allow one second for its terminal
	// deadline reply, then close/join the sole reader on every exit.
	timer := time.NewTimer(6 * time.Second)
	defer timer.Stop()
	select {
	case err := <-finished:
		joined = true
		if err != errReplyComplete {
			if err == nil {
				err = errors.New("MFL disconnected before the request reply")
			}
			return err
		}
	case <-timer.C:
		return errors.New("timed out waiting for correlated MFL reply")
	}
	decoder := json.NewDecoder(bytes.NewReader(reply))
	decoder.UseNumber()
	var object any
	if err := decoder.Decode(&object); err != nil {
		return err
	}
	encoder := json.NewEncoder(output)
	encoder.SetIndent("", "    ")
	encoder.SetEscapeHTML(false)
	if err := encoder.Encode(object); err != nil {
		return err
	}
	var envelope struct {
		Reply struct {
			Status string `json:"status"`
		} `json:"reply"`
	}
	json.Unmarshal(reply, &envelope)
	if envelope.Reply.Status == "error" {
		return errors.New("MFL request refused; see JSON error code")
	}
	return nil
}

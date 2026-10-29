// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package transport

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"strings"
	"time"
	"unicode/utf8"
)

func RequestID(text string) (string, error) {
	if len(text)+len("request ") > MaxRequest ||
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

// Request exchanges one ordinary metadata/control request. It closes and joins
// its sole reader on every exit. Sensitive content needs its separate owner.
// A terminal domain refusal is returned as JSON; only transport/framing fails.
func Request(parent context.Context, endpoint, text string) (json.RawMessage, error) {
	return request(parent, endpoint, text, false)
}

// Query admits read-only mode on the same connection before sending a query.
// It never sends caller text until that admission succeeds.
func Query(parent context.Context, endpoint, text string) (json.RawMessage, error) {
	if _, err := RequestID(text); err != nil {
		return nil, err
	}
	var header struct {
		Method string `json:"method"`
	}
	if json.Unmarshal([]byte(text), &header) != nil || header.Method != "query" {
		return nil, errors.New("read-only requests require method query")
	}
	return request(parent, endpoint, text, true)
}

func request(parent context.Context, endpoint, text string, readonly bool) (json.RawMessage, error) {
	id, err := RequestID(text)
	if err != nil {
		return nil, err
	}
	ctx, cancel := context.WithTimeout(parent, 6*time.Second)
	defer cancel()
	connection, err := (&net.Dialer{Timeout: WriteTimeout}).DialContext(ctx, "unix", endpoint)
	if err != nil {
		return nil, fmt.Errorf("unable to connect to FvwmMFL: %w", err)
	}
	session := New(connection)
	stop := context.AfterFunc(ctx, session.Close)
	defer stop()
	finished := make(chan error, 1)
	var reply json.RawMessage
	modePending := readonly
	modeID := "readonly"
	if id == modeID {
		modeID = "readonly-mode"
	}
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
				Version int             `json:"version"`
				ID      *string         `json:"id"`
				Status  string          `json:"status"`
				Result  json.RawMessage `json:"result"`
			}
			expectedID := id
			if modePending {
				expectedID = modeID
			}
			if json.Unmarshal(envelope.Reply, &details) != nil || details.Version != 1 ||
				(details.ID != nil && *details.ID != expectedID) || (details.Status != "ok" && details.Status != "error") ||
				details.ID == nil && details.Status != "error" {
				return errors.New("invalid or mismatched request reply")
			}
			if modePending {
				var admission struct {
					ReadOnly bool `json:"read_only"`
				}
				if details.Status != "ok" || json.Unmarshal(details.Result, &admission) != nil || !admission.ReadOnly {
					return errors.New("FvwmMFL refused read-only admission")
				}
				modePending = false
				return session.sendRequest(text)
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
	first := text
	if readonly {
		data, _ := json.Marshal(map[string]any{"version": 1, "id": modeID,
			"method": "set_mode", "mode": "read-only"})
		first = string(data)
	}
	if err := session.sendRequest(first); err != nil {
		if ctx.Err() != nil {
			return nil, ctx.Err()
		}
		return nil, err
	}
	select {
	case err := <-finished:
		joined = true
		if ctx.Err() != nil {
			return nil, ctx.Err()
		}
		if err != errReplyComplete {
			if err == nil {
				err = errors.New("MFL disconnected before the request reply")
			}
			return nil, err
		}
	case <-ctx.Done():
		return nil, ctx.Err()
	}
	return reply, nil
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filetransfer

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"errors"
	"io"
	"strconv"
	"strings"
	"unicode/utf8"
)

// Schema mirrors FILE-TRANSFER-2026-10-31.md and the native helper. IDs are
// canonical uint64 decimal strings; native generation/time/XIDs are uint32.
type Command struct {
	Version    int    `json:"v"`
	ID         string `json:"id"`
	Command    string `json:"command"`
	Epoch      string `json:"epoch,omitempty"`
	Generation uint32 `json:"generation,omitempty"`
	Enabled    *bool  `json:"enabled,omitempty"`
	Transfer   string `json:"transfer,omitempty"`
	Lease      string `json:"lease,omitempty"`
	Action     string `json:"action,omitempty"`
	URIs       string `json:"uris,omitempty"`
	Time       uint32 `json:"time,omitempty"`
	Success    *bool  `json:"success,omitempty"`
}

type Event struct {
	byteCount  int
	Hostname   string `json:"hostname,omitempty"`
	Version    int    `json:"v"`
	Event      string `json:"event,omitempty"`
	ID         string `json:"id,omitempty"`
	Status     string `json:"status,omitempty"`
	Code       string `json:"code,omitempty"`
	Epoch      string `json:"epoch,omitempty"`
	Generation uint32 `json:"generation,omitempty"`
	Frame      uint32 `json:"frame,omitempty"`
	List       uint32 `json:"list,omitempty"`
	Transfer   string `json:"transfer,omitempty"`
	Kind       string `json:"kind,omitempty"`
	Action     string `json:"action,omitempty"`
	URIs       string `json:"uris,omitempty"`
	Time       uint32 `json:"time,omitempty"`
	Rows       uint32 `json:"rows,omitempty"`
	Selection  string `json:"selection,omitempty"`
	Lease      string `json:"lease,omitempty"`
}

func CanonicalID(id string) bool {
	n, err := strconv.ParseUint(id, 10, 64)
	return err == nil && n > 0 && strconv.FormatUint(n, 10) == id
}

// Decode rejects duplicate keys as well as unknown fields. No error includes
// record content: proposals and outgoing buffers contain private filename bytes.
func Decode(data []byte) (Event, error) {
	var event Event
	if len(data) > MaxFrame || !utf8.Valid(data) {
		return event, errors.New("invalid transfer frame")
	}
	keys := json.NewDecoder(bytes.NewReader(data))
	token, err := keys.Token()
	if err != nil || token != json.Delim('{') {
		return event, errors.New("invalid transfer object")
	}
	seen := map[string]bool{}
	for keys.More() {
		key, err := keys.Token()
		if err != nil {
			return event, errors.New("invalid transfer key")
		}
		name, ok := key.(string)
		if !ok || seen[name] {
			return event, errors.New("duplicate transfer key")
		}
		seen[name] = true
		var value json.RawMessage
		if keys.Decode(&value) != nil || bytes.Equal(value, []byte("null")) {
			return event, errors.New("invalid transfer value")
		}
	}
	if _, err := keys.Token(); err != nil {
		return event, errors.New("invalid transfer frame")
	}
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.DisallowUnknownFields()
	if decoder.Decode(&event) != nil || decoder.Decode(new(any)) != io.EOF || event.Version != 1 {
		return Event{}, errors.New("invalid transfer schema")
	}
	for _, id := range []string{event.ID, event.Epoch, event.Transfer, event.Lease} {
		if id != "" && !CanonicalID(id) {
			return Event{}, errors.New("invalid transfer identity")
		}
	}
	allowed := "v event"
	switch event.Event {
	case "":
		allowed = "v id status code lease transfer"
	case "ready":
		allowed += " hostname"
	case "unavailable":
		allowed += " code"
	case "attached":
		allowed += " epoch generation frame list"
	case "action":
		allowed += " transfer kind epoch generation time rows selection"
	case "drag-begin":
		allowed += " transfer epoch generation time rows selection"
	case "proposal":
		allowed += " id transfer kind action uris epoch generation lease"
	case "result":
		allowed += " transfer status action"
	case "lost":
		allowed += " lease"
	}
	for key := range seen {
		if !strings.Contains(" "+allowed+" ", " "+key+" ") {
			return Event{}, errors.New("unexpected transfer field")
		}
	}
	if event.Event == "" {
		if event.ID == "" || (event.Status != "ok" && event.Status != "error") || len(event.Code) > 64 {
			return Event{}, errors.New("invalid transfer reply")
		}
		event.byteCount = len(data)
		return event, nil
	}
	switch event.Event {
	case "ready":
		if !validHost(event.Hostname) {
			return Event{}, errors.New("invalid local hostname")
		}
	case "unavailable":
		if event.Code == "" || len(event.Code) > 64 {
			return Event{}, errors.New("invalid unavailable event")
		}
	case "attached":
		if event.Epoch == "" || event.Generation == 0 || event.Frame == 0 || event.List == 0 {
			return Event{}, errors.New("invalid attachment")
		}
	case "action", "drag-begin":
		if event.Transfer == "" || event.Epoch == "" || event.Generation == 0 || event.Time == 0 || !seen["rows"] || !seen["selection"] {
			return Event{}, errors.New("invalid native action")
		}
		if event.Event == "action" && event.Kind != "copy" && event.Kind != "cut" && event.Kind != "paste" {
			return Event{}, errors.New("invalid native action kind")
		}
		if _, err := event.Selected(); err != nil {
			return Event{}, err
		}
	case "proposal":
		if event.Transfer == "" || event.Epoch == "" || event.Generation == 0 || (event.Kind != "clipboard" && event.Kind != "dnd") || (event.Action != "copy" && event.Action != "move") || len(event.URIs) == 0 || len(event.URIs) > MaxURIBytes {
			return Event{}, errors.New("invalid transfer proposal")
		}
	case "result":
		if event.Transfer == "" || (event.Action != "copy" && event.Action != "move") || (event.Status != "success" && event.Status != "failed" && event.Status != "cancelled" && event.Status != "timeout") {
			return Event{}, errors.New("invalid transfer result")
		}
	case "lost":
		if event.Lease == "" {
			return Event{}, errors.New("invalid lease event")
		}
	default:
		return Event{}, errors.New("unknown transfer event")
	}
	event.byteCount = len(data)
	return event, nil
}

// Selected checks exact bitset length, unused high bits and the selection cap.
// Bit zero is the first row, matching the native metadata protocol.
func (e Event) Selected() ([]int, error) {
	if e.Rows > 4096 || len(e.Selection) > 684 {
		return nil, errors.New("native row limit")
	}
	bits, err := base64.StdEncoding.Strict().DecodeString(e.Selection)
	if err != nil || len(bits) != int((e.Rows+7)/8) || base64.StdEncoding.EncodeToString(bits) != e.Selection {
		return nil, errors.New("invalid native selection")
	}
	ids := []int{}
	for i, b := range bits {
		for bit := 0; bit < 8; bit++ {
			if b&(1<<bit) != 0 {
				index := i*8 + bit
				if index >= int(e.Rows) || len(ids) == MaxReferences {
					return nil, errors.New("native selection limit")
				}
				ids = append(ids, index)
			}
		}
	}
	return ids, nil
}

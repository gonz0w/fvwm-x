// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"github.com/fvwmorg/FvwmPrompt/transport"
)

const providerPageMax = 8
const providerResultMax = 8192
const providerChunk = 256

type fileQuery struct {
	Version    int             `json:"version"`
	Serial     string          `json:"serial"`
	Provider   string          `json:"provider"`
	Scope      string          `json:"scope"`
	Generation string          `json:"generation"`
	Offset     int             `json:"offset"`
	Limit      int             `json:"limit"`
	Operation  string          `json:"operation,omitempty"`
	Values     json.RawMessage `json:"values,omitempty"`
}

type providerRegistration struct {
	Version int    `json:"version"`
	ID      string `json:"id"`
	Status  string `json:"status"`
	Result  struct {
		Provider   string `json:"provider"`
		Registered bool   `json:"registered"`
	} `json:"result"`
}

func (b *browser) registerProvider(ctx context.Context,
	registered <-chan providerRegistration, finished <-chan error) error {
	data, _ := json.Marshal(map[string]any{"version": 1, "id": "file-register",
		"method": "register", "resource": "files", "provider": b.instance})
	if err := b.session.Send("request " + string(data)); err != nil {
		return err
	}
	timer := time.NewTimer(transport.WriteTimeout)
	defer timer.Stop()
	select {
	case reply := <-registered:
		if reply.Version == 1 && reply.Status == "ok" && reply.Result.Registered && reply.Result.Provider == b.instance {
			return nil
		}
		return errors.New("file provider registration refused; use matching FvwmMFL/browser builds")
	case <-finished:
		return errors.New("file provider connection closed during registration")
	case <-ctx.Done():
		return ctx.Err()
	case <-timer.C:
		return errors.New("file provider registration timed out")
	}
}

func hexToken(value string, size int) bool {
	if len(value) != size {
		return false
	}
	for _, c := range value {
		if !(c >= '0' && c <= '9' || c >= 'a' && c <= 'f') {
			return false
		}
	}
	return true
}

func (b *browser) providerGeneration() string {
	// A snapshot token binds metadata/projection plus current native selection
	// and focus. Draft inputs and file content never enter the public snapshot.
	data, _ := json.Marshal(struct {
		Collection   uint64
		Selected     []string
		Focus        string
		View         string
		Inspected    []string
		Work         *providerWork
		Busy         bool
		Hidden       bool
		WatchEnabled bool
	}{b.model.Generation, b.model.SelectedIDs(), b.model.Focus, b.viewing(), b.inspected, b.providerWork, b.active, b.snapshot != nil && b.snapshot.Hidden, b.watch != nil && b.watch.enabled})
	sum := sha256.Sum256(data)
	return hex.EncodeToString(sum[:16])
}

func (b *browser) answerProvider(query fileQuery) (bool, error) {
	if !b.validFileQuery(query) {
		return false, errors.New("invalid file provider query")
	}
	var result map[string]any
	code := ""
	applied := false
	var action providerAction
	if query.Scope == "action" {
		var err error
		action, err = decodeProviderAction(query)
		if err != nil {
			code = "invalid_request"
		}
	}
	if code == "" && b.snapshot != nil && b.alias != "" {
		b.renderDeadline = time.Now().Add(workerLimit)
		packet, err := b.snapshotNative("q"+query.Serial, b.nativeGeneration, 0)
		if err != nil || b.model.SelectNative(b.model.Generation, packet) != nil {
			code = "native_state_unavailable"
		}
	}
	if code == "" && query.Scope == "action" {
		code = b.applyProviderAction(query, action)
		if code == "" {
			if err := b.show(); err != nil {
				// Retire the view on failed publication: never acknowledge a
				// split model/native state as an applied operation.
				return false, err
			}
			if query.Operation == "present" {
				selector := "All (FvwmFiles-" + b.instance + "-*) "
				if err := b.sendCommands([]string{selector + "Iconify false", selector + "Focus", selector + "Raise"}); err != nil {
					return false, err
				}
			}
			applied = true
			stateQuery := fileQuery{Version: 1, Provider: b.instance,
				Serial: query.Serial, Scope: "state", Limit: providerPageMax}
			result, code = b.providerResult(stateQuery)
			if code == "" {
				result["scope"], result["operation"], result["applied"] = "action", query.Operation, true
			}
		}
	} else if code == "" {
		result, code = b.providerResult(query)
	}
	envelope := map[string]any{"result": result}
	if code != "" {
		envelope = map[string]any{"error": map[string]string{"code": code}}
	}
	data, err := json.Marshal(envelope)
	if err != nil {
		return false, err
	}
	if len(data) > providerResultMax {
		data = []byte(`{"error":{"code":"result_too_large"}}`)
	}
	parts := 0
	for start := 0; start < len(data); start += providerChunk {
		end := start + providerChunk
		if end > len(data) {
			end = len(data)
		}
		if err := b.session.Send(fmt.Sprintf("files_reply %s part %d %s", query.Serial, parts, hex.EncodeToString(data[start:end]))); err != nil {
			return false, err
		}
		parts++
	}
	return applied, b.session.Send(fmt.Sprintf("files_reply %s end %d", query.Serial, parts))
}

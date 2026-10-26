// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopworker

import (
	"context"
	"encoding/json"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopinspect"
)

func TestInspectorPrivateAdmissionAndNoBackendActions(t *testing.T) {
	payload := json.RawMessage(`{"scope":"commands"}`)
	request := Request{Version: 1, Operation: "inspect", Inspect: payload}
	for _, field := range []string{"system", "action", "path", "directories", "check", "unknown"} {
		m := map[string]any{"version": 1, "operation": "inspect", "inspect": payload}
		m[field] = map[string]any{"system": "Linux", "action": "up", "path": "/tmp", "directories": []string{}, "check": map[string]any{}, "unknown": false}[field]
		data, _ := json.Marshal(m)
		if _, err := Decode(data); err == nil {
			t.Fatalf("admitted %s", field)
		}
	}
	for _, data := range []string{`{"version":1,"operation":"status","inspect":{"scope":"commands"}}`, `{"version":1,"operation":"inspect"}`, `{"version":1,"operation":"inspect","inspect":{"scope":"all","scope":"commands"}}`, `{"version":1,"operation":"inspect","inspect":{"scope":"commands"},"system":""}`} {
		if _, err := Decode([]byte(data)); err == nil {
			t.Fatal("accepted " + data)
		}
	}
	data, _ := json.Marshal(request)
	if _, err := Decode(data); err != nil {
		t.Fatal(err)
	}
	env := Environment{Run: func(context.Context, []string, time.Duration) (string, error) {
		t.Fatal("inspect executed backend")
		return "", nil
	}, Query: func(context.Context, string) (json.RawMessage, error) {
		t.Fatal("command inventory queried MFL")
		return nil, nil
	}}
	value, err := Run(context.Background(), request, env)
	if err != nil {
		t.Fatal(err)
	}
	result, ok := value.(desktopinspect.Result)
	if !ok || len(result.Sections) != 1 || len(result.Sections[0].Rows) == 0 {
		t.Fatal(value)
	}
}

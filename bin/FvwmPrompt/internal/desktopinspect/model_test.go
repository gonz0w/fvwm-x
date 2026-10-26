// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopinspect

import (
	"context"
	"encoding/json"
	"strings"
	"testing"
)

func TestStrictRequestAdmission(t *testing.T) {
	for _, data := range []string{`null`, `[]`, `{}`, `{"scope":"all","scope":"commands"}`, `{"Scope":"all"}`, `{"scope":"all","unknown":1}`, `{"scope":"all","filter":null}`, `{"scope":"all","filter":true}`, `{"scope":"all","provider":"/home/secret"}`, `{"scope":"all","source":null}`, `{"scope":"all","source":{"Path":"x","userdir":"x","datadir":"x","cwd":"x"}}`, `{"scope":"all","settings":{"x":1,"x":2}}`, `{"scope":"all","colors":{"background":"#fffffz"}}`, `{"scope":"all"} {}`, `{"scope":"all","filter":"` + strings.Repeat("é", 65) + `"}`, strings.Repeat(" ", RequestLimit+1)} {
		if _, err := DecodeRequest([]byte(data)); err == nil {
			t.Errorf("accepted %s", data)
		}
	}
	for _, data := range []string{`{"scope":"all"}`, `{"scope":"settings","settings":null,"colors":null}`, `{"scope":"commands","filter":"Echo"}`} {
		if _, err := DecodeRequest([]byte(data)); err != nil {
			t.Error(err)
		}
	}
}
func TestEntireResultValidationBeforePublication(t *testing.T) {
	r, err := Collect(context.Background(), Request{Scope: "commands"}, nil)
	if err != nil {
		t.Fatal(err)
	}
	data, _ := json.Marshal(r)
	if _, err := DecodeResult(data); err != nil {
		t.Fatal(err)
	}
	var root map[string]any
	_ = json.Unmarshal(data, &root)
	for _, mutate := range []func(map[string]any){
		func(m map[string]any) { m["schema_version"] = true },
		func(m map[string]any) { delete(m, "provider") },
		func(m map[string]any) { m["Schema_Version"] = m["schema_version"]; delete(m, "schema_version") },
		func(m map[string]any) { m["sections"] = nil },
		func(m map[string]any) { m["extra"] = 1 },
		func(m map[string]any) { m["sections"].([]any)[0].(map[string]any)["state"] = "fresh" },
		func(m map[string]any) { m["sections"].([]any)[0].(map[string]any)["truncated"] = nil },
		func(m map[string]any) { m["sections"].([]any)[0].(map[string]any)["reason"] = strings.Repeat("x", 201) },
		func(m map[string]any) {
			m["sections"].([]any)[0].(map[string]any)["rows"].([]any)[0].(map[string]any)["value"] = strings.Repeat("x", 201)
		},
		func(m map[string]any) {
			m["sections"].([]any)[0].(map[string]any)["rows"].([]any)[0].(map[string]any)["Label"] = "case"
		},
	} {
		var candidate map[string]any
		_ = json.Unmarshal(data, &candidate)
		mutate(candidate)
		bad, _ := json.Marshal(candidate)
		if _, err := DecodeResult(bad); err == nil {
			t.Fatalf("accepted %s", bad)
		}
	}
	if _, err := DecodeResult([]byte(strings.Replace(string(data), `"schema_version":1`, `"schema_version":1,"schema_version":1`, 1))); err == nil {
		t.Fatal("duplicate result field")
	}
}

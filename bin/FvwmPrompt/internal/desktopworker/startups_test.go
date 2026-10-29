// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopworker

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"
	"testing"
)

func startupQueryFixture(t *testing.T, mutate func(int, map[string]any)) func(context.Context, string) (json.RawMessage, error) {
	t.Helper()
	return func(ctx context.Context, text string) (json.RawMessage, error) {
		var request map[string]any
		if err := json.Unmarshal([]byte(text), &request); err != nil {
			t.Fatal(err)
		}
		offset := int(request["offset"].(float64))
		if request["resource"] != "startups" || request["method"] != "query" || request["limit"] != float64(16) || offset > 0 && request["generation"] != strings.Repeat("a", 48) {
			t.Fatal("startup query changed ownership/paging", request)
		}
		var next any
		if offset == 0 {
			next = 1
		}
		page := map[string]any{"state": "observed", "scope": "core-startup-activity", "generation": strings.Repeat("a", 48), "offset": offset, "next_offset": next, "more": next != nil, "total": 2, "limit": 16,
			"startups": []any{map[string]any{"id": fmt.Sprintf("launch-%d_TIME0", offset), "name": "App", "icon": "icon", "application_id": "app.desktop", "wmclass": "App", "state": []string{"pending", "protocol-ended"}[offset], "window": 0, "age_ms": 25}},
			"counters": map[string]any{"dropped": 0, "malformed": 1, "partial_expired": 0},
			"limits":   map[string]any{"records": 64, "streams": 32, "message_bytes": 4096, "partial_ms": 5000, "pending_ms": 60000, "retention_ms": 60000}}
		if mutate != nil {
			mutate(offset, page)
		}
		data, err := json.Marshal(map[string]any{"reply": map[string]any{"version": 1, "id": request["id"], "status": "ok", "result": page}})
		return data, err
	}
}

func TestStartupSnapshotCollectsCompleteGenerationWithoutSuccessClaim(t *testing.T) {
	got, err := collectStartups(context.Background(), startupQueryFixture(t, nil))
	if err != nil || len(got.Startups) != 2 || got.Startups[1].State != "protocol-ended" || got.Counters.Malformed != 1 {
		t.Fatal(got, err)
	}
	for _, mutation := range []func(int, map[string]any){
		func(offset int, p map[string]any) {
			if offset == 1 {
				p["generation"] = strings.Repeat("b", 48)
			}
		},
		func(offset int, p map[string]any) { p["next_offset"] = 0 },
		func(offset int, p map[string]any) { p["total"] = 65 },
		func(offset int, p map[string]any) { p["startups"].([]any)[0].(map[string]any)["state"] = "success" },
		func(offset int, p map[string]any) { delete(p, "next_offset") },
	} {
		if got, err := collectStartups(context.Background(), startupQueryFixture(t, mutation)); err == nil || len(got.Startups) != 0 {
			t.Fatal("malformed/changing snapshot yielded partial activity", got, err)
		}
	}
}

func TestStartupMetadataAcquisitionPreservesParentActionPolicy(t *testing.T) {
	env := xdgApplicationEnvironment(t)
	root := t.TempDir()
	writeXDGApplication(t, root, "app.desktop", "Name=Parent\nIcon=parent-icon\nStartupNotify=true\nStartupWMClass=ParentClass\nExec=app\nActions=One;\n[Desktop Action One]\nName=One\nExec=action\nStartupNotify=false\n")
	got, err := applications(context.Background(), []string{root}, env)
	if err != nil || len(got) != 2 {
		t.Fatal(got, err)
	}
	for _, app := range got {
		if app.Startup == nil || app.Startup.ApplicationID != "app.desktop" || app.Startup.WMClass != "ParentClass" || app.Startup.Notify == nil || !*app.Startup.Notify || app.Startup.Name != app.Name {
			t.Fatal("Desktop discovery dropped parent startup metadata", app)
		}
	}
}

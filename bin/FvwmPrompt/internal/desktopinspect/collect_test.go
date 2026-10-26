// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopinspect

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopcheck"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

const fixtureGeneration = "000000000000000000000000000000000000000000000001"

func fixtureRequest(t *testing.T, scope string) Request {
	t.Helper()
	values := desktopsettings.Defaults()
	values["contrast"] = "high"
	snapshot, err := desktopsettings.Query(desktopsettings.Snapshot{Saved: values, Effective: values, Overrides: desktopsettings.Values{}, Generation: "gen-1", Submitted: true}, "all", true)
	if err != nil {
		t.Fatal(err)
	}
	data, _ := json.Marshal(snapshot)
	return Request{Scope: scope, Provider: "desktop-1", SettingsGeneration: "gen-1", Settings: data, Appearance: "dark", Colors: map[string]string{"background": "#171b25", "surface": "#222838", "raised": "#313b50", "text": "#f4f6ff", "muted": "#bdc7dc", "border": "#53617c", "accent": "#83c3ff", "on_accent": "#132334"}}
}
func fixtureResult(resource, scope string) map[string]any {
	m := map[string]any{"state": "observed", "scope": "core-fixture", "generation": fixtureGeneration, "offset": 0, "next_offset": nil}
	rect := map[string]any{"x": 0, "y": 0, "width": 800, "height": 600}
	switch resource {
	case "appearance":
		m["window_effective_state"] = false
		m["rendered_completion"] = "unknown"
	case "colorsets":
		m["colorsets"] = []any{map[string]any{"id": 2, "update_state": "pending", "pixels": map[string]any{"foreground": "0000000000ffffff", "background": "0000000000000000"}, "background": map[string]any{"transparency": "none"}}}
	case "menu-styles":
		m["menu_styles"] = []any{map[string]any{"name": "fixture-menu", "opacity_percent": 100, "border_width": 2, "colorsets": map[string]any{"menu": 5, "active": 6}}}
	case "decorations":
		m["decorations"] = []any{map[string]any{"name": nil, "title_height": 20, "minimum_title_height": 12, "title_justification": "center"}}
	case "bindings":
		switch scope {
		case "policy":
			m["contexts"] = map[string]any{"root": 8, "window": 1}
			m["modifiers"] = map[string]any{"shift": 1, "control": 4}
			m["ignored_modifier_mask"] = 2
			m["xkb_ignored_modifier_mask"] = 16
			m["used_modifier_mask"] = 5
		case "placement":
			m["button_count"] = 15
			m["drag_finish_mask"] = 4
			m["interactive_finish_mask"] = 2
			m["key_bindings"] = false
			m["modifier_bindings"] = false
		default:
			m["bindings"] = []any{map[string]any{"type": "Key", "code": 67, "key": "F1", "context_mask": 8, "modifier_mask": 4, "action": "Exec touch /private/SECRET-SENTINEL", "window": "SECRET-WINDOW"}, map[string]any{"type": "MousePress", "code": 1, "key": nil, "context_mask": 1, "modifier_mask": 0, "action": "SECRET-FUNCTION SECRET-ARGS", "window": nil}}
		}
	case "topology":
		m["units"] = "actual-pixels"
		m["topology"] = map[string]any{"epoch": "0000000000000001", "monitors": []any{map[string]any{"name": "test-monitor", "primary": true, "geometry": rect}}}
	case "workspaces":
		m["units"] = "actual-pixels"
		m["desktop_mode"] = "global"
		m["work_area_freshness"] = "cached"
		m["monitors"] = []any{map[string]any{"name": "test-monitor", "desk": 2, "scale_quarters": 6, "viewport": rect, "pages": map[string]any{"width": 3, "height": 2}, "work_area": rect, "dynamic_work_area": nil}}
	case "desktop":
		delete(m, "state")
		m["scope"] = "core-current-monitor"
		m["units"] = "actual-pixels"
		m["desk"] = 2
		m["viewport"] = rect
		m["work_area"] = rect
	}
	return m
}
func fixtureQuery(t *testing.T, mutate func(map[string]json.RawMessage, map[string]any)) func(context.Context, string) (json.RawMessage, error) {
	t.Helper()
	return func(ctx context.Context, text string) (json.RawMessage, error) {
		if err := ctx.Err(); err != nil {
			return nil, err
		}
		var request map[string]json.RawMessage
		if err := json.Unmarshal([]byte(text), &request); err != nil {
			t.Fatal(err)
		}
		if stringField(request, "method") != "query" {
			t.Fatal("nonquery operation")
		}
		m := fixtureResult(stringField(request, "resource"), stringField(request, "scope"))
		if mutate != nil {
			mutate(request, m)
		}
		data, err := json.Marshal(map[string]any{"reply": map[string]any{"version": 1, "id": stringField(request, "id"), "status": "ok", "result": m}})
		return data, err
	}
}
func textResult(r Result) string { data, _ := json.Marshal(r); return string(data) }
func TestAllSectionsHaveBoundedTruthfulInventory(t *testing.T) {
	r := fixtureRequest(t, "all")
	root := t.TempDir()
	r.Source = desktopcheck.Request{Path: filepath.Join(root, "config"), Userdir: root, Datadir: root, Cwd: root}
	if err := os.WriteFile(r.Source.Path, []byte("*FvwmDesktop: motion on\nRead child\n*FvwmDesktop: motion off\nExec SECRET-SOURCE\n"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "child"), []byte("*FvwmDesktop: appearance dark\n"), 0600); err != nil {
		t.Fatal(err)
	}
	result, err := Collect(context.Background(), r, fixtureQuery(t, nil))
	if err != nil {
		t.Fatal(err)
	}
	if len(result.Sections) != 6 {
		t.Fatal(result)
	}
	for _, s := range result.Sections {
		if len(s.Rows) == 0 || len(s.Rows) > RowLimit || s.State == "unavailable" {
			t.Fatalf("%+v", s)
		}
	}
	text := textResult(result)
	for _, secret := range []string{"SECRET-SENTINEL", "SECRET-WINDOW", "SECRET-FUNCTION", "SECRET-ARGS", "SECRET-SOURCE", root} {
		if strings.Contains(text, secret) {
			t.Fatalf("leaked %s", secret)
		}
	}
	for _, want := range []string{"Policy role menu.normal", "Observed colorset 2", "pending", "Observed menu fixture-menu", "Observed decoration default", "menu-fallback Key F1", "placement button_count", "context=8(root)", "modifiers=4(control)", "Exec (arguments hidden)", "user action (hidden)", "scale=6/4", "pages=3x2", "unknown (cached work area unavailable)", "high-contrast", "resolved=auto", "child:1", "includes=1", "not an atomic"} {
		if !strings.Contains(text, want) {
			t.Errorf("missing %s", want)
		}
	}
	data, _ := json.Marshal(result)
	if _, err := DecodeResult(data); err != nil {
		t.Fatal(err)
	}
}
func TestMissingMFLRetainsLocalPolicyAndSourceResults(t *testing.T) {
	r := fixtureRequest(t, "all")
	result, err := Collect(context.Background(), r, nil)
	if err != nil {
		t.Fatal(err)
	}
	for _, s := range result.Sections {
		switch s.ID {
		case "settings", "commands":
			if len(s.Rows) == 0 {
				t.Fatal(s)
			}
		case "theme":
			if len(s.Rows) == 0 || s.State != "partial" {
				t.Fatal(s)
			}
		case "bindings", "layout", "sources":
			if s.State != "unavailable" || s.Reason == "" {
				t.Fatal(s)
			}
		}
	}
}
func TestBindingPaginationGenerationAndNoFalseCompletion(t *testing.T) {
	for _, behavior := range []string{"complete", "changed", "repeat", "oversized", "backend-error"} {
		t.Run(behavior, func(t *testing.T) {
			pages := 0
			query := fixtureQuery(t, func(req map[string]json.RawMessage, m map[string]any) {
				if stringField(req, "resource") != "bindings" || stringField(req, "scope") != "global" {
					return
				}
				pages++
				offset, _ := intField(req, "offset")
				m["offset"] = offset
				if offset == 0 {
					m["next_offset"] = 2
				} else if stringField(req, "generation") != fixtureGeneration {
					t.Fatal("missing generation continuation")
				}
				switch behavior {
				case "changed":
					if offset > 0 {
						m["generation"] = strings.Repeat("2", 48)
					}
				case "repeat":
					m["next_offset"] = offset
				case "oversized":
					m["bindings"] = make([]any, 17)
				case "backend-error":
					m["state"] = "invalid"
				}
			})
			result, err := Collect(context.Background(), Request{Scope: "bindings"}, query)
			if err != nil {
				t.Fatal(err)
			}
			s := result.Sections[0]
			if behavior == "complete" {
				if s.State != "observed" || pages != 2 {
					t.Fatalf("%s %d %+v", behavior, pages, s)
				}
			} else if s.State != "partial" {
				t.Fatalf("%s %+v", behavior, s)
			}
		})
	}
}
func TestFiniteQueryWorkAndRows(t *testing.T) {
	calls := 0
	query := fixtureQuery(t, func(req map[string]json.RawMessage, m map[string]any) {
		calls++
		resource, scope := stringField(req, "resource"), stringField(req, "scope")
		property := map[string]string{"colorsets": "colorsets", "menu-styles": "menu_styles", "decorations": "decorations", "bindings": "bindings"}[resource]
		if property == "" || scope == "policy" || scope == "placement" {
			return
		}
		offset, _ := intField(req, "offset")
		m["offset"] = offset
		original := m[property].([]any)
		items := []any{}
		for len(items) < 16 {
			items = append(items, original[0])
		}
		m[property] = items
		m["next_offset"] = offset + 16
	})
	result, err := Collect(context.Background(), fixtureRequest(t, "all"), query)
	if err != nil {
		t.Fatal(err)
	}
	if calls > 40 {
		t.Fatal(calls)
	}
	for _, s := range result.Sections {
		if len(s.Rows) > 128 {
			t.Fatal(s.ID)
		}
		if member(s.ID, []string{"theme", "bindings"}) && (!s.Truncated || s.State != "partial") {
			t.Fatal(s)
		}
	}
}
func TestErrorTextAndHomePathsNeverCopied(t *testing.T) {
	query := func(context.Context, string) (json.RawMessage, error) {
		return nil, errors.New("SECRET-TRANSPORT /home/secret")
	}
	result, err := Collect(context.Background(), Request{Scope: "bindings"}, query)
	if err != nil {
		t.Fatal(err)
	}
	if strings.Contains(textResult(result), "SECRET") || strings.Contains(textResult(result), "/home/") {
		t.Fatal(result)
	}
	home, _ := os.UserHomeDir()
	r := Request{Scope: "layout"}
	result, err = Collect(context.Background(), r, fixtureQuery(t, func(req map[string]json.RawMessage, m map[string]any) {
		if stringField(req, "resource") == "topology" {
			top := m["topology"].(map[string]any)
			top["monitors"].([]any)[0].(map[string]any)["name"] = home + "/monitor"
		}
	}))
	if err != nil {
		t.Fatal(err)
	}
	if home != "" && strings.Contains(textResult(result), home) {
		t.Fatal("home leaked")
	}
}
func TestOwnerGenerationMismatchAndProjectionGap(t *testing.T) {
	r := fixtureRequest(t, "settings")
	r.SettingsGeneration = "changed"
	result, err := Collect(context.Background(), r, nil)
	if err != nil {
		t.Fatal(err)
	}
	if result.Sections[0].State != "unavailable" {
		t.Fatal(result)
	}
	r = fixtureRequest(t, "theme")
	r.Colors = nil
	r.Appearance = ""
	result, err = Collect(context.Background(), r, fixtureQuery(t, nil))
	if err != nil {
		t.Fatal(err)
	}
	if result.Sections[0].State != "partial" || !strings.Contains(result.Sections[0].Reason, "projection unavailable") {
		t.Fatal(result)
	}
}
func TestCanceledQueryLeavesSettingsUsable(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	result, err := Collect(ctx, fixtureRequest(t, "all"), func(context.Context, string) (json.RawMessage, error) {
		t.Fatal("query after cancellation")
		return nil, nil
	})
	if err != nil {
		t.Fatal(err)
	}
	if result.Sections[0].State != "provider-policy" || len(result.Sections[0].Rows) == 0 {
		t.Fatal(result)
	}
}
func TestCommandsFilterSearchesWholeInventory(t *testing.T) {
	result, err := Collect(context.Background(), Request{Scope: "commands", Filter: "WindowStyle"}, nil)
	if err != nil {
		t.Fatal(err)
	}
	if result.Filter != "WindowStyle" || !strings.Contains(textResult(result), "WindowStyle") {
		t.Fatal(fmt.Sprint(result))
	}
}

func TestMalformedBindingTextNeverLabelsAnObservedCompleteList(t *testing.T) {
	for _, field := range []string{"key", "action", "window"} {
		t.Run(field, func(t *testing.T) {
			result, err := Collect(context.Background(), Request{Scope: "bindings"}, fixtureQuery(t, func(req map[string]json.RawMessage, m map[string]any) {
				if stringField(req, "resource") == "bindings" && stringField(req, "scope") == "global" {
					m["bindings"].([]any)[0].(map[string]any)[field] = 123
				}
			}))
			if err != nil {
				t.Fatal(err)
			}
			if result.Sections[0].State != "partial" {
				t.Fatal(result)
			}
		})
	}
}

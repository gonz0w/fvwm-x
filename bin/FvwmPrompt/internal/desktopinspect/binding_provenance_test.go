// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopinspect

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"
	"testing"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopcheck"
)

func fixtureProvenance(scope string, position int, source any) map[string]any {
	selection := "window-specific-before-global"
	if scope != "global" {
		selection = "menu-before-fallback"
	}
	return map[string]any{"version": 1, "source": source, "replaced_existing": false,
		"position": position, "selection": selection, "effective": "requires-event-context"}
}
func fixtureOrigin(kind, path string) map[string]any {
	source := map[string]any{"version": 1, "kind": kind}
	if path != "" {
		source["path"], source["first"], source["last"] = path, 2, 4
		source["include"] = []any{map[string]any{"path": "/fixture/root", "first": 7, "last": 9}}
	}
	return source
}
func TestBindingProvenanceKindsAndPrivacy(t *testing.T) {
	for _, kind := range []string{"file", "generated", "invocation", "unavailable"} {
		t.Run(kind, func(t *testing.T) {
			req := Request{Scope: "bindings", Source: desktopcheck.Request{Path: "config", Userdir: "/fixture", Datadir: "/data", Cwd: "/fixture"}}
			result, err := Collect(context.Background(), req, fixtureQuery(t, func(request map[string]json.RawMessage, m map[string]any) {
				scope := stringField(request, "scope")
				if stringField(request, "resource") != "bindings" || !member(scope, []string{"global", "menu", "menu-fallback"}) {
					return
				}
				source := fixtureOrigin(kind, "/fixture/配置")
				if kind == "unavailable" {
					source["reason"] = "binding source retention budget exhausted"
				}
				for i, value := range m["bindings"].([]any) {
					value.(map[string]any)["provenance"] = fixtureProvenance(scope, i, source)
				}
				m["bindings"].([]any)[0].(map[string]any)["provenance"].(map[string]any)["replaced_existing"] = true
			}))
			if err != nil {
				t.Fatal(err)
			}
			text := textResult(result)
			for _, secret := range []string{"SECRET", "/fixture", "Exec touch", "historical override"} {
				if strings.Contains(text, secret) {
					t.Fatal("private or invented data", text)
				}
			}
			for _, want := range []string{"same identity matched at definition", "position=0", "window-specific before global", "menu before fallback", "requires event context"} {
				if !strings.Contains(text, want) {
					t.Fatal("missing current-definition fact", want, text)
				}
			}
			switch kind {
			case "file":
				for _, want := range []string{"literal definition", "user:配置:2-4", "user:root:7-9"} {
					if !strings.Contains(text, want) {
						t.Fatal(want, text)
					}
				}
			case "generated", "invocation":
				if !strings.Contains(text, "enclosing context") || !strings.Contains(text, "exact definition unavailable") || strings.Contains(text, "literal definition") {
					t.Fatal(text)
				}
			case "unavailable":
				if result.Sections[0].State != "partial" || !strings.Contains(text, "retention budget exhausted") || strings.Contains(text, "user:配置") {
					t.Fatal(text)
				}
			}
		})
	}
}
func TestBindingProvenanceRejectsMalformedAndAbsentMetadata(t *testing.T) {
	valid := `{"version":1,"source":{"version":1,"kind":"file","path":"/fixture/config","first":2,"last":4},"replaced_existing":false,"position":0,"selection":"window-specific-before-global","effective":"requires-event-context"}`
	for name, raw := range map[string]string{
		"absent": "", "null": "null", "array": "[]",
		"duplicate":     strings.Replace(string(valid), `"version":1`, `"version":1,"version":1`, 1),
		"unknown":       strings.Replace(string(valid), `"version":1`, `"extra":1,"version":1`, 1),
		"missing":       strings.Replace(string(valid), `"version":1,`, "", 1),
		"bad-version":   strings.Replace(string(valid), `"version":1`, `"version":1.0`, 1),
		"bad-position":  strings.Replace(string(valid), `"position":0`, `"position":1`, 1),
		"null-replaced": strings.Replace(string(valid), `"replaced_existing":false`, `"replaced_existing":null`, 1),
		"winner":        strings.Replace(string(valid), "requires-event-context", "winner", 1),
		"selection":     strings.Replace(string(valid), "window-specific-before-global", "menu-before-fallback", 1),
		"unicode":       strings.Replace(string(valid), "/fixture/config", `/fixture/\ud800`, 1),
		"control":       strings.Replace(string(valid), "/fixture/config", `/fixture/\n`, 1),
		"relative":      strings.Replace(string(valid), "/fixture/config", "relative", 1),
	} {
		t.Run(name, func(t *testing.T) {
			c := collector{req: Request{Source: desktopcheck.Request{Userdir: "/fixture"}}}
			s := Section{State: "observed", Rows: []Row{{"binding", "action hidden"}}}
			item := map[string]json.RawMessage{}
			if raw != "" {
				item["provenance"] = json.RawMessage(raw)
			}
			if err := c.bindingProvenance(&s, item, "global", "global Key F12", 0); err != nil {
				t.Fatal(err)
			}
			if !strings.Contains(s.Rows[1].Label, "global Key F12") || s.State != "partial" || len(s.Rows) != 2 || !strings.Contains(s.Rows[1].Value, "unavailable") || strings.Contains(s.Rows[1].Value, "/fixture") {
				t.Fatal(s)
			}
		})
	}
}
func TestBindingLocationsContinueAndRedactUnknownRoots(t *testing.T) {
	path := "/fixture/" + strings.Repeat("配置", 160)
	source := fixtureOrigin("file", path)
	source["include"] = []any{map[string]any{"path": "/private/SECRET/root", "first": 1, "last": 1}}
	raw, _ := json.Marshal(fixtureProvenance("global", 0, source))
	c := collector{req: Request{Source: desktopcheck.Request{Userdir: "/fixture"}}}
	s := Section{State: "observed"}
	if err := c.bindingProvenance(&s, map[string]json.RawMessage{"provenance": raw}, "global", "global Key F12", 0); err != nil {
		t.Fatal(err)
	}
	var joined strings.Builder
	for _, row := range s.Rows {
		if !strings.Contains(row.Label, "global Key F12") || utf8.RuneCountInString(row.Value) > 200 {
			t.Fatal(row)
		}
		if strings.Contains(row.Label, "definition ") {
			joined.WriteString(row.Value)
		}
	}
	if joined.String() != "user:"+strings.Repeat("配置", 160)+":2-4" || s.Truncated {
		t.Fatal("location lost", joined.String(), s)
	}
	encoded, _ := json.Marshal(s)
	if strings.Contains(string(encoded), "SECRET") || !strings.Contains(string(encoded), "outside:(hidden):1-1") {
		t.Fatal(string(encoded))
	}
}
func TestBindingSourceRowExhaustionHasReservedMarker(t *testing.T) {
	raw, _ := json.Marshal(fixtureProvenance("global", 0, fixtureOrigin("file", "/fixture/"+strings.Repeat("p", 600))))
	c := collector{req: Request{Source: desktopcheck.Request{Userdir: "/fixture"}}}
	for _, used := range []int{RowLimit - 5, RowLimit - 2, RowLimit - 1} {
		s := Section{State: "observed", Rows: make([]Row, used)}
		err := c.bindingProvenance(&s, map[string]json.RawMessage{"provenance": raw}, "global", "global Key F12", 0)
		if !strings.Contains(s.Rows[len(s.Rows)-1].Label, "global Key F12") || err == nil || !s.Truncated || s.State != "partial" || len(s.Rows) > RowLimit || !strings.Contains(s.Rows[len(s.Rows)-1].Value, "Source display incomplete") {
			t.Fatal(used, err, s)
		}
	}
}
func TestBindingUnavailableReasonDoesNotLeakProviderText(t *testing.T) {
	source := fixtureOrigin("unavailable", "")
	source["reason"] = "cannot open /private/SECRET"
	raw, _ := json.Marshal(fixtureProvenance("global", 0, source))
	c := collector{}
	s := Section{State: "observed"}
	if err := c.bindingProvenance(&s, map[string]json.RawMessage{"provenance": raw}, "global", "global Key F12", 0); err != nil {
		t.Fatal(err)
	}
	if s.State != "partial" || strings.Contains(fmt.Sprint(s), "SECRET") || !strings.Contains(fmt.Sprint(s), "details hidden") {
		t.Fatal(s)
	}
}

// Key filtering must retain metadata through the binding identity, independent
// of source filenames. Exercise the actual final per-row collector filter.
func TestBindingFilterRetainsRelatedProvenance(t *testing.T) {
	for _, suffix := range []string{"", strings.Repeat("leaf", 120)} {
		req := Request{Scope: "bindings", Filter: "f12", Source: desktopcheck.Request{
			Path: "entry", Userdir: "/fixture", Datadir: "/data", Cwd: "/fixture",
		}}
		result, err := Collect(context.Background(), req, fixtureQuery(t, func(request map[string]json.RawMessage, m map[string]any) {
			scope := stringField(request, "scope")
			if stringField(request, "resource") != "bindings" || !member(scope, []string{"global", "menu", "menu-fallback"}) {
				return
			}
			source := fixtureOrigin("file", "/fixture/leaf"+suffix)
			source["include"] = []any{map[string]any{"path": "/fixture/entry", "first": 7, "last": 9}}
			item := m["bindings"].([]any)[0].(map[string]any)
			item["key"] = "F12"
			item["provenance"] = fixtureProvenance(scope, 0, source)
		}))
		if err != nil {
			t.Fatal(err)
		}
		section := result.Sections[0]
		if section.State != "observed" || section.Truncated {
			t.Fatal(section)
		}
		text := textResult(result)
		for _, want := range []string{"position=0", "window-specific before global", "menu before fallback",
			"requires event context", "same identity matched at definition=false", "literal definition", "user:entry:7-9"} {
			if !strings.Contains(text, want) {
				t.Fatal("filtered binding metadata lost", want, text)
			}
		}
		for _, scope := range []string{"global", "menu", "menu-fallback"} {
			var location strings.Builder
			foundInclude := false
			for _, row := range section.Rows {
				if !strings.Contains(row.Label, "Key F12") {
					t.Fatal("unrelated filtered row", row)
				}
				if strings.HasPrefix(row.Label, scope+" Key F12 #0 definition ") {
					location.WriteString(row.Value)
				}
				if strings.HasPrefix(row.Label, scope+" Key F12 #0 include ") && row.Value == "user:entry:7-9" {
					foundInclude = true
				}
			}
			if location.String() != "user:leaf"+suffix+":2-4" || !foundInclude {
				t.Fatal("filtered source/span/include lost", scope, section)
			}
		}
		for _, secret := range []string{"SECRET", "/fixture", "Exec touch"} {
			if strings.Contains(text, secret) {
				t.Fatal("filtered binding leaked private fields", secret, text)
			}
		}
	}
}

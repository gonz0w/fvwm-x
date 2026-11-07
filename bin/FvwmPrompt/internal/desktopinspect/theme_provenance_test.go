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

func fixtureThemeProvenance(basis, operation, retention string, source any) map[string]any {
	return map[string]any{"version": 1, "basis": basis, "source": source,
		"operation": operation, "retention": retention,
		"replaces_definition": false, "copied_from": nil}
}

func TestThemeProvenanceCombinationContract(t *testing.T) {
	for _, basis := range []string{"builtin-default", "last-definition", "runtime-preview", "unknown"} {
		for _, operation := range []string{"builtin-default", "merge", "copy", "reset", "preview", "unknown"} {
			for _, retention := range []string{"not-applicable", "retained", "unavailable", "budget-exhausted", "unknown"} {
				for _, kind := range []string{"unavailable", "file", "generated", "invocation"} {
					for _, replaced := range []bool{false, true} {
						for _, donor := range []any{nil, "*"} {
							m := fixtureThemeProvenance(basis, operation, retention, fixtureOrigin(kind, ""))
							if kind != "unavailable" {
								m["source"] = fixtureOrigin(kind, "/fixture/config")
							}
							m["replaces_definition"], m["copied_from"] = replaced, donor
							data, _ := json.Marshal(m)
							_, err := decodeThemeProvenance(data)
							want := basis == "builtin-default" && operation == "builtin-default" && retention == "not-applicable" && kind == "unavailable" && !replaced && donor == nil
							want = want || basis == "runtime-preview" && operation == "preview" && retention == "not-applicable" && kind == "unavailable" && donor == nil
							if basis == "last-definition" && member(operation, []string{"merge", "copy", "reset"}) {
								retained := retention == "retained"
								lost := member(retention, []string{"unavailable", "budget-exhausted"}) && kind == "unavailable"
								donorOK := donor == nil
								if operation == "copy" {
									donorOK = donor != nil || lost
								}
								want = (retained || lost) && donorOK
							}
							if (err == nil) != want {
								t.Fatalf("unexpected combination acceptance %s: want %t, error %v", data, want, err)
							}
						}
					}
				}
			}
		}
	}
}

func TestThemeProvenanceRejectsMalformedMetadata(t *testing.T) {
	valid := `{"version":1,"basis":"last-definition","source":{"version":1,"kind":"file","path":"/fixture/config","first":2,"last":4},"operation":"copy","retention":"retained","replaces_definition":true,"copied_from":"donor"}`
	cases := map[string]string{
		"absent": "", "null": "null", "array": "[]",
		"unknown":            strings.Replace(valid, `"basis":`, `"extra":"SECRET","basis":`, 1),
		"missing":            strings.Replace(valid, `,"copied_from":"donor"`, ``, 1),
		"duplicate":          strings.Replace(valid, `"version":1`, `"version":1,"version":1`, 1),
		"version":            strings.Replace(valid, `"version":1`, `"version":1.0`, 1),
		"version-type":       strings.Replace(valid, `"version":1`, `"version":"1"`, 1),
		"replacement-null":   strings.Replace(valid, `"replaces_definition":true`, `"replaces_definition":null`, 1),
		"replacement-type":   strings.Replace(valid, `"replaces_definition":true`, `"replaces_definition":1`, 1),
		"donor-null":         strings.Replace(valid, `"copied_from":"donor"`, `"copied_from":null`, 1),
		"donor-type":         strings.Replace(valid, `"copied_from":"donor"`, `"copied_from":true`, 1),
		"donor-empty":        strings.Replace(valid, `"copied_from":"donor"`, `"copied_from":""`, 1),
		"donor-unicode":      strings.Replace(valid, `"copied_from":"donor"`, `"copied_from":"\ud800"`, 1),
		"donor-invalid-utf8": strings.Replace(valid, `"copied_from":"donor"`, `"copied_from":"`+string([]byte{0xff})+`"`, 1),
		"donor-control":      strings.Replace(valid, `"copied_from":"donor"`, `"copied_from":"\nSECRET"`, 1),
		"donor-long-bytes":   strings.Replace(valid, `"copied_from":"donor"`, `"copied_from":"`+strings.Repeat("界", 86)+`"`, 1),
		"basis-null":         strings.Replace(valid, `"basis":"last-definition"`, `"basis":null`, 1),
		"operation-type":     strings.Replace(valid, `"operation":"copy"`, `"operation":[]`, 1),
		"retention-type":     strings.Replace(valid, `"retention":"retained"`, `"retention":1`, 1),
		"source-relative":    strings.Replace(valid, `/fixture/config`, `relative`, 1),
		"source-missing":     strings.Replace(valid, `,"first":2`, ``, 1),
		"source-unknown":     strings.Replace(valid, `"kind":"file"`, `"kind":"file","secret":"SECRET"`, 1),
		"source-unicode":     strings.Replace(valid, `/fixture/config`, `/fixture/\ud800`, 1),
		"source-control":     strings.Replace(valid, `/fixture/config`, `/fixture/\nSECRET`, 1),
	}
	for name, raw := range cases {
		t.Run(name, func(t *testing.T) {
			if _, err := decodeThemeProvenance([]byte(raw)); err == nil {
				t.Fatal("malformed theme metadata accepted", raw)
			}
			c := collector{}
			s := Section{State: "observed", Rows: []Row{{"Observed colorset 2", "pending"}}}
			item := map[string]json.RawMessage{}
			if raw != "" {
				item["provenance"] = json.RawMessage(raw)
			}
			if err := c.themeProvenance(&s, item, "Observed colorset 2"); err != nil {
				t.Fatal(err)
			}
			if s.State != "partial" || len(s.Rows) != 2 || !strings.Contains(s.Rows[1].Value, "unavailable") || strings.Contains(fmt.Sprint(s), "SECRET") {
				t.Fatal(s)
			}
		})
	}
}

func TestThemeRowsDescribeLatestAggregateAndHidePrivateSources(t *testing.T) {
	for _, operation := range []string{"merge", "copy", "reset"} {
		for _, kind := range []string{"file", "generated", "invocation", "unavailable"} {
			t.Run(operation+"/"+kind, func(t *testing.T) {
				c := collector{req: Request{Source: desktopcheck.Request{Userdir: "/fixture"}}}
				s := Section{State: "observed", Rows: []Row{}}
				source := fixtureOrigin(kind, "/fixture/配置")
				source["reason"] = "private /SECRET/source"
				m := fixtureThemeProvenance("last-definition", operation, "retained", source)
				m["replaces_definition"] = true
				if operation == "copy" {
					m["copied_from"] = "donor-menu"
				}
				raw, _ := json.Marshal(m)
				if err := c.themeProvenance(&s, map[string]json.RawMessage{"provenance": raw}, "Observed menu target"); err != nil {
					t.Fatal(err)
				}
				text := fmt.Sprint(s)
				for _, secret := range []string{"SECRET", "/fixture", "last value change", "per-field owner", "event winner"} {
					if strings.Contains(text, secret) {
						t.Fatal("private or invented data", text)
					}
				}
				for _, want := range []string{"latest aggregate definition", "untouched fields may retain earlier definitions", "replaces earlier definition=true", operation} {
					if !strings.Contains(text, want) {
						t.Fatal("missing definition fact", want, text)
					}
				}
				if operation == "copy" && (!strings.Contains(text, "donor-menu") || !strings.Contains(text, "copy invocation") || !strings.Contains(text, "no transitive history")) {
					t.Fatal(text)
				}
				switch kind {
				case "file":
					if !strings.Contains(text, "user:配置:2-4") || !strings.Contains(text, "user:root:7-9") || s.State != "observed" {
						t.Fatal(text)
					}
				case "generated", "invocation":
					if !strings.Contains(text, "enclosing context only") || !strings.Contains(text, "exact definition unavailable") {
						t.Fatal(text)
					}
				case "unavailable":
					if s.State != "partial" || strings.Contains(text, "user:配置") || !strings.Contains(text, "details hidden") {
						t.Fatal(text)
					}
				}
			})
		}
	}
}

func TestThemeDefaultsPreviewAndRetentionGaps(t *testing.T) {
	for _, test := range []struct{ basis, operation, retention, want string }{
		{"builtin-default", "builtin-default", "not-applicable", "builtin-default; no configuration source"},
		{"runtime-preview", "preview", "not-applicable", "runtime preview; configuration source unavailable"},
		{"last-definition", "merge", "unavailable", "reported source unavailable; details hidden"},
		{"last-definition", "copy", "budget-exhausted", "theme source retention budget exhausted"},
	} {
		t.Run(test.basis+"/"+test.retention, func(t *testing.T) {
			m := fixtureThemeProvenance(test.basis, test.operation, test.retention, fixtureOrigin("unavailable", "/SECRET/site"))
			raw, _ := json.Marshal(m)
			c := collector{}
			s := Section{State: "observed", Rows: []Row{}}
			if err := c.themeProvenance(&s, map[string]json.RawMessage{"provenance": raw}, "Observed colorset 2"); err != nil {
				t.Fatal(err)
			}
			text := fmt.Sprint(s)
			if !strings.Contains(text, test.want) || strings.Contains(text, "SECRET") {
				t.Fatal(text)
			}
			wantState := "observed"
			if test.basis == "last-definition" {
				wantState = "partial"
			}
			if s.State != wantState || test.operation == "copy" && !strings.Contains(text, "donor unavailable") {
				t.Fatal(s)
			}
		})
	}
}

func TestThemeInventoryFairRowsPrivacyContinuationsAndFilters(t *testing.T) {
	for _, filter := range []string{"", "Observed colorset 2", "Observed menu fixture-menu", "Observed decoration default"} {
		t.Run(filter, func(t *testing.T) {
			req := fixtureRequest(t, "theme")
			req.Filter = filter
			req.Source = desktopcheck.Request{Path: "config", Userdir: "/fixture", Datadir: "/data", Cwd: "/fixture"}
			calls := 0
			result, err := Collect(context.Background(), req, fixtureQuery(t, func(request map[string]json.RawMessage, m map[string]any) {
				calls++
				resource := stringField(request, "resource")
				property := map[string]string{"colorsets": "colorsets", "menu-styles": "menu_styles", "decorations": "decorations"}[resource]
				if property == "" {
					return
				}
				if resource != "colorsets" && stringField(request, "generation") != "" && stringField(request, "generation") != fixtureGeneration {
					t.Fatal("appearance generation guard changed")
				}
				original := m[property].([]any)[0].(map[string]any)
				source := fixtureOrigin("file", "/fixture/"+strings.Repeat("long", 130))
				source["include"] = []any{map[string]any{"path": "/private/SECRET/root", "first": 8, "last": 9}}
				original["provenance"] = fixtureThemeProvenance("last-definition", "merge", "retained", source)
				items := []any{}
				for i := 0; i < 16; i++ {
					items = append(items, original)
				}
				offset, _ := intField(request, "offset")
				m[property], m["offset"], m["next_offset"] = items, offset, offset+16
			}))
			if err != nil {
				t.Fatal(err)
			}
			s := result.Sections[0]
			if len(s.Rows) > RowLimit || !s.Truncated || s.State != "partial" || calls > 10 {
				t.Fatal("unbounded or falsely complete inventory", s, calls)
			}
			for _, prefix := range []string{"Observed colorset 2", "Observed menu fixture-menu", "Observed decoration default"} {
				if filter != "" && filter != prefix {
					continue
				}
				var path strings.Builder
				basis, definition, include := false, false, false
				for _, row := range s.Rows {
					if utf8.RuneCountInString(row.Value) > 200 || utf8.RuneCountInString(row.Label) > 80 {
						t.Fatal(row)
					}
					if strings.HasPrefix(row.Label, prefix+" definition ") && path.Len() < len("user:"+strings.Repeat("long", 130)+":2-4") {
						path.WriteString(row.Value)
					}
					basis = basis || row.Label == prefix+" basis"
					definition = definition || row.Label == prefix
					include = include || strings.HasPrefix(row.Label, prefix+" include ") && row.Value == "outside:(hidden):8-9"
				}
				if path.String() != "user:"+strings.Repeat("long", 130)+":2-4" || !basis || !definition || !include {
					t.Fatal("domain source lost under pressure", prefix, s)
				}
			}
			if strings.Contains(textResult(result), "SECRET") || strings.Contains(textResult(result), "/fixture") {
				t.Fatal("source privacy lost", result)
			}
			if filter == "" && !strings.Contains(textResult(result), "Theme inventory incomplete") {
				t.Fatal("row pressure hidden", result)
			}
		})
	}
}

func TestThemeMissingAndMalformedMetadataKeepDefinitions(t *testing.T) {
	for _, behavior := range []string{"absent", "malformed"} {
		t.Run(behavior, func(t *testing.T) {
			result, err := Collect(context.Background(), fixtureRequest(t, "theme"), fixtureQuery(t, func(request map[string]json.RawMessage, m map[string]any) {
				property := map[string]string{"colorsets": "colorsets", "menu-styles": "menu_styles", "decorations": "decorations"}[stringField(request, "resource")]
				if property != "" {
					for _, value := range m[property].([]any) {
						item := value.(map[string]any)
						delete(item, "provenance")
						if behavior == "malformed" {
							item["provenance"] = true
						}
					}
				}
			}))
			if err != nil || result.Sections[0].State != "partial" {
				t.Fatal(result, err)
			}
			text := textResult(result)
			for _, want := range []string{"Observed colorset 2", "Observed menu fixture-menu", "Observed decoration default", map[string]string{"absent": "producer supplied no metadata", "malformed": "malformed metadata"}[behavior]} {
				if !strings.Contains(text, want) {
					t.Fatal(want, result)
				}
			}
		})
	}
}

func TestThemeDonorAndLongIdentityContinuations(t *testing.T) {
	for _, donor := range []string{strings.Repeat("x", 256), strings.Repeat("界", 85), "\U0001f600", `literal\uD800`} {
		m := fixtureThemeProvenance("last-definition", "copy", "retained", fixtureOrigin("file", "/fixture/config"))
		m["copied_from"] = donor
		raw, _ := json.Marshal(m)
		c := collector{req: Request{Source: desktopcheck.Request{Userdir: "/fixture"}}}
		s := Section{State: "observed", Rows: []Row{}}
		prefix := themeRowPrefix("Observed menu " + strings.Repeat("長", 90))
		if err := c.themeProvenance(&s, map[string]json.RawMessage{"provenance": raw}, prefix); err != nil {
			t.Fatal(err)
		}
		var joined strings.Builder
		for _, row := range s.Rows {
			if utf8.RuneCountInString(row.Label) > 80 || utf8.RuneCountInString(row.Value) > 200 {
				t.Fatal("continuation counter clipped", row)
			}
			if strings.HasPrefix(row.Label, prefix+" donor ") {
				joined.WriteString(row.Value)
			}
		}
		if joined.String() != donor || !strings.HasSuffix(prefix, "...") {
			t.Fatal("donor identity changed or name clipping hidden", s)
		}
	}
	// Escaped supplementary Unicode is valid, whereas either unpaired half is
	// refused. A literal backslash-u sequence is an ordinary donor name.
	for _, raw := range []string{`"\ud83d\ude00"`, `"escaped\/slash"`, `"literal\\ud800"`} {
		if !themeDonorEscapes([]byte(raw)) {
			t.Fatal("valid donor escape rejected", raw)
		}
	}
	for _, raw := range []string{`"\ud800"`, `"\udc00"`, `"\ud800\u0041"`} {
		if themeDonorEscapes([]byte(raw)) {
			t.Fatal("unpaired donor surrogate accepted", raw)
		}
	}
}

func TestThemeSourcePressureLeavesIncompleteMarker(t *testing.T) {
	m := fixtureThemeProvenance("last-definition", "merge", "retained", fixtureOrigin("file", "/fixture/"+strings.Repeat("p", 600)))
	raw, _ := json.Marshal(m)
	c := collector{req: Request{Source: desktopcheck.Request{Userdir: "/fixture"}}}
	for _, used := range []int{RowLimit - 5, RowLimit - 2, RowLimit - 1} {
		s := Section{State: "observed", Rows: make([]Row, used)}
		if err := c.themeProvenance(&s, map[string]json.RawMessage{"provenance": raw}, "Observed menu target"); err == nil {
			t.Fatal("source exhaustion claimed complete", used)
		}
		if !s.Truncated || s.State != "partial" || len(s.Rows) > RowLimit || !strings.Contains(s.Rows[len(s.Rows)-1].Value, "Theme source display incomplete") {
			t.Fatal(used, s)
		}
	}
}

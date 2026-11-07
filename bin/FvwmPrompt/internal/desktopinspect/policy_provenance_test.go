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

func fixturePolicyEntry(basis, effect, retention string, source any) map[string]any {
	return map[string]any{"basis": basis, "effect": effect, "retention": retention, "source": source}
}
func fixturePolicyProvenance(groups ...string) map[string]any {
	m := map[string]any{"version": 1}
	for _, group := range groups {
		m[group] = fixturePolicyEntry("builtin-default", "builtin-default", "not-applicable", fixtureOrigin("unavailable", ""))
	}
	return m
}

func TestPolicyProvenanceCombinationContract(t *testing.T) {
	for _, group := range []string{"ignored_modifiers", "drag", "interactive"} {
		for _, basis := range []string{"builtin-default", "last-value-change", "unknown"} {
			for _, effect := range []string{"builtin-default", "applied", "parse-error", "unknown"} {
				for _, retention := range []string{"not-applicable", "retained", "unavailable", "budget-exhausted", "unknown"} {
					for _, kind := range []string{"unavailable", "file", "generated", "invocation"} {
						name := strings.Join([]string{group, basis, effect, retention, kind}, "/")
						t.Run(name, func(t *testing.T) {
							path := ""
							if kind != "unavailable" {
								path = "/fixture/config"
							}
							m := map[string]any{"version": 1, group: fixturePolicyEntry(basis, effect, retention, fixtureOrigin(kind, path))}
							raw, _ := json.Marshal(m)
							_, err := decodePolicyProvenance(raw, group)
							want := basis == "builtin-default" && effect == "builtin-default" && retention == "not-applicable" && kind == "unavailable"
							if basis == "last-value-change" && (effect == "applied" || effect == "parse-error" && group == "ignored_modifiers") {
								want = retention == "retained" || member(retention, []string{"unavailable", "budget-exhausted"}) && kind == "unavailable"
							}
							if (err == nil) != want {
								t.Fatal("unexpected tuple acceptance", string(raw), want, err)
							}
						})
					}
				}
			}
		}
	}
}

func TestPolicyProvenanceRejectsMalformedMetadata(t *testing.T) {
	valid := `{"version":1,"ignored_modifiers":{"basis":"last-value-change","source":{"version":1,"kind":"file","path":"/fixture/config","first":2,"last":4},"effect":"applied","retention":"retained"}}`
	cases := map[string]string{
		"absent": "", "null": "null", "array": "[]", "missing-group": `{"version":1}`,
		"root-unknown":    strings.Replace(valid, `"version":1`, `"extra":1,"version":1`, 1),
		"duplicate":       strings.Replace(valid, `"version":1`, `"version":1,"version":1`, 1),
		"version":         strings.Replace(valid, `"version":1`, `"version":1.0`, 1),
		"version-type":    strings.Replace(valid, `"version":1`, `"version":"1"`, 1),
		"entry-unknown":   strings.Replace(valid, `"basis":`, `"extra":true,"basis":`, 1),
		"entry-missing":   strings.Replace(valid, `"retention":"retained"`, `"extra":"retained"`, 1),
		"entry-null":      strings.Replace(valid, `"ignored_modifiers":{`, `"ignored_modifiers":null,"bad":{`, 1),
		"basis-type":      strings.Replace(valid, `"basis":"last-value-change"`, `"basis":true`, 1),
		"effect-null":     strings.Replace(valid, `"effect":"applied"`, `"effect":null`, 1),
		"retention-type":  strings.Replace(valid, `"retention":"retained"`, `"retention":[]`, 1),
		"source-unknown":  strings.Replace(valid, `"kind":"file"`, `"kind":"file","secret":"SECRET"`, 1),
		"source-missing":  strings.Replace(valid, `"first":2,`, ``, 1),
		"source-null":     strings.Replace(valid, `"path":"/fixture/config"`, `"path":null`, 1),
		"source-relative": strings.Replace(valid, `/fixture/config`, `relative`, 1),
		"source-unicode":  strings.Replace(valid, `/fixture/config`, `/fixture/\ud800`, 1),
		"source-control":  strings.Replace(valid, `/fixture/config`, `/fixture/\n`, 1),
	}
	for name, raw := range cases {
		t.Run(name, func(t *testing.T) {
			if _, err := decodePolicyProvenance([]byte(raw), "ignored_modifiers"); err == nil {
				t.Fatal("malformed policy accepted", raw)
			}
			c := collector{}
			s := Section{State: "observed", Rows: []Row{{"value", "2"}}}
			item := map[string]json.RawMessage{}
			if raw != "" {
				item["provenance"] = json.RawMessage(raw)
			}
			c.policyProvenance(&s, item, "ignored_modifiers")
			if s.State != "partial" || len(s.Rows) != 2 || !strings.Contains(s.Rows[1].Value, "unavailable") || strings.Contains(fmt.Sprint(s), "SECRET") {
				t.Fatal(s)
			}
		})
	}
	// Placement metadata is one fixed group; a valid drag cannot conceal a
	// missing/foreign interactive entry or an unsupported parse-error claim.
	for _, mutate := range []func(map[string]any){
		func(m map[string]any) { delete(m, "interactive") },
		func(m map[string]any) { m["ignored_modifiers"] = m["drag"] },
		func(m map[string]any) { m["interactive"].(map[string]any)["effect"] = "parse-error" },
	} {
		m := fixturePolicyProvenance("drag", "interactive")
		mutate(m)
		raw, _ := json.Marshal(m)
		if _, err := decodePolicyProvenance(raw, "drag", "interactive"); err == nil {
			t.Fatal("invalid placement group accepted", string(raw))
		}
	}
}

func TestPolicyRowsKindsPrivacyAndEffects(t *testing.T) {
	for _, kind := range []string{"file", "generated", "invocation", "unavailable"} {
		for _, effect := range []string{"applied", "parse-error"} {
			t.Run(kind+"/"+effect, func(t *testing.T) {
				c := collector{req: Request{Source: desktopcheck.Request{Userdir: "/fixture"}}}
				s := Section{State: "observed", Rows: []Row{}}
				source := fixtureOrigin(kind, "/fixture/配置")
				retention := "retained"
				if kind == "unavailable" {
					retention = "unavailable"
					source["reason"] = "cannot read /private/SECRET"
				}
				raw, _ := json.Marshal(map[string]any{"version": 1, "ignored_modifiers": fixturePolicyEntry("last-value-change", effect, retention, source)})
				c.policyProvenance(&s, map[string]json.RawMessage{"provenance": raw}, "ignored_modifiers")
				text := fmt.Sprint(s)
				for _, secret := range []string{"SECRET", "/private", "/fixture", "successful"} {
					if strings.Contains(text, secret) {
						t.Fatal(text)
					}
				}
				if !strings.Contains(text, "aggregate mask only") || !strings.Contains(text, effect) {
					t.Fatal(text)
				}
				if effect == "parse-error" && !strings.Contains(text, "command did not succeed") || effect == "applied" && !strings.Contains(text, "same-value commands retain earlier source") {
					t.Fatal(text)
				}
				switch kind {
				case "file":
					if !strings.Contains(text, "user:配置:2-4") || !strings.Contains(text, "user:root:7-9") || s.State != "observed" {
						t.Fatal(text)
					}
				case "generated", "invocation":
					if !strings.Contains(text, "exact definition unavailable") || !strings.Contains(text, "enclosing context") {
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

func TestPolicyRowsReserveSourcesBeforeBindingPressureAndFilter(t *testing.T) {
	for _, filter := range []string{"", "policy", "policy placement drag", "policy ignored modifiers"} {
		t.Run(filter, func(t *testing.T) {
			req := Request{Scope: "bindings", Filter: filter, Source: desktopcheck.Request{Path: "config", Userdir: "/fixture", Datadir: "/data", Cwd: "/fixture"}}
			result, err := Collect(context.Background(), req, fixtureQuery(t, func(request map[string]json.RawMessage, m map[string]any) {
				if stringField(request, "resource") != "bindings" {
					return
				}
				scope := stringField(request, "scope")
				if scope == "policy" || scope == "placement" {
					groups := []string{"ignored_modifiers"}
					if scope == "placement" {
						if stringField(request, "generation") != fixtureGeneration {
							t.Fatal("placement lost generation guard")
						}
						groups = []string{"drag", "interactive"}
					}
					provenance := fixturePolicyProvenance(groups...)
					for _, group := range groups {
						source := fixtureOrigin("file", "/fixture/"+strings.Repeat("leaf", 130))
						source["include"] = []any{map[string]any{"path": "/private/SECRET/root", "first": 8, "last": 9}}
						provenance[group] = fixturePolicyEntry("last-value-change", "applied", "retained", source)
					}
					m["provenance"] = provenance
					return
				}
				offset, _ := intField(request, "offset")
				items := []any{}
				for i := 0; i < 16; i++ {
					items = append(items, map[string]any{"type": "Key", "code": 67, "key": "F1", "context_mask": 8, "modifier_mask": 4, "action": "Nop", "window": nil,
						"provenance": fixtureProvenance(scope, offset+i, fixtureOrigin("file", "/fixture/config"))})
				}
				m["bindings"], m["offset"], m["next_offset"] = items, offset, offset+16
			}))
			if err != nil {
				t.Fatal(err)
			}
			s := result.Sections[0]
			if len(s.Rows) > RowLimit || !s.Truncated || s.State != "partial" {
				t.Fatal(s)
			}
			groups := []string{"ignored_modifiers", "drag", "interactive"}
			for _, group := range groups {
				prefix := policyIdentity(group)
				if filter != "" && !strings.Contains(prefix, filter) {
					continue
				}
				var path strings.Builder
				include, effect, value := false, false, false
				for _, row := range s.Rows {
					if utf8.RuneCountInString(row.Value) > 200 {
						t.Fatal(row)
					}
					if strings.HasPrefix(row.Label, prefix+" last change definition ") {
						path.WriteString(row.Value)
					}
					if strings.HasPrefix(row.Label, prefix+" last change include ") && row.Value == "outside:(hidden):8-9" {
						include = true
					}
					effect = effect || row.Label == prefix+" effect"
					value = value || row.Label == prefix+" value"
				}
				if path.String() != "user:"+strings.Repeat("leaf", 130)+":2-4" || !include || !effect || !value {
					t.Fatal("policy source/span/explanation lost under pressure", group, s)
				}
			}
			if strings.Contains(textResult(result), "SECRET") || strings.Contains(textResult(result), "/fixture") {
				t.Fatal("policy path leaked", result)
			}
			if filter == "" {
				lastPolicy, firstBinding := -1, RowLimit
				for i, row := range s.Rows {
					if strings.HasPrefix(row.Label, "policy ") {
						lastPolicy = i
					} else if strings.HasPrefix(row.Label, "global ") && firstBinding == RowLimit {
						firstBinding = i
					}
				}
				if lastPolicy >= firstBinding || firstBinding == RowLimit {
					t.Fatal("policy rows followed enumeration", s)
				}
			}
		})
	}
}

func TestPolicySourceBudgetAndMissingMetadataAreVisible(t *testing.T) {
	for _, scope := range []string{"policy", "placement"} {
		for _, behavior := range []string{"absent", "malformed", "budget-exhausted"} {
			t.Run(scope+"/"+behavior, func(t *testing.T) {
				result, err := Collect(context.Background(), Request{Scope: "bindings"}, fixtureQuery(t, func(request map[string]json.RawMessage, m map[string]any) {
					if stringField(request, "resource") != "bindings" || stringField(request, "scope") != scope {
						return
					}
					switch behavior {
					case "absent":
						delete(m, "provenance")
					case "malformed":
						m["provenance"] = true
					case "budget-exhausted":
						for key := range m["provenance"].(map[string]any) {
							if key != "version" {
								m["provenance"].(map[string]any)[key] = fixturePolicyEntry("last-value-change", "applied", "budget-exhausted", fixtureOrigin("unavailable", ""))
							}
						}
					}
				}))
				if err != nil || result.Sections[0].State != "partial" {
					t.Fatal(result, err)
				}
				want := map[string]string{"absent": "producer supplied no metadata", "malformed": "malformed metadata", "budget-exhausted": "policy source retention budget exhausted"}[behavior]
				if !strings.Contains(textResult(result), want) {
					t.Fatal("failure hidden", result)
				}
			})
		}
	}
}

func TestPolicySourceExhaustionHasVisibleMarker(t *testing.T) {
	provenance := fixturePolicyProvenance("ignored_modifiers")
	provenance["ignored_modifiers"] = fixturePolicyEntry("last-value-change", "applied", "retained", fixtureOrigin("file", "/fixture/"+strings.Repeat("p", 600)))
	raw, _ := json.Marshal(provenance)
	c := collector{req: Request{Source: desktopcheck.Request{Userdir: "/fixture"}}}
	for _, used := range []int{RowLimit - 5, RowLimit - 2, RowLimit - 1} {
		s := Section{State: "observed", Rows: make([]Row, used)}
		c.policyProvenance(&s, map[string]json.RawMessage{"provenance": raw}, "ignored_modifiers")
		if !s.Truncated || s.State != "partial" || len(s.Rows) > RowLimit || !strings.Contains(s.Rows[len(s.Rows)-1].Value, "Source display incomplete") {
			t.Fatal(used, s)
		}
	}
}

// Generations guard collection consistency; they are not evidence that a mask
// changed. Placement may conservatively invalidate a generation on a no-op.
func TestPolicyNoOpOriginDisplayDoesNotInferFromGeneration(t *testing.T) {
	var previous []Row
	for _, generation := range []string{fixtureGeneration, strings.Repeat("2", 48)} {
		result, err := Collect(context.Background(), Request{Scope: "bindings", Filter: "policy", Source: desktopcheck.Request{
			Path: "entry", Userdir: "/fixture", Datadir: "/data", Cwd: "/fixture",
		}}, fixtureQuery(t, func(request map[string]json.RawMessage, m map[string]any) {
			if stringField(request, "resource") != "bindings" {
				return
			}
			m["generation"] = generation
			if provenance, ok := m["provenance"].(map[string]any); ok {
				for key := range provenance {
					if key != "version" {
						provenance[key] = fixturePolicyEntry("last-value-change", "applied", "retained", fixtureOrigin("file", "/fixture/earlier-value-change"))
					}
				}
			}
		}))
		if err != nil || result.Sections[0].State != "observed" || result.Sections[0].Generation != generation {
			t.Fatal(result, err)
		}
		rows := result.Sections[0].Rows
		if previous != nil && fmt.Sprint(rows) != fmt.Sprint(previous) {
			t.Fatal("generation fabricated a different policy origin", previous, rows)
		}
		previous = rows
		if !strings.Contains(fmt.Sprint(rows), "user:earlier-value-change:2-4") || !strings.Contains(fmt.Sprint(rows), "same-value commands retain earlier source") {
			t.Fatal(rows)
		}
	}
}

func TestPolicyRetainedUnavailableSourceNeverClaimsCoordinates(t *testing.T) {
	m := fixturePolicyProvenance("ignored_modifiers")
	source := fixtureOrigin("unavailable", "/fixture/SECRET")
	source["reason"] = "origin has /private/SECRET"
	m["ignored_modifiers"] = fixturePolicyEntry("last-value-change", "applied", "retained", source)
	raw, _ := json.Marshal(m)
	c := collector{}
	s := Section{State: "observed", Rows: []Row{}}
	c.policyProvenance(&s, map[string]json.RawMessage{"provenance": raw}, "ignored_modifiers")
	if s.State != "partial" || !strings.Contains(fmt.Sprint(s), "reported source unavailable") || strings.Contains(fmt.Sprint(s), "SECRET") || strings.Contains(fmt.Sprint(s), "definition") {
		t.Fatal(s)
	}
}

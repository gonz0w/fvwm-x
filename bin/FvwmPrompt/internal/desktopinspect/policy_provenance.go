// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopinspect

import (
	"encoding/json"
	"errors"
	"fmt"

	"github.com/fvwmorg/FvwmPrompt/internal/configsource"
)

type policyOrigin struct {
	basis, effect, retention string
	source                   *configsource.Source
}

// Validate the entire fixed group before presenting any origin. A valid source
// alone cannot establish that it belongs to the observed aggregate mask.
func decodePolicyProvenance(data []byte, groups ...string) (map[string]policyOrigin, error) {
	invalid := errors.New("invalid policy provenance")
	var fields map[string]json.RawMessage
	allowed := append([]string{"version"}, groups...)
	if strict(data, &fields, len(groups)*(configsource.Limit+256)+64) != nil ||
		len(fields) != len(allowed) || !knownFields(fields, allowed) {
		return nil, invalid
	}
	if version, ok := intField(fields, "version"); !ok || version != 1 {
		return nil, invalid
	}
	origins := make(map[string]policyOrigin, len(groups))
	for _, group := range groups {
		var entry map[string]json.RawMessage
		if strict(fields[group], &entry, configsource.Limit+256) != nil || len(entry) != 4 ||
			!knownFields(entry, []string{"basis", "source", "effect", "retention"}) {
			return nil, invalid
		}
		origin := policyOrigin{basis: stringField(entry, "basis"), effect: stringField(entry, "effect"), retention: stringField(entry, "retention")}
		var err error
		origin.source, err = configsource.Decode(entry["source"])
		if err != nil {
			return nil, invalid
		}
		switch origin.basis {
		case "builtin-default":
			if origin.effect != "builtin-default" || origin.retention != "not-applicable" || origin.source.Kind != "unavailable" {
				return nil, invalid
			}
		case "last-value-change":
			if origin.effect != "applied" && (origin.effect != "parse-error" || group != "ignored_modifiers") {
				return nil, invalid
			}
			switch origin.retention {
			case "retained":
				// Retaining an origin snapshot does not promise coordinates:
				// its version1 serialization may itself be unavailable.
			case "unavailable", "budget-exhausted":
				if origin.source.Kind != "unavailable" {
					return nil, invalid
				}
			default:
				return nil, invalid
			}
		default:
			return nil, invalid
		}
		origins[group] = origin
	}
	return origins, nil
}

func policyIdentity(group string) string {
	if group == "ignored_modifiers" {
		return "policy ignored modifiers"
	}
	return "policy placement " + group
}

func (c *collector) policyProvenance(s *Section, item map[string]json.RawMessage, groups ...string) {
	raw, exists := item["provenance"]
	origins, err := decodePolicyProvenance(raw, groups...)
	for _, group := range groups {
		prefix := policyIdentity(group)
		if err != nil {
			reason := "unavailable; malformed metadata"
			if !exists {
				reason = "unavailable; producer supplied no metadata"
			}
			c.row(s, prefix+" source", reason)
			c.problem(s, "Policy provenance unavailable")
			continue
		}
		origin := origins[group]
		rows := []Row{
			{prefix + " basis", origin.basis + "; aggregate mask only; no per-bit/button ownership or event winner"},
			{prefix + " retention", origin.retention},
		}
		effect := "builtin-default; no value-changing configuration command"
		if origin.effect == "applied" {
			effect = "applied; last value-changing command; same-value commands retain earlier source"
		} else if origin.effect == "parse-error" {
			effect = "parse-error; parse error in IgnoreModifiers changed observed mask; command did not succeed"
		}
		rows = append(rows, Row{prefix + " effect", effect})
		location := func(label string, site configsource.Site) {
			value := sourceRootLabel(c.req.Source, site.Path) + fmt.Sprintf(":%d-%d", site.First, site.Last)
			runes := []rune(value)
			parts := (len(runes) + 199) / 200
			for part := 0; part < parts; part++ {
				end := (part + 1) * 200
				if end > len(runes) {
					end = len(runes)
				}
				rows = append(rows, Row{fmt.Sprintf("%s %s %d/%d", prefix, label, part+1, parts), string(runes[part*200 : end])})
			}
		}
		source := origin.source
		switch source.Kind {
		case "file":
			rows = append(rows, Row{prefix + " source", "file; last value change; include chain root first"})
			location("last change definition", source.Site)
		case "generated", "invocation":
			rows = append(rows, Row{prefix + " source", source.Kind + "; exact definition unavailable; enclosing context only"})
			if source.Site.Path != "" {
				location("last change enclosing context", source.Site)
			}
		case "unavailable":
			reason := "unavailable; reported source unavailable; details hidden"
			if origin.basis == "builtin-default" {
				reason = "builtin-default; no configuration source"
			} else if origin.retention == "budget-exhausted" {
				reason = "unavailable; policy source retention budget exhausted"
			}
			rows = append(rows, Row{prefix + " source", reason})
		}
		if source.Kind != "unavailable" {
			for i, site := range source.Include {
				label := fmt.Sprintf("last change include %d/%d", i+1, len(source.Include))
				if source.Kind != "file" {
					label += " enclosing"
				}
				location(label, site)
			}
		}
		// The existing helper reserves a marker rather than silently clipping a
		// path/span. All policy groups run before enumeration can spend rows.
		if c.bindingSourceRows(s, prefix, rows) != nil {
			c.problem(s, "Policy source display incomplete")
		}
		if source.Kind == "unavailable" && origin.basis != "builtin-default" {
			c.problem(s, "Policy source unavailable")
		}
	}
}

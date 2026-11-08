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
	"strconv"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/configsource"
)

// A long definition name must not consume a continuation's part counter.
// The ellipsis makes the bounded identity visibly incomplete.
func themeRowPrefix(prefix string) string {
	runes := []rune(prefix)
	if len(runes) > 48 {
		return string(runes[:45]) + "..."
	}
	return prefix
}

// encoding/json substitutes unpaired UTF-16 escapes. Reject them so an exact
// copied-from identity cannot silently become a different donor name.
func themeDonorEscapes(data []byte) bool {
	for i := 0; i < len(data); i++ {
		if data[i] != '\\' {
			continue
		}
		i++
		if i >= len(data) {
			return false
		}
		if data[i] != 'u' {
			continue
		}
		if i+4 >= len(data) {
			return false
		}
		value, err := strconv.ParseUint(string(data[i+1:i+5]), 16, 16)
		if err != nil || value >= 0xdc00 && value <= 0xdfff {
			return false
		}
		i += 4
		if value >= 0xd800 && value <= 0xdbff {
			if i+6 >= len(data) || data[i+1] != '\\' || data[i+2] != 'u' {
				return false
			}
			low, err := strconv.ParseUint(string(data[i+3:i+7]), 16, 16)
			if err != nil || low < 0xdc00 || low > 0xdfff {
				return false
			}
			i += 6
		}
	}
	return true
}

type themeOrigin struct {
	basis, operation, retention string
	replaces                    bool
	copiedFrom                  string
	source                      *configsource.Source
}

// These are aggregate definition operations, including same-value definitions.
// A retained source does not establish ownership of each current field.
func decodeThemeProvenance(data []byte) (themeOrigin, error) {
	invalid := errors.New("invalid theme provenance")
	var fields map[string]json.RawMessage
	if strict(data, &fields, configsource.Limit+2048) != nil || len(fields) != 7 ||
		!knownFields(fields, []string{"version", "basis", "source", "operation", "retention", "replaces_definition", "copied_from"}) {
		return themeOrigin{}, invalid
	}
	version, vok := intField(fields, "version")
	replaces, rok := boolField(fields, "replaces_definition")
	if !vok || version != 1 || !rok {
		return themeOrigin{}, invalid
	}
	origin := themeOrigin{basis: stringField(fields, "basis"), operation: stringField(fields, "operation"), retention: stringField(fields, "retention"), replaces: replaces}
	var err error
	origin.source, err = configsource.Decode(fields["source"])
	if err != nil {
		return themeOrigin{}, invalid
	}
	donorNull := strings.TrimSpace(string(fields["copied_from"])) == "null"
	if !donorNull {
		var ok bool
		origin.copiedFrom, ok = boundedStringField(fields, "copied_from", 256, false)
		if !ok || origin.copiedFrom == "" || !safe(origin.copiedFrom, 256) || !themeDonorEscapes(fields["copied_from"]) {
			return themeOrigin{}, invalid
		}
	}
	switch origin.basis {
	case "builtin-default":
		if origin.operation != "builtin-default" || origin.retention != "not-applicable" || origin.source.Kind != "unavailable" || replaces || !donorNull {
			return themeOrigin{}, invalid
		}
	case "runtime-preview":
		if origin.operation != "preview" || origin.retention != "not-applicable" || origin.source.Kind != "unavailable" || !donorNull {
			return themeOrigin{}, invalid
		}
	case "last-definition":
		if !member(origin.operation, []string{"merge", "copy", "reset"}) {
			return themeOrigin{}, invalid
		}
		switch origin.retention {
		case "retained":
			// The retained snapshot may itself report unavailable coordinates.
			if origin.operation == "copy" && donorNull {
				return themeOrigin{}, invalid
			}
		case "unavailable", "budget-exhausted":
			if origin.source.Kind != "unavailable" {
				return themeOrigin{}, invalid
			}
		default:
			return themeOrigin{}, invalid
		}
		if origin.operation != "copy" && !donorNull {
			return themeOrigin{}, invalid
		}
	default:
		return themeOrigin{}, invalid
	}
	return origin, nil
}

// Keep each location/donor continuation explicit before the ordinary row bound.
// The marker occupies a reserved row when a domain runs out of display space.
func (c *collector) themeSourceRows(s *Section, prefix string, rows []Row) error {
	available := RowLimit - len(s.Rows) - 1
	if available < 0 {
		available = 0
	}
	complete := len(rows) <= available
	if !complete {
		rows = rows[:available]
	}
	for _, row := range rows {
		c.row(s, row.Label, row.Value)
	}
	if !complete {
		c.row(s, prefix+" source", "Theme source display incomplete; row budget reached")
		s.Truncated = true
		c.problem(s, "Theme source display incomplete")
		return errors.New("theme source row budget reached")
	}
	return nil
}

func (c *collector) themeProvenance(s *Section, item map[string]json.RawMessage, prefix string) error {
	prefix = themeRowPrefix(prefix)
	raw, exists := item["provenance"]
	origin, err := decodeThemeProvenance(raw)
	if err != nil {
		reason := "unavailable; malformed metadata"
		if !exists {
			reason = "unavailable; producer supplied no metadata"
		}
		c.problem(s, "Theme provenance unavailable")
		return c.themeSourceRows(s, prefix, []Row{{prefix + " source", reason}})
	}
	basis := origin.basis
	if basis == "last-definition" {
		basis += "; latest aggregate definition; untouched fields may retain earlier definitions"
	} else if basis == "runtime-preview" {
		basis += "; runtime preview of aggregate; configuration definition may be restored"
	}
	rows := []Row{
		{prefix + " basis", basis},
		{prefix + " operation", origin.operation},
		{prefix + " retention", origin.retention},
		{prefix + " replacement", fmt.Sprintf("replaces earlier definition=%t; aggregate operation only", origin.replaces)},
	}
	continuation := func(label, value string) {
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
	location := func(label string, site configsource.Site) {
		continuation(label, sourceRootLabel(c.req.Source, site.Path)+fmt.Sprintf(":%d-%d", site.First, site.Last))
	}
	if origin.operation == "copy" {
		rows = append(rows, Row{prefix + " copy", "source is copy invocation; donor identity only; no transitive history"})
		if origin.copiedFrom == "" {
			rows = append(rows, Row{prefix + " donor", "donor unavailable; retention gap"})
		} else {
			continuation("donor", origin.copiedFrom)
		}
	}
	source := origin.source
	switch source.Kind {
	case "file":
		rows = append(rows, Row{prefix + " source", "file; latest aggregate definition; include chain root first"})
		location("definition", source.Site)
	case "generated", "invocation":
		rows = append(rows, Row{prefix + " source", source.Kind + "; exact definition unavailable; enclosing context only"})
		if source.Site.Path != "" {
			location("enclosing context", source.Site)
		}
	case "unavailable":
		reason := "unavailable; reported source unavailable; details hidden"
		if origin.basis == "builtin-default" {
			reason = "builtin-default; no configuration source"
		} else if origin.basis == "runtime-preview" {
			reason = "runtime preview; configuration source unavailable"
		} else if origin.retention == "budget-exhausted" {
			reason = "unavailable; theme source retention budget exhausted"
		}
		rows = append(rows, Row{prefix + " source", reason})
	}
	if source.Kind != "unavailable" {
		for i, site := range source.Include {
			label := fmt.Sprintf("include %d/%d", i+1, len(source.Include))
			if source.Kind != "file" {
				label += " enclosing"
			}
			location(label, site)
		}
	}
	err = c.themeSourceRows(s, prefix, rows)
	if source.Kind == "unavailable" && origin.basis == "last-definition" {
		c.problem(s, "Theme source unavailable")
	}
	return err
}

// Admit rows in domain order, one from each domain per turn. A long colorset
// inventory cannot hide every menu/decor definition or its first source group.
func (c *collector) themeDomainRows(s *Section, domains ...*Section) {
	for _, domain := range domains {
		if domain.Reason != "" {
			c.problem(s, domain.Reason)
		}
		s.Truncated = s.Truncated || domain.Truncated
	}
	incomplete := false
	for index := 0; ; index++ {
		more := false
		for _, domain := range domains {
			if index >= len(domain.Rows) {
				continue
			}
			more = true
			if len(s.Rows) >= RowLimit-1 {
				incomplete = true
				break
			}
			row := domain.Rows[index]
			c.row(s, row.Label, row.Value)
		}
		if !more || incomplete {
			break
		}
	}
	if incomplete || s.Truncated {
		c.row(s, "Theme inventory", "Theme inventory incomplete; row or query budget reached")
		s.Truncated = true
		c.problem(s, "Theme inventory incomplete")
	}
}

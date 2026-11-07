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

func decodeBindingProvenance(data []byte, scope string, position int) (*configsource.Source, bool, error) {
	invalid := errors.New("invalid binding provenance")
	var fields map[string]json.RawMessage
	if strict(data, &fields, configsource.Limit+512) != nil || len(fields) != 6 ||
		!knownFields(fields, []string{"version", "source", "replaced_existing", "position", "selection", "effective"}) {
		return nil, false, invalid
	}
	version, vok := intField(fields, "version")
	got, pok := intField(fields, "position")
	replaced, rok := boolField(fields, "replaced_existing")
	selection := "window-specific-before-global"
	if scope != "global" {
		selection = "menu-before-fallback"
	}
	if !vok || version != 1 || !pok || got != position || !rok ||
		stringField(fields, "selection") != selection || stringField(fields, "effective") != "requires-event-context" {
		return nil, false, invalid
	}
	source, err := configsource.Decode(fields["source"])
	if err != nil {
		return nil, false, invalid
	}
	return source, replaced, nil
}

// Reserve a physical row for a visible incomplete-source marker. Locations are
// split before c.row's normal bound, so no cut path/span can appear complete.
func (c *collector) bindingSourceRows(s *Section, binding string, rows []Row) error {
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
		c.row(s, binding+" source", "Source display incomplete; row budget reached")
		s.Truncated = true
		c.problem(s, "Binding source display incomplete")
		return errors.New("binding source row budget reached")
	}
	return nil
}

func (c *collector) bindingProvenance(s *Section, item map[string]json.RawMessage, scope, binding string, position int) error {
	// Repeat the visible binding identity so the existing per-row filter retains
	// its facts and locations even when the source filename does not match.
	prefix := fmt.Sprintf("%s #%d", binding, position)
	raw, exists := item["provenance"]
	source, replaced, err := decodeBindingProvenance(raw, scope, position)
	if err != nil {
		reason := "unavailable; malformed metadata"
		if !exists {
			reason = "unavailable; producer supplied no metadata"
		}
		c.problem(s, "Binding provenance unavailable")
		return c.bindingSourceRows(s, prefix, []Row{{prefix + " source", reason}})
	}
	selection := "window-specific before global"
	if scope != "global" {
		selection = "menu before fallback"
	}
	rows := []Row{
		{prefix + " order", fmt.Sprintf("position=%d; %s; requires event context", position, selection)},
		{prefix + " replacement", fmt.Sprintf("same identity matched at definition=%t; current definition only", replaced)},
	}
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
	switch source.Kind {
	case "file":
		rows = append(rows, Row{prefix + " source", "file; literal definition; include chain root first"})
		location("definition", source.Site)
	case "generated", "invocation":
		rows = append(rows, Row{prefix + " source", source.Kind + "; exact definition unavailable; enclosing context only"})
		if source.Site.Path != "" {
			location("enclosing context", source.Site)
		}
	case "unavailable":
		reason := "unavailable; reported source unavailable; details hidden"
		if source.Reason == "binding source retention budget exhausted" {
			reason = "unavailable; binding source retention budget exhausted"
		}
		rows = append(rows, Row{prefix + " source", reason})
	}
	// An unavailable source must never present an optional site as exact.
	if source.Kind != "unavailable" {
		for i, site := range source.Include {
			label := fmt.Sprintf("include %d/%d", i+1, len(source.Include))
			if source.Kind != "file" {
				label += " enclosing"
			}
			location(label, site)
		}
	}
	err = c.bindingSourceRows(s, prefix, rows)
	if source.Kind == "unavailable" {
		c.problem(s, "Binding source unavailable")
	}
	return err
}

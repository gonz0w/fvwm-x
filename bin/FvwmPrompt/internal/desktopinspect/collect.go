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
	"sort"
	"strconv"
	"strings"
	"time"
	"unicode"

	"github.com/fvwmorg/FvwmPrompt/internal/commandmeta"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopcheck"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

type collector struct {
	ctx     context.Context
	req     Request
	query   func(context.Context, string) (json.RawMessage, error)
	queries int
	home    string
}

// Collect owns finite query work, source traversal and projection only. Domains
// are sampled separately; even equal generations do not imply an atomic desktop
// snapshot. The settings snapshot belongs to the calling resident controller.
func Collect(ctx context.Context, req Request, query func(context.Context, string) (json.RawMessage, error)) (Result, error) {
	if err := req.validate(); err != nil {
		return Result{}, err
	}
	ctx, cancel := context.WithTimeout(ctx, 4*time.Second)
	defer cancel()
	home, _ := os.UserHomeDir()
	c := collector{ctx: ctx, req: req, query: query, home: home}
	result := Result{SchemaVersion: 1, Scope: req.Scope, Filter: req.Filter, Provider: req.Provider, SettingsGeneration: req.SettingsGeneration, Sections: []Section{}}
	for _, scope := range scopes {
		if req.Scope != "all" && req.Scope != scope {
			continue
		}
		s := Section{ID: scope, Title: strings.ToUpper(scope[:1]) + scope[1:], State: "unavailable", Rows: []Row{}}
		switch scope {
		case "settings":
			c.settings(&s)
		case "theme":
			c.theme(&s)
		case "bindings":
			c.bindings(&s)
		case "layout":
			c.layout(&s)
		case "sources":
			c.sources(&s)
		case "commands":
			c.commands(&s)
		}
		if req.Filter != "" {
			rows := []Row{}
			needle := strings.ToLower(req.Filter)
			for _, row := range s.Rows {
				if strings.Contains(strings.ToLower(row.Label+" "+row.Value), needle) {
					rows = append(rows, row)
				}
			}
			s.Rows = rows
		}
		if len(s.Rows) > 0 && s.State == "unavailable" {
			s.State = "partial"
		}
		if s.Truncated {
			c.problem(&s, "Row or query budget reached; inventory is incomplete")
		}
		result.Sections = append(result.Sections, s)
	}
	if err := result.validate(); err != nil {
		return Result{}, err
	}
	encoded, err := json.Marshal(result)
	if err != nil {
		return Result{}, err
	}
	if len(encoded) > ResultLimit {
		return Result{}, errors.New("inspector result exceeds limit")
	}
	return result, nil
}
func (c *collector) clean(text string, limit int) string {
	if c.home != "" && c.home != "/" {
		text = strings.ReplaceAll(text, c.home, "~")
	}
	out := []rune{}
	for _, ch := range strings.ToValidUTF8(text, "�") {
		if unicode.IsControl(ch) {
			ch = ' '
		}
		if len(out) == limit {
			break
		}
		out = append(out, ch)
	}
	return string(out)
}
func (c *collector) row(s *Section, label, value string) {
	if len(s.Rows) >= RowLimit {
		s.Truncated = true
		return
	}
	s.Rows = append(s.Rows, Row{c.clean(label, 80), c.clean(value, 200)})
}
func (c *collector) problem(s *Section, reason string) {
	if len(s.Rows) > 0 {
		s.State = "partial"
	} else {
		s.State = "unavailable"
	}
	if s.Reason == "" {
		s.Reason = c.clean(reason, 200)
	} else if !strings.Contains(s.Reason, reason) {
		s.Reason = c.clean(s.Reason+"; "+reason, 200)
	}
}
func (c *collector) call(resource, scope string, offset int, generation string, paged bool) (map[string]json.RawMessage, error) {
	if c.query == nil {
		return nil, errors.New("MFL read-only query provider unavailable")
	}
	if err := c.ctx.Err(); err != nil {
		return nil, errors.New("Inspection deadline reached")
	}
	if c.queries >= 40 {
		return nil, errors.New("Query budget reached")
	}
	c.queries++
	request := map[string]any{"version": 1, "id": fmt.Sprintf("inspect-%d", c.queries), "method": "query", "resource": resource}
	if scope != "" {
		request["scope"] = scope
	}
	if generation != "" {
		request["generation"] = generation
	}
	if paged {
		request["offset"] = offset
		request["limit"] = 16
	}
	text, _ := json.Marshal(request)
	data, err := c.query(c.ctx, string(text))
	if err != nil {
		return nil, errors.New("MFL unavailable or query interrupted")
	}
	var envelope struct {
		Reply struct {
			Version int             `json:"version"`
			ID      *string         `json:"id"`
			Status  string          `json:"status"`
			Result  json.RawMessage `json:"result"`
			Error   *struct {
				Code string `json:"code"`
			} `json:"error"`
		} `json:"reply"`
	}
	// The transport has already checked reply identity. Validate it again for
	// injected callers, and do not expose provider-supplied error text.
	if err := strict(data, &envelope, ResultLimit); err != nil {
		return nil, errors.New("Malformed MFL reply")
	}
	if envelope.Reply.Version != 1 || envelope.Reply.ID == nil || *envelope.Reply.ID != request["id"] {
		return nil, errors.New("Mismatched MFL reply")
	}
	if envelope.Reply.Status == "error" {
		if envelope.Reply.Error != nil && member(envelope.Reply.Error.Code, []string{"stale_generation", "generation_unavailable", "query_too_large", "unsupported_resource", "unavailable", "provider_unavailable", "provider_lost", "permission_denied", "timeout", "invalid_request"}) {
			return nil, errors.New(envelope.Reply.Error.Code)
		}
		return nil, errors.New("MFL query refused")
	}
	if envelope.Reply.Status != "ok" {
		return nil, errors.New("Malformed MFL reply status")
	}
	var result map[string]json.RawMessage
	if err := strict(envelope.Reply.Result, &result, ResultLimit); err != nil || result == nil {
		return nil, errors.New("Malformed query result")
	}
	if resource != "desktop" && stringField(result, "state") != "observed" {
		return nil, errors.New("Query result is not observed")
	}
	return result, nil
}
func stringField(m map[string]json.RawMessage, key string) string {
	var s string
	_ = json.Unmarshal(m[key], &s)
	return s
}
func boundedStringField(m map[string]json.RawMessage, key string, limit int, nullable bool) (string, bool) {
	raw, exists := m[key]
	if !exists {
		return "", false
	}
	if string(raw) == "null" {
		return "", nullable
	}
	var value string
	if json.Unmarshal(raw, &value) != nil || len(value) > limit {
		return "", false
	}
	return value, true
}
func intField(m map[string]json.RawMessage, key string) (int, bool) {
	var n int
	if len(m[key]) == 0 || string(m[key]) == "null" {
		return 0, false
	}
	err := json.Unmarshal(m[key], &n)
	return n, err == nil
}
func boolField(m map[string]json.RawMessage, key string) (bool, bool) {
	var b bool
	if len(m[key]) == 0 || string(m[key]) == "null" {
		return false, false
	}
	err := json.Unmarshal(m[key], &b)
	return b, err == nil
}
func objectField(m map[string]json.RawMessage, key string) (map[string]json.RawMessage, bool) {
	var v map[string]json.RawMessage
	err := json.Unmarshal(m[key], &v)
	return v, err == nil && v != nil
}
func listField(m map[string]json.RawMessage, key string) ([]map[string]json.RawMessage, bool) {
	var v []map[string]json.RawMessage
	err := json.Unmarshal(m[key], &v)
	return v, err == nil && v != nil
}
func token(s string) bool { return len(s) == 48 && strings.Trim(s, "0123456789abcdef") == "" }

// pages validates continuations before invoking the projector. A changing
// generation never labels previously accumulated rows as a fresh complete list.
func (c *collector) pages(s *Section, resource, scope, property string, expected *string, project func(map[string]json.RawMessage) error) {
	offset, generation := 0, ""
	for page := 0; page < 8; page++ {
		m, err := c.call(resource, scope, offset, generation, true)
		if err != nil {
			c.problem(s, resource+" "+scope+": "+err.Error())
			return
		}
		got := stringField(m, "generation")
		position, ok := intField(m, "offset")
		if !token(got) || !ok || position != offset || generation != "" && generation != got || expected != nil && *expected != "" && *expected != got {
			c.problem(s, resource+" generation or offset changed")
			return
		}
		generation = got
		if expected != nil {
			*expected = got
		}
		items, ok := listField(m, property)
		if !ok || len(items) > 16 {
			c.problem(s, resource+" malformed page")
			return
		}
		for _, item := range items {
			if err := project(item); err != nil {
				c.problem(s, resource+": "+err.Error())
				return
			}
		}
		next, exists := m["next_offset"]
		if !exists {
			c.problem(s, resource+" missing continuation")
			return
		}
		if string(next) == "null" {
			return
		}
		n, ok := intField(m, "next_offset")
		if !ok || len(items) == 0 || n != offset+len(items) || n > 4096 {
			c.problem(s, resource+" invalid continuation")
			return
		}
		offset = n
		if len(s.Rows) >= RowLimit {
			s.Truncated = true
			return
		}
	}
	s.Truncated = true
}

// Session Query distinguishes proposed policy, unavailable observations and a
// live owner. Only the unavailable observation may carry a null effective
// value; a bus request always describes next-login policy, never a live change.
func sessionSettingsMetadata(key string, item map[string]json.RawMessage) (bool, bool) {
	available := strings.TrimSpace(string(item["available"]))
	if available != "true" && available != "false" {
		return false, false
	}
	requestedSource := stringField(item, "requested_source")
	if !member(requestedSource, []string{"saved", "module-configuration"}) {
		return false, false
	}
	var requested any
	decoder := json.NewDecoder(strings.NewReader(string(item["requested"])))
	decoder.UseNumber()
	if decoder.Decode(&requested) != nil {
		return false, false
	}
	if _, err := desktopsettings.Value(key, requested, false); err != nil {
		return false, false
	}
	source, application := stringField(item, "source"), stringField(item, "application")
	nullEffective := strings.TrimSpace(string(item["effective"])) == "null"
	expected := "not-submitted"
	unavailable := false
	switch source {
	case "saved", "module-configuration":
		if source != requestedSource || nullEffective {
			return false, false
		}
	case "unavailable":
		if available != "false" || !nullEffective {
			return false, false
		}
		expected, unavailable = "live-unavailable", true
	case "session-owner":
		if available != "true" || nullEffective {
			return false, false
		}
		expected = "live-owner-observed"
	default:
		return false, false
	}
	if key == "session_bus" {
		expected = "next-login"
	}
	return unavailable, application == expected
}

func (c *collector) settings(s *Section) {
	s.Generation = c.req.SettingsGeneration
	var snapshot map[string]map[string]json.RawMessage
	if len(c.req.Settings) == 0 || json.Unmarshal(c.req.Settings, &snapshot) != nil || snapshot == nil {
		s.Reason = "Controller settings snapshot unavailable"
		return
	}
	s.State = "provider-policy"
	s.Reason = "Controller policy; submitted does not establish final paint or an atomic desktop snapshot"
	keys, _ := desktopsettings.Scope("all")
	registry := desktopsettings.Registry()
	for _, key := range keys {
		item := snapshot[key]
		definition := registry[key]
		if item == nil || stringField(item, "type") != definition.Type || stringField(item, "generation") != c.req.SettingsGeneration {
			c.problem(s, "Settings metadata missing or generation changed")
			continue
		}
		session := desktopsettings.SessionKey(key)
		sessionUnavailable := false
		if session {
			var valid bool
			sessionUnavailable, valid = sessionSettingsMetadata(key, item)
			if !valid {
				c.problem(s, "Invalid session settings metadata")
				continue
			}
		}
		value := func(field string) string {
			if field == "effective" && sessionUnavailable {
				return "unavailable"
			}
			var v any
			decoder := json.NewDecoder(strings.NewReader(string(item[field])))
			decoder.UseNumber()
			if decoder.Decode(&v) != nil {
				c.problem(s, "Settings value unavailable")
				return "unavailable"
			}
			validationKey := key
			if field == "resolved" && key == "wallpaper_appearance" {
				// Follow may resolve to the scheduled appearance policy. Reuse
				// its registry grammar rather than adding a second enum.
				validationKey = "appearance"
			}
			validated, err := desktopsettings.Value(validationKey, v, false)
			if err != nil {
				c.problem(s, "Settings value unavailable")
				return "unavailable"
			}
			return fmt.Sprint(validated)
		}
		c.row(s, key, fmt.Sprintf("default=%v; saved=%s; effective=%s", definition.Default, value("saved"), value("effective")))
		c.row(s, key+" metadata", definition.Description+"; group="+definition.Group+"; type="+definition.Type)
		constraint := ""
		if len(definition.Choices) > 0 {
			constraint = "choices=" + strings.Join(definition.Choices, ",")
		}
		if definition.Minimum != nil {
			constraint = fmt.Sprintf("range=%v..%v", definition.Minimum, definition.Maximum)
		}
		if constraint != "" {
			c.row(s, key+" values", constraint)
		}
		source, application := stringField(item, "source"), stringField(item, "application")
		applications := []string{"not-submitted", "submitted-to-fvwm"}
		if desktopsettings.TrayKey(key) {
			applications = append(applications, "requested-state-submitted")
		}
		if !session && (!member(source, []string{"saved", "module-configuration"}) || !member(application, applications)) {
			c.problem(s, "Invalid settings source or application metadata")
			continue
		}
		c.row(s, key+" source", source+"; "+application+"; owner=FvwmDesktop")
		if session {
			request := "requested=" + value("requested") + "; source=" + stringField(item, "requested_source") + "; available=" + strings.TrimSpace(string(item["available"]))
			if key == "session_bus" {
				request += "; next login"
			}
			c.row(s, key+" request", request)
		}
		if desktopsettings.TrayKey(key) {
			if stringField(item, "readiness") != "unobserved" {
				c.problem(s, "Invalid tray readiness metadata")
				continue
			}
			c.row(s, key+" request", "requested="+value("requested")+"; readiness=unobserved")
		}
		var runtime any
		_ = json.Unmarshal(item["runtime_constraint"], &runtime)
		if runtime != nil {
			if constraint, ok := runtime.(string); ok && member(constraint, []string{"wallpaper-retained", "high-contrast", "transparency-disabled"}) {
				c.row(s, key+" constraint", constraint)
			} else {
				c.problem(s, "Unknown settings constraint")
			}
		}
		resolution := stringField(item, "resolution")
		if member(resolution, []string{"explicit", "retained", "follow", "requested", "high-contrast", "transparency-disabled"}) {
			c.row(s, key+" resolution", resolution+"; resolved="+value("resolved"))
		}
		if data := item["persistence_error"]; len(data) > 0 && string(data) != "null" {
			c.row(s, "Persistence", "An error was reported; details hidden")
		}
	}
}
func (c *collector) commands(s *Section) {
	s.State = "provider-policy"
	s.Reason = "Supported command names and descriptions; argument grammar and execution are unavailable"
	s.Generation = "embedded-command-metadata"
	commands := commandmeta.Commands()
	sort.Slice(commands, func(i, j int) bool { return commands[i].Name < commands[j].Name })
	for _, command := range commands {
		// Filter the complete bounded generated inventory before row admission,
		// allowing searches for commands beyond the first displayed page.
		if c.req.Filter != "" && !strings.Contains(strings.ToLower(command.Name+" "+command.Description), strings.ToLower(c.req.Filter)) {
			continue
		}
		c.row(s, command.Name, command.Description)
	}
}
func (c *collector) action(text string) string {
	fields := strings.Fields(text)
	if len(fields) == 0 {
		return "user action (hidden)"
	}
	for _, command := range commandmeta.Commands() {
		if strings.EqualFold(fields[0], command.Name) {
			return command.Name + " (arguments hidden)"
		}
	}
	return "user action (hidden)"
}
func (c *collector) sources(s *Section) {
	if c.req.Source == (desktopcheck.Request{}) {
		s.Reason = "No literal configuration source selected; exact runtime origin is unavailable"
		return
	}
	observed, err := desktopcheck.Observe(c.ctx, c.req.Source)
	if err != nil {
		s.Reason = "Literal source traversal unavailable"
		return
	}
	s.State = "literal-observations"
	s.Generation = "literal-traversal"
	s.Truncated = observed.Truncated
	s.Reason = "Candidate literal definitions in traversal order; never proof of exact runtime origin"
	if observed.Report.Status != "checked-subset" {
		c.problem(s, "Traversal has errors or unchecked syntax; last definition unknown")
	}
	c.row(s, "Coverage", fmt.Sprintf("%s; files=%d; checked=%d; unchecked=%d; errors=%d", observed.Report.Coverage, len(observed.Report.Files), observed.Report.CheckedDirectives, observed.Report.Unchecked, observed.Report.Errors))
	for _, observation := range observed.Observations {
		position := fmt.Sprintf("%s:%d", filepath.Base(observation.Path), observation.Line)
		if observation.EndLine != observation.Line {
			position += fmt.Sprintf("-%d", observation.EndLine)
		}
		last := ""
		if observation.Last {
			last = "; last in checked literals"
		}
		c.row(s, fmt.Sprintf("%d %s", observation.Order, observation.Key), fmt.Sprintf("%v; %s; includes=%d%s", observation.Value, position, len(observation.IncludedFrom), last))
	}
	for _, diagnostic := range observed.Report.Diagnostics {
		// Codes and parser ownership are fixed checker data; message text and
		// full paths may contain user-specific source and are never forwarded.
		c.row(s, "Source diagnostic", diagnostic.Severity+" "+diagnostic.Code+"; "+filepath.Base(diagnostic.Path)+":"+strconv.Itoa(diagnostic.Line))
	}
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopworker

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

type StartupRecord struct {
	ID            string `json:"id"`
	Name          string `json:"name"`
	Icon          string `json:"icon"`
	ApplicationID string `json:"application_id"`
	WMClass       string `json:"wmclass"`
	State         string `json:"state"`
	Window        uint64 `json:"window"`
	AgeMS         uint64 `json:"age_ms"`
}
type StartupCounters struct {
	Dropped        uint64 `json:"dropped"`
	Malformed      uint64 `json:"malformed"`
	PartialExpired uint64 `json:"partial_expired"`
}
type StartupLimits struct {
	Records      int `json:"records"`
	Streams      int `json:"streams"`
	MessageBytes int `json:"message_bytes"`
	PartialMS    int `json:"partial_ms"`
	PendingMS    int `json:"pending_ms"`
	RetentionMS  int `json:"retention_ms"`
}
type StartupSnapshot struct {
	Generation string          `json:"generation"`
	Startups   []StartupRecord `json:"startups"`
	Counters   StartupCounters `json:"counters"`
	Limits     StartupLimits   `json:"limits"`
}

func startupGeneration(value string) bool {
	return len(value) == 48 && strings.Trim(value, "0123456789abcdef") == ""
}
func startupLimits(value StartupLimits) bool {
	return value == (StartupLimits{Records: 64, Streams: 32, MessageBytes: 4096,
		PartialMS: 5000, PendingMS: 60000, RetentionMS: 60000})
}
func startupObject(value any, keys ...string) bool {
	object, ok := value.(map[string]any)
	if !ok || len(object) != len(keys) {
		return false
	}
	for _, key := range keys {
		if value, ok := object[key]; !ok || value == nil {
			return false
		}
	}
	return true
}
func validateStartupSnapshot(value StartupSnapshot) error {
	if !startupGeneration(value.Generation) || value.Startups == nil || len(value.Startups) > 64 || !startupLimits(value.Limits) {
		return errors.New("invalid startup snapshot bounds")
	}
	seen := make(map[string]bool)
	for _, record := range value.Startups {
		if record.ID == "" || seen[record.ID] || record.Name == "" || record.Window > 0xffffffff {
			return errors.New("invalid startup record identity")
		}
		seen[record.ID] = true
		for _, text := range []string{record.ID, record.Name, record.Icon, record.ApplicationID, record.WMClass} {
			if len(text) > 255 || !utf8.ValidString(text) || strings.ContainsRune(text, 0) {
				return errors.New("invalid startup record text")
			}
		}
		switch record.State {
		case "pending", "protocol-ended", "window-observed", "expired":
		default:
			return errors.New("invalid startup activity state")
		}
		if record.State == "window-observed" && record.Window == 0 || record.State != "window-observed" && record.Window != 0 {
			return errors.New("startup window does not match its observation state")
		}
	}
	return nil
}

// DecodeStartupSnapshot checks the private worker result before UI publication.
// The worker already performed duplicate-aware query/page validation.
func DecodeStartupSnapshot(data []byte) (StartupSnapshot, error) {
	var result StartupSnapshot
	if len(data) > ResultLimit || !utf8.Valid(data) {
		return result, errors.New("startup snapshot exceeds result bounds")
	}
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(&result); err != nil {
		return StartupSnapshot{}, err
	}
	if err := validateStartupSnapshot(result); err != nil {
		return StartupSnapshot{}, err
	}
	return result, nil
}

func collectStartups(ctx context.Context, query func(context.Context, string) (json.RawMessage, error)) (StartupSnapshot, error) {
	if query == nil {
		return StartupSnapshot{}, errors.New("startup activity query unavailable")
	}
	result := StartupSnapshot{Startups: []StartupRecord{}}
	offset, total := 0, -1
	for page := 0; page < 64; page++ {
		if err := ctx.Err(); err != nil {
			return StartupSnapshot{}, err
		}
		id := fmt.Sprintf("startup-%d", page)
		request := map[string]any{"version": 1, "id": id, "method": "query", "resource": "startups", "offset": offset, "limit": 16}
		if result.Generation != "" {
			request["generation"] = result.Generation
		}
		encoded, _ := json.Marshal(request)
		data, err := query(ctx, string(encoded))
		if err != nil {
			return StartupSnapshot{}, errors.New("startup activity query unavailable or interrupted")
		}
		// Reuse the project's duplicate-aware bounded JSON grammar; typed
		// validation then prevents null/missing continuations and partial lists.
		decoded, err := desktopsettings.DecodeJSON(data)
		if err != nil {
			return StartupSnapshot{}, errors.New("malformed startup activity reply")
		}
		outer, ok := decoded.(map[string]any)
		if !ok {
			return StartupSnapshot{}, errors.New("invalid startup activity envelope")
		}
		reply, ok := outer["reply"].(map[string]any)
		if !ok || reply["version"] != json.Number("1") || reply["id"] != id || reply["status"] != "ok" {
			return StartupSnapshot{}, errors.New("startup activity query refused or mismatched")
		}
		raw, ok := reply["result"].(map[string]any)
		if !ok {
			return StartupSnapshot{}, errors.New("invalid startup activity page")
		}
		for _, key := range []string{"state", "scope", "generation", "offset", "next_offset", "total", "limit", "more", "startups", "counters", "limits"} {
			if value, ok := raw[key]; !ok || key != "next_offset" && value == nil {
				return StartupSnapshot{}, errors.New("incomplete startup activity page")
			}
		}
		if !startupObject(raw["counters"], "dropped", "malformed", "partial_expired") ||
			!startupObject(raw["limits"], "records", "streams", "message_bytes", "partial_ms", "pending_ms", "retention_ms") {
			return StartupSnapshot{}, errors.New("invalid startup activity counters or limits")
		}
		records, ok := raw["startups"].([]any)
		if !ok || len(records) > 16 {
			return StartupSnapshot{}, errors.New("invalid startup activity records")
		}
		for _, record := range records {
			if !startupObject(record, "id", "name", "icon", "application_id", "wmclass", "state", "window", "age_ms") {
				return StartupSnapshot{}, errors.New("incomplete startup activity record")
			}
		}
		var got struct {
			State, Scope, Generation string
			Offset, Total, Limit     int
			More                     bool
			NextOffset               *int `json:"next_offset"`
			Startups                 []StartupRecord
			Counters                 StartupCounters
			Limits                   StartupLimits
		}
		encoded, _ = json.Marshal(raw)
		decoder := json.NewDecoder(bytes.NewReader(encoded))
		decoder.DisallowUnknownFields()
		if err := decoder.Decode(&got); err != nil || got.State != "observed" || got.Scope != "core-startup-activity" ||
			!startupGeneration(got.Generation) || got.Offset != offset || got.Total < 0 || got.Total > 64 || got.Limit < 1 || got.Limit > 16 ||
			got.Startups == nil || len(got.Startups) > got.Limit || offset+len(got.Startups) > got.Total || !startupLimits(got.Limits) ||
			raw["more"] == nil || got.More != (got.NextOffset != nil) ||
			result.Generation != "" && result.Generation != got.Generation || total >= 0 && total != got.Total {
			return StartupSnapshot{}, errors.New("startup activity generation or page changed")
		}
		result.Generation, total, result.Counters, result.Limits = got.Generation, got.Total, got.Counters, got.Limits
		result.Startups = append(result.Startups, got.Startups...)
		if err := validateStartupSnapshot(result); err != nil {
			return StartupSnapshot{}, err
		}
		offset += len(got.Startups)
		if got.NextOffset == nil {
			if offset != total {
				return StartupSnapshot{}, errors.New("startup activity ended before complete inventory")
			}
			return result, nil
		}
		if len(got.Startups) == 0 || *got.NextOffset != offset || offset >= total {
			return StartupSnapshot{}, errors.New("invalid startup activity continuation")
		}
	}
	return StartupSnapshot{}, errors.New("startup activity page budget exceeded")
}

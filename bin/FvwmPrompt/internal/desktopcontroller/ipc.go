// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"bytes"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
	"reflect"
	"sort"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

func stringAt(v values, key string) string { s, _ := v[key].(string); return s }
func identity(s string, size int) bool {
	if len(s) != size {
		return false
	}
	for _, c := range s {
		if !(c >= '0' && c <= '9' || c >= 'a' && c <= 'f') {
			return false
		}
	}
	return true
}
func exact(r values, keys ...string) bool {
	if len(r) != len(keys) {
		return false
	}
	for _, k := range keys {
		if _, ok := r[k]; !ok {
			return false
		}
	}
	return true
}
func failure(code string) values   { return values{"error": values{"code": code}} }
func success(result values) values { return values{"result": result} }

// Public settings inherit owner, generation, preview, availability, capability
// and the common persistence error from their result envelope. Distinct
// per-setting metadata still belongs to that row, including a runtime-kept
// unsaved preference.
// Keep each standalone native/inspector record self-contained, but avoid
// repeating shared metadata in every row of one bounded reply.
// Copies preserve the standalone metadata if a caller retains that query.
func publicSettings(settings values, commonError string) values {
	result := values{}
	for key, raw := range settings {
		item := values{}
		for field, value := range raw.(map[string]any) {
			if field == "owner" || field == "generation" || field == "preview" && value == false {
				continue
			}
			if field == "available" && value == true || field == "capability" && value == "owned-desktop-preference" {
				continue
			}
			if field == "persistence_error" {
				switch message := value.(type) {
				case *string:
					if message == nil && commonError == "" || message != nil && *message == commonError {
						continue
					}
				case string:
					if message == commonError {
						continue
					}
				case nil:
					if commonError == "" {
						continue
					}
				}
			}
			item[field] = value
		}
		result[key] = item
	}
	return result
}

// Public IPC additionally inherits repeated metadata from two explicit maps.
// Saved/effective values, descriptions and choices/bounds stay per row.
// Application rows retain their group explicitly to select application defaults.
// Shared type/default and observation metadata have explicit inheritance maps.
// Standalone query/inspector records remain self-contained.
func compactPublicSettings(settings values, commonError string) (values, values, values) {
	rows := publicSettings(settings, commonError)
	defaults := sharedSettingMetadata(rows, false, []string{
		"native_preview", "runtime_constraint", "source", "application", "type", "default", "group"})
	application := sharedSettingMetadata(rows, true, []string{
		"capability", "native_mapping", "application", "readiness", "resource_state",
		"native_run", "native_sequence", "xserial", "failure_reason", "type", "default"})
	// The application group is the explicit discriminator for its second
	// defaults map. Other groups may use the ordinary shared metadata default.
	if defaults["group"] == "application" {
		delete(defaults, "group")
	}
	for field, value := range application {
		if common, present := defaults[field]; present && reflect.DeepEqual(common, value) {
			delete(application, field)
		}
	}
	for key, raw := range rows {
		row := raw.(map[string]any)
		inherited := merged(defaults, nil)
		if row["group"] == "application" {
			inherited = merged(inherited, application)
		}
		for field, value := range inherited {
			if field == "group" && row["group"] == "application" {
				continue
			}
			if actual, present := row[field]; present && reflect.DeepEqual(actual, value) {
				delete(row, field)
			}
		}
		// The fixed runtime-held source implies this exact error after metadata
		// inheritance. Keep standalone records self-contained and preserve any
		// distinct explicit error; eight held rows must fit the public cap even
		// with maximum JSON-escaped application preference strings.
		full := settings[key].(map[string]any)
		if full["source"] == "runtime-kept-preference-unsaved" {
			if full["persistence_error"] == "Runtime unsaved." {
				delete(row, "persistence_error")
			} else {
				// A different error must override the source implication even
				// when publicSettings removed an envelope-equal value.
				row["persistence_error"] = full["persistence_error"]
			}
		}
	}
	return rows, defaults, application
}

func sharedSettingMetadata(rows values, application bool, fields []string) values {
	result := values{}
	for _, field := range fields {
		counts := map[string]int{}
		original := values{}
		eligible, present := 0, 0
		for _, raw := range rows {
			row := raw.(map[string]any)
			if application && row["group"] != "application" {
				continue
			}
			eligible++
			value, ok := row[field]
			if !ok {
				continue
			}
			present++
			encoded, _ := json.Marshal(value) // Fixed scalar metadata only.
			key := string(encoded)
			counts[key]++
			original[key] = value
		}
		// A default must not manufacture fields absent from standalone records.
		if eligible < 2 || present != eligible {
			continue
		}
		keys := make([]string, 0, len(counts))
		for key := range counts {
			keys = append(keys, key)
		}
		sort.Strings(keys)
		best, count := "", 1
		for _, key := range keys {
			if counts[key] > count {
				best, count = key, counts[key]
			}
		}
		if count > 1 {
			result[field] = original[best]
		}
	}
	return result
}

func (d *Controller) settingsRequest(r values) values {
	if r == nil {
		return failure("invalid_request")
	}
	op := stringAt(r, "operation")
	fields := map[string][]string{
		"query": {"operation", "scope"}, "provider": {"operation"},
		"set":      {"operation", "provider", "generation", "values"},
		"validate": {"operation", "provider", "generation", "values"},
		"reset":    {"operation", "provider", "generation", "scope"},
		"reload":   {"operation", "provider", "generation"},
	}
	expected, ok := fields[op]
	if !ok {
		return failure("unsupported_operation")
	}
	if op == "query" || op == "provider" {
		if _, ok := r["provider"]; ok {
			expected = append(expected, "provider")
		}
	}
	if op == "query" {
		if _, ok := r["generation"]; ok {
			expected = append(expected, "generation")
			if _, ok = r["provider"]; !ok {
				return failure("invalid_request")
			}
		}
	}
	if !exact(r, expected...) {
		return failure("invalid_request")
	}
	if provider, ok := r["provider"]; ok {
		p, valid := provider.(string)
		if !valid || !identity(p, 32) {
			return failure("invalid_request")
		}
		if p != d.instance {
			return nil
		}
	}
	var persistence any
	if d.persistenceError != "" {
		persistence = d.persistenceError
	}
	if op == "provider" {
		lifecycle, available := "starting", "configuring"
		if d.ready {
			lifecycle, available = "ready", "available"
		}
		return success(values{"owner": "FvwmDesktop", "schema_version": 1, "provider": d.instance, "generation": d.generation,
			"lifecycle": lifecycle, "lifecycle_scope": "desktop-preferences", "availability": available,
			"failure_reason": nil, "persistence_error": persistence, "resources": []string{"settings", "operations", "session"},
			"operations": []string{"query", "validate", "set", "reset", "reload"}, "operation_targets": append([]string{"applications", "status"}, sessionOperationTargets()...),
			"operation_limit": 16, "operation_retention_ms": 300000, "notifications": "invalidation-only", "preview": false,
			"application_completion": "unknown", "service_lifecycle": d.sessionObserved()})
	}
	if !d.ready {
		return failure("provider_not_ready")
	}
	scope := "all"
	if raw, ok := r["scope"]; ok {
		var valid bool
		scope, valid = raw.(string)
		if !valid || len(scope) > 64 {
			return failure("invalid_request")
		}
	}
	_, hasGeneration := r["generation"]
	if op != "query" || hasGeneration {
		g, ok := r["generation"].(string)
		if !ok || !identity(g, 32) {
			return failure("invalid_request")
		}
		if op != "query" && g != d.generation {
			return failure("stale_generation")
		}
	}
	keys, err := desktopsettings.Scope(scope)
	if err != nil {
		return failure("invalid_value")
	}
	var changes, candidate, effective values
	if op == "set" || op == "validate" {
		changes, ok = r["values"].(map[string]any)
		if !ok || len(changes) == 0 || len(changes) > len(desktopsettings.Registry()) {
			return failure("invalid_value")
		}
		for key, value := range changes {
			if _, err := desktopsettings.Value(key, value, false); err != nil {
				return failure("invalid_value")
			}
		}
	} else if op == "reset" {
		changes = values{}
		defaults := desktopsettings.Defaults()
		for _, key := range keys {
			changes[key] = defaults[key]
		}
	}
	if changes != nil {
		candidate, effective, err = d.prepare(changes)
		if err != nil {
			return failure("invalid_value")
		}
	}
	if op == "query" && r["generation"] == d.generation {
		return success(values{"owner": "FvwmDesktop", "schema_version": 1, "provider": d.instance, "generation": d.generation,
			"state": "unchanged", "scope": scope, "snapshot_required": false, "preview": false, "persistence_error": persistence,
			"available": true, "capability": "owned-desktop-preference"})
	}
	if op == "validate" {
		settings, err := d.query("all", candidate, effective)
		if err != nil {
			return failure("invalid_value")
		}
		rows, defaults, applicationDefaults := compactPublicSettings(settings, d.persistenceError)
		return success(values{"owner": "FvwmDesktop", "provider": d.instance, "schema_version": 1, "operation": op, "state": "validated",
			"generation": d.generation, "saved_revision": d.revision, "settings": rows, "setting_defaults": defaults, "application_defaults": applicationDefaults, "persistence": "not-attempted", "persistence_error": persistence,
			"application": "not-submitted", "asynchronous_completion": "not-started", "preview": false,
			"available": true, "capability": "owned-desktop-preference"})
	}
	before := d.generation
	submissionFailed := false
	if op != "query" {
		if op == "reload" {
			err = d.reloadSettings()
		} else {
			err = d.changeSettings(changes)
		}
		if err != nil {
			if d.generation == before {
				return failure("persistence_failed")
			}
			submissionFailed = true
		}
	}
	settings, err := d.query(scope, nil, nil)
	if err != nil {
		return failure("invalid_value")
	}
	state := "committed"
	if op == "query" {
		state = "observed"
	}
	if op == "reload" {
		state = "reloaded"
	}
	application := "not-submitted"
	if d.lastTheme != nil {
		application = "submitted-to-fvwm"
	}
	if onlySession(changes) {
		application = "live-pending"
		if d.session.snapshot == nil && d.session.work == nil {
			application = "live-unavailable"
		}
		if len(changes) == 1 {
			if _, ok := changes["session_bus"]; ok {
				application = "next-login"
			}
		}
	}
	if submissionFailed {
		application = "submission-failed"
	}
	// Reload may clear an earlier common error. Project the current owner
	// state after the operation, rather than its admission-time snapshot.
	persistence = nil
	if d.persistenceError != "" {
		persistence = d.persistenceError
	}
	rows, defaults, applicationDefaults := compactPublicSettings(settings, d.persistenceError)
	return success(values{"owner": "FvwmDesktop", "provider": d.instance, "schema_version": 1, "operation": op, "generation": d.generation,
		"state": state, "settings": rows, "setting_defaults": defaults, "application_defaults": applicationDefaults, "saved_revision": d.revision, "application": application, "persistence_error": persistence,
		"asynchronous_completion": "unknown", "preview": false,
		"available": true, "capability": "owned-desktop-preference"})
}

type operation struct {
	id, generation, target, state, application, notified string
	cancelRequested                                      bool
	result                                               any
	err                                                  any
	finished                                             time.Time
}

func (d *Controller) expireOperations() {
	kept := d.operations[:0]
	for _, record := range d.operations {
		if record.finished.IsZero() || time.Since(record.finished) < 300*time.Second {
			kept = append(kept, record)
		}
	}
	d.operations = kept
}
func (d *Controller) operationSnapshot(r *operation, summary bool) values {
	var retention any
	if !r.finished.IsZero() {
		retention = max(int((300*time.Second-time.Since(r.finished))/time.Millisecond), 0)
	}
	v := values{"id": r.id, "generation": r.generation, "target": r.target, "state": r.state, "cancel_requested": r.cancelRequested, "retention_ms": retention}
	if !summary {
		for key, value := range (values{"progress": values{"completed": nil, "total": nil, "unit": "unknown"}, "application": r.application,
			"result": r.result, "error": r.err, "worker_stop": "unknown", "rollback": false}) {
			v[key] = value
		}
	}
	return v
}
func (d *Controller) announceOperation(r *operation) {
	if r.notified != r.generation {
		d.emit("SendToModule FvwmMFL MFL_OPERATION_1 " + d.instance + " " + r.id + " " + r.generation + " " + r.state)
		r.notified = r.generation
	}
}
func (d *Controller) operationChanged(r *operation, state string) {
	r.state = state
	r.generation = newID(16)
	if state == "completed" || state == "failed" || state == "cancelled" {
		r.finished = time.Now()
		d.announceOperation(r)
	}
}
func (d *Controller) operationRequest(r values) values {
	op := stringAt(r, "operation")
	keys := []string{"resource", "operation", "provider"}
	switch op {
	case "start", "cancel":
		keys = append(keys, "target", "generation")
	case "query":
		if _, ok := r["target"]; ok {
			keys = append(keys, "target")
		}
	default:
		return failure("unsupported_operation")
	}
	if !exact(r, keys...) || r["resource"] != "operations" {
		return failure("invalid_request")
	}
	for _, key := range []string{"provider", "generation"} {
		if v, ok := r[key]; ok {
			s, ok := v.(string)
			if !ok || !identity(s, 32) {
				return failure("invalid_request")
			}
		}
	}
	if r["provider"] != d.instance {
		return nil
	}
	if !d.ready {
		return failure("provider_not_ready")
	}
	d.expireOperations()
	target := ""
	if raw, ok := r["target"]; ok {
		var valid bool
		target, valid = raw.(string)
		if !valid {
			return failure("invalid_request")
		}
	}
	var record *operation
	if op == "start" {
		if sessionTarget(target) {
			return d.startSessionOperation(target, r)
		}
		if target != "applications" && target != "status" {
			return failure("unsupported_target")
		}
		if r["generation"] != d.generation {
			return failure("stale_generation")
		}
		key := target
		if target == "applications" {
			key = "apps"
		}
		if _, ok := d.jobs[key]; ok || len(d.jobs) >= 4 {
			return failure("operation_busy")
		}
		if len(d.operations) >= 16 {
			index := -1
			for i, r := range d.operations {
				if !r.finished.IsZero() {
					index = i
					break
				}
			}
			if index < 0 {
				return failure("operation_limit")
			}
			d.operations = append(d.operations[:index], d.operations[index+1:]...)
		}
		args := values{}
		if key == "status" {
			args["system"] = d.system
		}
		if !d.workerJob(key, key, args) {
			return failure("operation_unavailable")
		}
		record = &operation{id: newID(16), generation: newID(16), target: target, state: "accepted", application: "not-submitted"}
		d.operations = append(d.operations, record)
		d.operationJobs[key] = record
	} else {
		if _, ok := r["target"]; !ok {
			records := []values{}
			for _, r := range d.operations {
				records = append(records, d.operationSnapshot(r, true))
			}
			return success(values{"owner": "FvwmDesktop", "provider": d.instance, "state": "observed", "operations": records,
				"operation_limit": 16, "terminal_retention_ms": 300000, "disconnect": "continue"})
		}
		if !identity(target, 32) {
			return failure("invalid_request")
		}
		for _, r := range d.operations {
			if r.id == target {
				record = r
				break
			}
		}
		if record == nil {
			return failure("operation_unavailable")
		}
		if op == "cancel" {
			if r["generation"] != record.generation {
				return failure("stale_generation")
			}
			if !record.finished.IsZero() {
				return failure("operation_finished")
			}
			if sessionTarget(record.target) {
				if record.target != "logout" {
					return failure("cancellation_unavailable")
				}
				if d.session.logout == nil || d.session.logout.record != record {
					return failure("operation_unavailable")
				}
				if err := d.cancelLogout(); err != nil {
					return failure("cancellation_unavailable")
				}
				record.cancelRequested = true
				return success(values{"owner": "FvwmDesktop", "provider": d.instance, "operation": d.operationSnapshot(record, false), "disconnect": "continue"})
			}
			key := record.target
			if key == "applications" {
				key = "apps"
			}
			if !record.cancelRequested {
				record.cancelRequested = true
				d.operationChanged(record, record.state)
			}
			if d.cancelQueued(key) {
				delete(d.operationJobs, key)
				d.operationChanged(record, "cancelled")
			}
		}
	}
	return success(values{"owner": "FvwmDesktop", "provider": d.instance, "operation": d.operationSnapshot(record, false), "disconnect": "continue"})
}

// Replies travel as hexadecimal JSON over the private module channel, never as
// embedded HTML. Keep printable '<', '>' and '&' literal so a valid 128-byte
// font cannot consume six times its value length in the fixed reply budget.
// The standard encoder still escapes quotes, backslashes and control bytes.
func encodeProviderReply(value any) ([]byte, error) {
	var buffer bytes.Buffer
	encoder := json.NewEncoder(&buffer)
	encoder.SetEscapeHTML(false)
	if err := encoder.Encode(value); err != nil {
		return nil, err
	}
	return bytes.TrimSuffix(buffer.Bytes(), []byte{'\n'}), nil
}

func (d *Controller) ipc(argument string) {
	parts := strings.Split(argument, " ")
	if len(parts) != 3 || !address(parts[0]) || !identity(parts[1], 16) || parts[1] == "0000000000000000" {
		fmt.Fprintln(os.Stderr, "FvwmDesktop: invalid settings request address")
		return
	}
	var response values
	if len(parts[2]) == 0 || len(parts[2]) > 768 || len(parts[2])%2 != 0 || !identity(parts[2], len(parts[2])) {
		response = failure("invalid_request")
	} else {
		data, _ := hex.DecodeString(parts[2])
		decoded, err := desktopsettings.DecodeJSON(data)
		request, ok := decoded.(map[string]any)
		if err != nil || !ok {
			response = failure("invalid_request")
		} else if request["resource"] == "session" {
			response = d.sessionRequest(request)
		} else if request["resource"] == "operations" {
			response = d.operationRequest(request)
		} else {
			response = d.settingsRequest(request)
		}
	}
	if response == nil {
		return
	}
	data, _ := encodeProviderReply(response)
	if len(data) > 8192 {
		if result, ok := response["result"].(map[string]any); ok {
			if _, settings := result["settings"]; settings {
				delete(result, "settings")
				result["settings_unavailable"] = "reply_too_large"
				data, _ = encodeProviderReply(response)
			}
		}
		if len(data) > 8192 {
			data, _ = encodeProviderReply(failure("provider_reply_too_large"))
		}
	}
	prefix := "SendToModule FvwmMFL MFL_SETTINGS_1 " + parts[0] + " " + parts[1]
	count := 0
	for offset := 0; offset < len(data); offset += 256 {
		d.emit(fmt.Sprintf("%s part %d %s", prefix, count, hex.EncodeToString(data[offset:min(offset+256, len(data))])))
		count++
	}
	d.emit(fmt.Sprintf("%s end %d", prefix, count))
}
func address(s string) bool {
	if len(s) == 0 || len(s) > 64 {
		return false
	}
	for _, c := range s {
		if c != '-' && !(c >= '0' && c <= '9' || c >= 'a' && c <= 'f') {
			return false
		}
	}
	return true
}

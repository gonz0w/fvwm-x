// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"errors"
	"fmt"
	"io/fs"
	"os"
	"strings"
	"unicode/utf8"
)

const MaxRoleDiagnostics = 16
const MaxRoleDiagnosticPathBytes = 512
const MaxRoleDiagnosticMessageBytes = 256

type RoleDiagnosticCode string

const (
	RoleDiagnosticInvalidEntry RoleDiagnosticCode = "invalid-desktop-entry"
	RoleDiagnosticPermission   RoleDiagnosticCode = "inaccessible-optional-input"
	RoleDiagnosticSuppressed   RoleDiagnosticCode = "diagnostics-suppressed"
)

// RoleDiagnostic describes a rejected input, never a command or launch result.
// A lookup emits at most MaxRoleDiagnostics details and one omission summary.
type RoleDiagnostic struct {
	Code    RoleDiagnosticCode
	Path    string
	Message string
	Count   int
}

func roleDiagnosticText(value string, limit int) string {
	value = strings.ToValidUTF8(value, "\ufffd")
	if len(value) > limit {
		value = value[:limit]
		for !utf8.ValidString(value) {
			value = value[:len(value)-1]
		}
	}
	return value
}

// String quotes source-derived text so one diagnostic remains one stderr line.
func (d RoleDiagnostic) String() string {
	return fmt.Sprintf("%s path=%q message=%q count=%d", d.Code,
		roleDiagnosticText(d.Path, MaxRoleDiagnosticPathBytes),
		roleDiagnosticText(d.Message, MaxRoleDiagnosticMessageBytes), d.Count)
}

type roleDiagnostics struct {
	callback           func(RoleDiagnostic)
	delivered, omitted int
}

func newRoleDiagnostics(callback func(RoleDiagnostic)) *roleDiagnostics {
	return &roleDiagnostics{callback: callback}
}

func (d *roleDiagnostics) emit(value RoleDiagnostic) {
	if d.callback == nil {
		return
	}
	if d.delivered >= MaxRoleDiagnostics {
		d.omitted++
		return
	}
	d.delivered++
	value.Path = roleDiagnosticText(value.Path, MaxRoleDiagnosticPathBytes)
	value.Message = roleDiagnosticText(value.Message, MaxRoleDiagnosticMessageBytes)
	value.Count = 1
	d.callback(value)
}

func (d *roleDiagnostics) finish() {
	if d.callback != nil && d.omitted != 0 {
		d.callback(RoleDiagnostic{Code: RoleDiagnosticSuppressed,
			Message: "additional input diagnostics omitted", Count: d.omitted})
	}
}

func roleDiagnostic(options DirectoryRoleOptions, code RoleDiagnosticCode, path string, err error) {
	if options.Diagnostic != nil {
		options.Diagnostic(RoleDiagnostic{Code: code, Path: path, Message: err.Error()})
	}
}

// Optional search files may be absent or inaccessible. All other acquisition
// failures retain their original fatal meaning, including budget exhaustion,
// cancellation, special files and errors after a read has begun.
func (s *Source) readRoleOptional(path string, limit int, options DirectoryRoleOptions) ([]byte, error) {
	data, err := s.readLimited(path, limit)
	var pathError *os.PathError
	opening := errors.As(err, &pathError) && pathError.Op == "open" && pathError.Path == path
	if opening && errors.Is(err, fs.ErrNotExist) {
		return nil, nil
	}
	if opening && errors.Is(err, fs.ErrPermission) {
		roleDiagnostic(options, RoleDiagnosticPermission, path, err)
		return nil, nil
	}
	return data, err
}

// Filesystem errors while validating an entry's working directory are not
// malformed desktop metadata and must not become a successful partial result.
type roleHandlerAcquisitionError struct{ error }

func (e roleHandlerAcquisitionError) Unwrap() error { return e.error }

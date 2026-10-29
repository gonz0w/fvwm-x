// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"context"
	"errors"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

type classifierDiagnostic struct {
	size     int
	overflow bool
}

func (d *classifierDiagnostic) Write(value []byte) (int, error) {
	if len(value) > 4096-d.size {
		d.overflow = true
		d.size = 4096
	} else {
		d.size += len(value)
	}
	return len(value), nil
}

func classifierMIME(data []byte) (string, error) {
	value := string(data)
	if strings.HasSuffix(value, "\r\n") {
		value = strings.TrimSuffix(value, "\r\n")
	} else if strings.HasSuffix(value, "\n") {
		value = strings.TrimSuffix(value, "\n")
	}
	if len(data) > 256 {
		return "", errors.New("File type identification returned an invalid MIME type")
	}
	canonical, err := xdgmenu.NormalizeMIMEType(value)
	if err != nil {
		return "", errors.New("File type identification returned an invalid MIME type")
	}
	return canonical, nil
}

// Optional file(1) performs content work only in the isolated worker. Its
// inherited worker group is retired by the parent even after timeout or crash.
func classifyFile(ctx context.Context, path string) (string, error) {
	if !filepath.IsAbs(path) || strings.ContainsRune(path, 0) {
		return "", errors.New("Invalid captured file path")
	}
	program, err := exec.LookPath("file")
	if err != nil {
		return "", errors.New("Optional file(1) MIME classifier is unavailable")
	}
	ctx, cancel := context.WithTimeout(ctx, 2*time.Second)
	defer cancel()
	command := exec.Command(program, "-b", "--mime-type", path)
	diagnostics := &classifierDiagnostic{}
	command.Stderr = diagnostics
	data, err := ownedProcessOutput(ctx, command, 256, false)
	if ctx.Err() != nil {
		return "", errors.New("File type identification timed out or was cancelled")
	}
	if err != nil || diagnostics.overflow {
		return "", errors.New("File type identification failed or exceeded output limits")
	}
	return classifierMIME(data)
}

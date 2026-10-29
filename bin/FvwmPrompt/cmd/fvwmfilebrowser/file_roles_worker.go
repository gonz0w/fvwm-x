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

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

type fileRoleResult struct {
	directoryRoleResult
	MIME         string `json:"mime"`
	ObservedMIME string `json:"observed_mime"`
}

func validFileRoleResult(result *fileRoleResult) bool {
	if result == nil || !validDirectoryRoleResult(&result.directoryRoleResult) {
		return false
	}
	for _, value := range []string{result.MIME, result.ObservedMIME} {
		if parsed, err := classifierMIME([]byte(value)); err != nil || parsed != value {
			return false
		}
	}
	return true
}

func validateRoleFile(snapshot *filemodel.Snapshot, id string) (string, error) {
	if snapshot == nil {
		return "", errors.New("Missing captured file snapshot")
	}
	path, kind, err := snapshot.Validate(id)
	if err != nil {
		return "", errors.New("Selected file changed; refresh the browser folder and reselect the file")
	}
	regular := false
	for _, item := range snapshot.Items {
		if item.ID == id {
			regular = item.Identity.Mode.IsRegular()
			break
		}
	}
	if kind != "file" || !regular {
		return "", errors.New("Select exactly one regular file; links and special files are unavailable")
	}
	return path, nil
}

func performFileRole(request workRequest) (result workResult) {
	result.Schema, result.Status = 1, "failed"
	path, err := validateRoleFile(request.Snapshot, request.ID)
	if err != nil {
		result.Error = err.Error()
		return
	}
	defer func() {
		if result.Status == "started" {
			return
		}
		if current, err := validateRoleFile(request.Snapshot, request.ID); err != nil || current != path {
			result.Status, result.FileRoles, result.RoleFileValid = "failed", nil, false
			result.Error = "Selected file changed; refresh the browser folder and reselect the file"
			return
		}
		result.RoleFileValid = true
	}()
	observed, err := classifyFile(context.Background(), path)
	if err != nil {
		result.Error = err.Error()
		return
	}
	if _, err = validateRoleFile(request.Snapshot, request.ID); err != nil {
		result.Error = err.Error()
		return
	}
	if request.Operation == "file-role-launch" && observed != request.ObservedMIME {
		result.Error = "Selected file MIME type changed; refresh choices before launching"
		return
	}
	opts, err := currentRoleOptions(request.Program)
	if err != nil {
		result.Error = err.Error()
		return
	}
	roles, err := xdgsource.New(context.Background()).ApplicationRoles(observed, opts)
	if err != nil {
		result.Error = "File applications unavailable: " + err.Error()
		return
	}
	if request.Operation == "file-roles" {
		wire := &fileRoleResult{directoryRoleResult: directoryRoleResult{DefaultID: roles.DefaultID, Choices: make([]directoryRoleChoice, 0, len(roles.Handlers))}, MIME: roles.MIME, ObservedMIME: observed}
		for _, handler := range roles.Handlers {
			wire.Choices = append(wire.Choices, directoryRoleChoice{handler.ID, handler.Name, handler.Icon, handler.Fingerprint, handler.Terminal})
		}
		if !validFileRoleResult(wire) {
			result.Error = "File applications exceed result limits"
			return
		}
		result.Status, result.FileRoles = "roles", wire
		return
	}
	if request.RoleMIME != roles.MIME || request.RoleID == "" || len(request.RoleID) > filemodel.MaxPath || !hexToken(request.RoleFingerprint, 64) {
		result.Error = "File application type or choice changed; refresh choices before launching"
		return
	}
	var selected *xdgsource.DirectoryHandler
	for i := range roles.Handlers {
		if roles.Handlers[i].ID == request.RoleID && roles.Handlers[i].Fingerprint == request.RoleFingerprint {
			selected = &roles.Handlers[i]
			break
		}
	}
	if selected == nil {
		result.Error = "File application changed or is no longer associated; refresh choices"
		return
	}
	// The inherited expansion keeps raw target bytes and the exact URI contract;
	// a file's fallback cwd is its containing folder, never the file pathname.
	handler := *selected
	if handler.WorkingDir == "" {
		handler.WorkingDir = filepath.Dir(path)
	}
	argv, cwd, err := directoryLaunchArguments(handler, path, request.Program)
	if err != nil {
		result.Error = err.Error()
		return
	}
	if _, err = validateRoleFile(request.Snapshot, request.ID); err != nil {
		result.Error = err.Error()
		return
	}
	result.RoleFileValid = true
	command := exec.Command(argv[0], argv[1:]...)
	detachApplication(command)
	command.Dir = cwd
	if err := command.Start(); err != nil {
		result.Error = "Could not start file application"
		return
	}
	command.Process.Release()
	result.Status, result.ActionCount = "started", 1
	return
}

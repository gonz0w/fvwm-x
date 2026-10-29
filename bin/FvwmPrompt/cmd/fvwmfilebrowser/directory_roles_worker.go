// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"net/url"
	"os"
	"os/exec"
	"strings"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

const directoryRoleMax = 128

// Only presentation/captured identity crosses worker JSON. Launch metadata is
// reread and expanded in this worker so raw target bytes never become JSON text.
type directoryRoleChoice struct {
	ID          string `json:"id"`
	Name        string `json:"name"`
	Icon        string `json:"icon"`
	Fingerprint string `json:"fingerprint"`
	Terminal    bool   `json:"terminal"`
}
type directoryRoleResult struct {
	Choices   []directoryRoleChoice `json:"choices"`
	DefaultID string                `json:"default_id"`
}

func roleToken(choice directoryRoleChoice) string {
	sum := sha256.Sum256([]byte(choice.ID + "\x00" + choice.Fingerprint))
	return hex.EncodeToString(sum[:16])
}

func currentRoleOptions(terminal string) (xdgsource.DirectoryRoleOptions, error) {
	home, err := os.UserHomeDir()
	if err != nil {
		return xdgsource.DirectoryRoleOptions{}, err
	}
	env := make(map[string]string)
	for _, pair := range os.Environ() {
		key, value, ok := strings.Cut(pair, "=")
		if ok {
			env[key] = value
		}
	}
	return xdgsource.DirectoryRoleOptions{Home: home, Environment: env, Terminal: terminal,
		Executable: func(program string) bool { _, err := exec.LookPath(program); return err == nil }}, nil
}

func validDirectoryRoleResult(result *directoryRoleResult) bool {
	if result == nil || len(result.Choices) > directoryRoleMax {
		return false
	}
	seen := make(map[string]bool, len(result.Choices))
	total := 0
	hasDefault := result.DefaultID == ""
	for _, choice := range result.Choices {
		if choice.ID == "" || len(choice.ID) > filemodel.MaxPath || !utf8.ValidString(choice.ID) || strings.ContainsRune(choice.ID, 0) || seen[choice.ID] ||
			!hexToken(choice.Fingerprint, 64) || len(choice.Name) > xdgmenu.MaxValueBytes || !utf8.ValidString(choice.Name) || strings.ContainsRune(choice.Name, 0) ||
			len(choice.Icon) > xdgmenu.MaxValueBytes || !utf8.ValidString(choice.Icon) || strings.ContainsRune(choice.Icon, 0) {
			return false
		}
		seen[choice.ID] = true
		total += 6*(len(choice.ID)+len(choice.Name)+len(choice.Icon)) + 256
		if total > 1024*1024 {
			return false
		}
		hasDefault = hasDefault || choice.ID == result.DefaultID
	}
	return hasDefault
}

func directoryLaunchArguments(handler xdgsource.DirectoryHandler, path, terminal string) ([]string, string, error) {
	uri := (&url.URL{Scheme: "file", Path: path}).String()
	decoded, err := filemodel.ParseTarget(uri)
	if err != nil || string(decoded) != path {
		return nil, "", errors.New("Could not encode the captured local folder")
	}
	argv, err := xdgmenu.ExpandExec(handler.Exec, xdgmenu.ExecContext{Name: handler.Name, Icon: handler.Icon, Location: handler.DesktopPath, Files: []string{path}, URLs: []string{uri}})
	if err != nil {
		return nil, "", err
	}
	if handler.Terminal {
		if terminal == "" {
			return nil, "", errors.New("No configured terminal is available")
		}
		if _, err := exec.LookPath(terminal); err != nil {
			return nil, "", errors.New("No configured terminal is available")
		}
		argv = append([]string{terminal, "-e"}, argv...)
	}
	if len(argv) > 4096 {
		return nil, "", errors.New("Folder application arguments exceed limit")
	}
	total := 0
	for _, arg := range argv {
		if len(arg) > 64*1024-total {
			return nil, "", errors.New("Folder application arguments exceed limit")
		}
		total += len(arg)
	}
	cwd := handler.WorkingDir
	if cwd == "" {
		cwd = path
	}
	return argv, cwd, nil
}

func performDirectoryRole(request workRequest) (result workResult) {
	result.Schema, result.Status = 1, "failed"
	if request.Snapshot == nil {
		result.Error = "Missing captured folder snapshot"
		return
	}
	path, err := request.Snapshot.RootPath()
	if err != nil {
		result.Error = err.Error()
		return
	}
	defer func() {
		// Role metadata failure is not a failed directory scan. Keep the
		// existing collection current only after this worker revalidates its
		// captured root; no application success is inferred from this flag.
		if result.Status == "started" {
			return
		}
		if _, err := request.Snapshot.RootPath(); err != nil {
			result.Status, result.Roles, result.Error, result.RoleFolderValid = "failed", nil, err.Error(), false
			return
		}
		result.RoleFolderValid = true
	}()
	opts, err := currentRoleOptions(request.Program)
	if err != nil {
		result.Error = err.Error()
		return
	}
	roles, err := xdgsource.New(context.Background()).DirectoryRoles(opts)
	if err != nil {
		result.Error = "Folder applications unavailable: " + err.Error()
		return
	}
	if request.Operation == "directory-roles" {
		wire := &directoryRoleResult{DefaultID: roles.DefaultID, Choices: make([]directoryRoleChoice, 0, len(roles.Handlers))}
		for _, handler := range roles.Handlers {
			wire.Choices = append(wire.Choices, directoryRoleChoice{handler.ID, handler.Name, handler.Icon, handler.Fingerprint, handler.Terminal})
		}
		if !validDirectoryRoleResult(wire) {
			result.Error = "Folder applications exceed result limits"
			return
		}
		result.Status, result.Roles = "roles", wire
		return
	}
	if request.RoleID == "" || len(request.RoleID) > filemodel.MaxPath || !hexToken(request.RoleFingerprint, 64) {
		result.Error = "Invalid captured folder application"
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
		result.Error = "Folder application changed or is no longer associated; refresh choices"
		return
	}
	argv, cwd, err := directoryLaunchArguments(*selected, path, request.Program)
	if err != nil {
		result.Error = err.Error()
		return
	}
	// The same identity is checked again after discovery, immediately before the
	// handoff. A path race cannot be made atomic with an external application.
	if _, err = request.Snapshot.RootPath(); err != nil {
		result.Error = err.Error()
		return
	}
	result.RoleFolderValid = true
	command := exec.Command(argv[0], argv[1:]...)
	detachApplication(command)
	command.Dir = cwd
	if err := command.Start(); err != nil {
		result.Error = "Could not start folder application"
		return
	}
	command.Process.Release()
	result.Status, result.ActionCount = "started", 1
	return
}

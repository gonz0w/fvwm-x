// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"io"
	"os/exec"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

// Private parent/worker data. Raw paths are base64 JSON bytes, never commands.
type workRequest struct {
	Operation       string              `json:"operation"`
	Targets         [][]byte            `json:"target_bytes,omitempty"`
	Path            []byte              `json:"path_bytes,omitempty"`
	Hidden          bool                `json:"hidden"`
	Snapshot        *filemodel.Snapshot `json:"snapshot,omitempty"`
	ID              string              `json:"id,omitempty"`
	IDs             []string            `json:"ids,omitempty"`
	Program         string              `json:"program,omitempty"`
	RoleID          string              `json:"role_id,omitempty"`
	RoleFingerprint string              `json:"role_fingerprint,omitempty"`
}

type workResult struct {
	Schema          int                  `json:"schema_version"`
	Selected        []string             `json:"selected_ids,omitempty"`
	Status          string               `json:"status"`
	Snapshot        *filemodel.Snapshot  `json:"snapshot,omitempty"`
	Error           string               `json:"error,omitempty"`
	ActionCount     int                  `json:"action_count,omitempty"`
	Roles           *directoryRoleResult `json:"directory_roles,omitempty"`
	RoleFolderValid bool                 `json:"role_folder_valid,omitempty"`
}

func decode(data []byte, value any) error {
	if len(data) > filemodel.MaxWire {
		return errors.New("worker record exceeds limit")
	}
	d := json.NewDecoder(bytes.NewReader(data))
	d.DisallowUnknownFields()
	if err := d.Decode(value); err != nil {
		return err
	}
	if d.Decode(new(any)) != io.EOF {
		return errors.New("trailing worker data")
	}
	return nil
}

func perform(request workRequest) (result workResult) {
	result.Schema = 1
	result.Status = "failed"
	if request.Operation == "directory-roles" || request.Operation == "directory-role-launch" {
		return performDirectoryRole(request)
	}
	if request.Operation == "locate" {
		snapshot, ids, err := filemodel.Locate(request.Targets, request.Hidden)
		if err != nil {
			result.Error = err.Error()
			return
		}
		result.Status, result.Snapshot, result.Selected = "loaded", snapshot, ids
		return
	}
	if request.Operation == "custom" {
		paths, err := request.Snapshot.ValidateSelection(request.IDs)
		if err != nil {
			result.Error = err.Error()
			return
		}
		return launch(request, paths)
	}
	path := string(request.Path)
	if request.Operation == "activate" || request.Operation == "browse" || request.Operation == "terminal" {
		if request.Snapshot == nil {
			result.Error = "Missing current file snapshot"
			return
		}
		if request.Operation == "activate" || request.Operation == "browse" {
			var kind string
			var err error
			path, kind, err = request.Snapshot.Validate(request.ID)
			if err != nil {
				result.Error = err.Error()
				return
			}
			if request.Operation == "browse" && kind != "directory" {
				result.Error = "Target is not a directory"
				return
			}
			if kind == "directory" {
				request.Operation = "scan"
			}
		} else {
			var err error
			path, err = request.Snapshot.RootPath()
			if err != nil {
				result.Error = err.Error()
				return
			}

		}
	}
	if request.Operation == "scan" {
		snapshot, err := filemodel.Scan(path, request.Hidden)
		if err != nil {
			result.Error = err.Error()
			return
		}
		result.Status, result.Snapshot = "loaded", snapshot
		return
	}
	if request.Operation != "activate" && request.Operation != "terminal" {
		result.Error = "Unsupported file action"
		return
	}
	if request.Program == "" {
		result.Error = "No configured opener/terminal is available"
		return
	}
	args := []string{path}
	if request.Operation == "terminal" {
		args = nil
	}
	return launch(request, args)
}

func launch(request workRequest, args []string) (result workResult) {
	result.Schema, result.Status = 1, "failed"
	if request.Program == "" || len(request.Program) > filemodel.MaxPath || strings.ContainsRune(request.Program, 0) {
		result.Error = "Invalid configured application"
		return
	}
	command := exec.Command(request.Program, args...)
	command.Dir = string(request.Snapshot.Path)
	if err := command.Start(); err != nil {
		result.Error = "Could not start the configured application"
		return
	}
	// Launch is a handoff, not proof of application completion or opened pixels.
	command.Process.Release()
	result.Status, result.ActionCount = "started", len(args)
	return
}

func runWorker(input io.Reader, output io.Writer) error {
	data, err := io.ReadAll(io.LimitReader(input, filemodel.MaxWire+1))
	if err != nil {
		return err
	}
	var request workRequest
	if err := decode(data, &request); err != nil {
		return err
	}
	data, err = json.Marshal(perform(request))
	if err != nil || len(data) > filemodel.MaxWire {
		return errors.New("worker output exceeds limit")
	}
	_, err = output.Write(append(data, '\n'))
	return err
}

// One child, one result slot, one replacement request. A blocked kernel call
// can delay reaping; cancellation never frees the slot until Wait completes.
func startWorker(ctx context.Context, executable string, request workRequest) workResult {
	data, err := json.Marshal(request)
	if err != nil || len(data) > filemodel.MaxWire {
		return workResult{Schema: 1, Status: "failed", Error: "File request exceeds limit"}
	}
	command := exec.CommandContext(ctx, executable, "--worker")
	command.Stdin = bytes.NewReader(data)
	command.WaitDelay = time.Second
	stdout, err := command.StdoutPipe()
	if err != nil {
		return workResult{Schema: 1, Status: "failed", Error: "Worker output is unavailable"}
	}
	if err := command.Start(); err != nil {
		if ctx.Err() != nil {
			return workResult{Schema: 1, Status: "cancelled", Error: "File operation cancelled or timed out"}
		}
		return workResult{Schema: 1, Status: "failed", Error: "Could not start file worker"}
	}
	data, readErr := io.ReadAll(io.LimitReader(stdout, filemodel.MaxWire+1))
	if readErr != nil || len(data) > filemodel.MaxWire {
		command.Process.Kill()
	}
	err = command.Wait()
	if ctx.Err() != nil {
		return workResult{Schema: 1, Status: "cancelled", Error: "File operation cancelled or timed out"}
	}
	var result workResult
	if err != nil || readErr != nil || decode(data, &result) != nil || result.Schema != 1 {
		return workResult{Schema: 1, Status: "failed", Error: "File worker failed or returned invalid data"}
	}
	return result
}

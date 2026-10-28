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
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/internal/iconrender"
	"github.com/fvwmorg/FvwmPrompt/internal/pinboard"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

const workerBytes = 8 * 1024 * 1024
const workerLimit = 10 * time.Second

type observedPin struct {
	ID     string                `json:"id"`
	Target filemodel.Observation `json:"target"`
}

type workRequest struct {
	Operation   string                 `json:"operation"`
	Path        string                 `json:"state_path,omitempty"`
	State       pinboard.State         `json:"state"`
	Expected    uint64                 `json:"expected_revision,omitempty"`
	Observation *filemodel.Observation `json:"observation,omitempty"`
	Program     string                 `json:"program,omitempty"`
	Browser     string                 `json:"browser,omitempty"`
	Socket      string                 `json:"socket,omitempty"`
	Home        string                 `json:"home,omitempty"`
	Environment map[string]string      `json:"environment,omitempty"`
	Theme       string                 `json:"theme,omitempty"`
	Extensions  []string               `json:"extensions,omitempty"`
	Cache       string                 `json:"cache,omitempty"`
	Owner       string                 `json:"owner,omitempty"`
	Cells       []pinboard.Cell        `json:"cells,omitempty"`
	Pin         *pinboard.Pin          `json:"pin,omitempty"`
	Layout      *pinboard.Layout       `json:"layout_changes,omitempty"`
}

type workResult struct {
	Schema   int               `json:"schema_version"`
	Status   string            `json:"status"`
	State    *pinboard.State   `json:"state,omitempty"`
	Observed []observedPin     `json:"observed,omitempty"`
	Pictures map[string]string `json:"pictures,omitempty"`
	Warnings []string          `json:"warnings,omitempty"`
	Error    string            `json:"error,omitempty"`
}

func observe(state pinboard.State) ([]observedPin, error) {
	if err := state.Validate(); err != nil {
		return nil, err
	}
	observed := make([]observedPin, 0, len(state.Pins))
	for _, p := range state.Pins {
		target, err := filemodel.ObserveTarget(p.Target)
		if err != nil {
			return nil, err
		}
		observed = append(observed, observedPin{p.ID, target})
	}
	return observed, nil
}

func perform(ctx context.Context, request workRequest) (result workResult) {
	result.Schema, result.Status = 1, "failed"
	fail := func(err error) workResult { result.Error = err.Error(); return result }
	switch request.Operation {
	case "load":
		if err := prepareStore(request.Path); err != nil {
			return fail(err)
		}
		state, err := pinboard.Load(request.Path)
		if err != nil {
			return fail(err)
		}
		result.State = &state
		result.Observed, err = observe(state)
		if err != nil {
			return fail(err)
		}
		result.Status = "loaded"
	case "save":
		state, err := pinboard.Save(request.Path, request.Expected, request.State)
		if err != nil {
			return fail(err)
		}
		result.State = &state
		// Persistence succeeded even if subsequent observation is unavailable.
		// Observations encode those per-reference failures as explicit states.
		result.Observed, err = observe(state)
		if err != nil {
			return fail(err)
		}
		result.Status = "saved"
	case "pin":
		if request.Pin == nil {
			return fail(errors.New("missing pin reference"))
		}
		if err := prepareStore(request.Path); err != nil {
			return fail(err)
		}
		state, err := pinboard.Load(request.Path)
		if err != nil {
			return fail(err)
		}
		next, err := state.Change("create", *request.Pin)
		if err != nil {
			return fail(err)
		}
		if request.Layout != nil {
			if request.Layout.CellWidth != 0 {
				next.Layout.CellWidth = request.Layout.CellWidth
			}
			if request.Layout.CellHeight != 0 {
				next.Layout.CellHeight = request.Layout.CellHeight
			}
			if request.Layout.IconSize != 0 {
				next.Layout.IconSize = request.Layout.IconSize
			}
		}
		next, err = pinboard.Save(request.Path, state.Revision, next)
		if err != nil {
			return fail(err)
		}
		result.State = &next
		result.Status = "saved"
	case "observe":
		var err error
		result.Observed, err = observe(request.State)
		if err != nil {
			return fail(err)
		}
		result.Status = "observed"
	case "open":
		if request.Observation == nil {
			return fail(errors.New("target has no current observation; refresh"))
		}
		path, kind, err := request.Observation.Validate()
		if err != nil {
			return fail(err)
		}
		program, args, err := launchPlan(request, path, kind)
		if err != nil {
			return fail(err)
		}
		command := exec.Command(program, args...)
		if kind == "directory" {
			command.Dir = path
		} else {
			command.Dir = filepath.Dir(path)
		}
		if err := command.Start(); err != nil {
			return fail(errors.New("configured application is unavailable: " + err.Error()))
		}
		// Explicit process-created handoff, as in the file browser. The
		// application is no longer owned by the disposable acquisition worker.
		command.Process.Release()
		result.Status = "started"
	case "icons":
		if len(request.Cells) > 8 {
			return fail(errors.New("icon page exceeds eight cells"))
		}
		if !filepath.IsAbs(request.Cache) || filepath.Clean(request.Cache) != request.Cache || len(request.Cache) > 256 {
			return fail(errors.New("invalid private icon generation directory"))
		}
		if err := pinboard.PrepareImages(request.Cache, request.Owner); err != nil {
			return fail(err)
		}
		if err := request.State.Validate(); err != nil {
			return fail(err)
		}
		paths, err := xdgmenu.SearchDirectories(request.Home, request.Environment)
		if err != nil {
			return fail(err)
		}
		source := xdgsource.New(ctx)
		renderer, err := iconrender.New(source, paths, request.Home, iconrender.Config{CacheDir: request.Cache, Extensions: request.Extensions})
		if err != nil {
			return fail(err)
		}
		result.Pictures = map[string]string{}
		seen := map[string]bool{}
		for _, cell := range request.Cells {
			if err := ctx.Err(); err != nil {
				return fail(err)
			}
			i := request.State.Index(cell.ID)
			if i < 0 || cell.Index != i || seen[cell.ID] || cell.Quarters < 4 || cell.Quarters > 16 {
				return fail(errors.New("invalid icon cell identity"))
			}
			seen[cell.ID] = true
			if !cell.Visible {
				continue
			}
			pin := request.State.Pins[i]
			name := "text-x-generic"
			observation, err := filemodel.ObserveTarget(pin.Target)
			if err == nil && observation.Status == "present" && observation.Kind == "directory" {
				name = "folder"
			}
			picture, err := renderer.Resolve(xdgmenu.IconRequest{Name: name, Override: pin.Icon, Fallback: "text-x-generic", Theme: request.Theme, State: "normal", Size: request.State.Layout.IconSize, ScaleQuarters: cell.Quarters})
			if err != nil {
				result.Warnings = append(result.Warnings, "Named icon unavailable for "+cell.ID)
				continue
			}
			if len(picture) <= 384 {
				result.Pictures[pin.ID] = picture
			}
		}
		if err := pinboard.CheckImages(request.Cache, request.Owner); err != nil {
			return fail(err)
		}
		result.Status = "icons"
	case "retire-images":
		if err := pinboard.RetireImages(request.Cache, request.Owner); err != nil {
			return fail(err)
		}
		result.Status = "retired-images"
	default:
		return fail(errors.New("unsupported pinboard operation"))
	}
	return result
}

func prepareStore(path string) error {
	if !filepath.IsAbs(path) || filepath.Clean(path) != path || len(path) > filemodel.MaxPath || strings.ContainsRune(path, 0) {
		return errors.New("invalid state path")
	}
	return os.MkdirAll(filepath.Dir(path), 0700)
}

func launchPlan(request workRequest, path, kind string) (string, []string, error) {
	program := request.Program
	args := []string{path}
	if kind == "directory" {
		program = request.Browser
		args = []string{"--reuse", "--dir", path}
		if request.Socket != "" {
			args = append(args, "--socket", request.Socket)
		}
	} else if kind != "file" {
		return "", nil, errors.New("target type cannot be opened")
	}
	if program == "" {
		if kind == "directory" {
			program = "fvwm-file-browser"
		} else {
			program = "xdg-open"
		}
	}
	if len(program) > filemodel.MaxPath || strings.ContainsRune(program, 0) {
		return "", nil, errors.New("invalid configured executable")
	}
	// Lookup and all eventual path validation are worker-owned. Nothing is
	// interpreted by a shell, FVWM command parser or desktop-entry launcher.
	resolved, err := exec.LookPath(program)
	if err != nil {
		return "", nil, errors.New("configured opener/browser is unavailable")
	}
	return resolved, args, nil
}

func runWorker(input io.Reader, output io.Writer) error {
	data, err := io.ReadAll(io.LimitReader(input, workerBytes+1))
	if err != nil {
		return err
	}
	if len(data) > workerBytes {
		return errors.New("pinboard worker input exceeds limit")
	}
	var request workRequest
	if err := pinboard.DecodeStrict(data, &request); err != nil {
		return err
	}
	ctx, cancel := context.WithTimeout(context.Background(), workerLimit)
	defer cancel()
	data, err = json.Marshal(perform(ctx, request))
	if err != nil {
		return err
	}
	if len(data) > workerBytes {
		return errors.New("pinboard worker output exceeds limit")
	}
	_, err = output.Write(append(data, '\n'))
	return err
}

// No queue/replacement slot: the owner retains its active slot until this
// function has killed (when needed) and actually waited for its one child.
func startWorker(ctx context.Context, executable string, request workRequest) workResult {
	failure := workResult{Schema: 1, Status: "failed", Error: "Pinboard worker failed; edit outcome may be unknown. Reload before retrying edits"}
	data, err := json.Marshal(request)
	if err != nil || len(data) > workerBytes {
		return failure
	}
	command := exec.CommandContext(ctx, executable, "--worker")
	command.Stdin = bytes.NewReader(data)
	command.WaitDelay = time.Second
	stdout, err := command.StdoutPipe()
	if err != nil {
		return failure
	}
	if err := command.Start(); err != nil {
		return failure
	}
	data, readErr := io.ReadAll(io.LimitReader(stdout, workerBytes+1))
	if readErr != nil || len(data) > workerBytes {
		command.Process.Kill()
	}
	err = command.Wait()
	if ctx.Err() != nil {
		failure.Status = "cancelled"
		failure.Error = "Pinboard operation cancelled or timed out; worker reaped"
		return failure
	}
	var result workResult
	if err != nil || readErr != nil || len(data) > workerBytes || pinboard.DecodeStrict(data, &result) != nil || result.Schema != 1 {
		return failure
	}
	return result
}

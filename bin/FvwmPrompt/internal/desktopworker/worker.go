// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package desktopworker owns nonresident desktop acquisition and base-system
// commands. Results are data only; this package never writes FVWM commands.
package desktopworker

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"runtime"
	"strings"
	"time"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopcheck"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopinspect"
	"github.com/fvwmorg/FvwmPrompt/transport"
)

const RequestLimit = 64 * 1024
const ResultLimit = 1024 * 1024
const CommandLimit = 64 * 1024
const Deadline = 4 * time.Second

// Request is a private one-shot protocol, not an installed configuration API.
type Request struct {
	Version     int             `json:"version"`
	Operation   string          `json:"operation"`
	System      string          `json:"system,omitempty"`
	Action      string          `json:"action,omitempty"`
	Directories []string        `json:"directories,omitempty"`
	Check       json.RawMessage `json:"check,omitempty"`
	Inspect     json.RawMessage `json:"inspect,omitempty"`
	Path        string          `json:"path,omitempty"`
}

type Response struct {
	Version int    `json:"version"`
	Result  any    `json:"result,omitempty"`
	Error   string `json:"error,omitempty"`
}

func Decode(data []byte) (Request, error) {
	var request Request
	if len(data) > RequestLimit || !utf8.Valid(data) {
		return request, errors.New("invalid desktop worker request size or encoding")
	}
	// encoding/json normally accepts duplicate fields. Refuse ambiguous operation
	// or action ownership before decoding the typed request.
	decoder := json.NewDecoder(bytes.NewReader(data))
	token, err := decoder.Token()
	if err != nil || token != json.Delim('{') {
		return request, errors.New("expected request object")
	}
	seen := map[string]bool{}
	allowed := map[string]bool{"version": true, "operation": true, "system": true, "action": true, "directories": true, "check": true, "inspect": true, "path": true}
	for decoder.More() {
		token, err = decoder.Token()
		key, ok := token.(string)
		if err != nil || !ok || !allowed[key] || seen[key] {
			return request, errors.New("duplicate request field")
		}
		seen[key] = true
		var value json.RawMessage
		if err = decoder.Decode(&value); err != nil {
			return request, err
		}
		if bytes.Equal(bytes.TrimSpace(value), []byte("null")) {
			return request, errors.New("null request field")
		}
	}
	if _, err = decoder.Token(); err != nil {
		return request, err
	}
	var trailing any
	if err = decoder.Decode(&trailing); err != io.EOF {
		return request, errors.New("trailing request data")
	}
	decoder = json.NewDecoder(bytes.NewReader(data))
	decoder.DisallowUnknownFields()
	if err = decoder.Decode(&request); err != nil {
		return request, err
	}
	if request.Operation == "inspect" && (len(seen) != 3 || !seen["version"] || !seen["operation"] || !seen["inspect"]) {
		return request, errors.New("unexpected inspector worker field")
	}
	return validateRequest(request)
}

func validateRequest(request Request) (Request, error) {
	// Refuse counts before copying/scanning, including direct Go callers.
	if len(request.Directories) > 64 {
		return request, errors.New("too many application directories")
	}

	if request.Version != 1 {
		return request, errors.New("unsupported desktop worker protocol")
	}
	for _, value := range append([]string{request.System, request.Action, request.Path}, request.Directories...) {
		if len(value) > 4096 || strings.ContainsRune(value, 0) {
			return request, errors.New("invalid request value")
		}
	}

	if request.Operation == "inspect" {
		if request.System != "" || request.Action != "" || request.Path != "" || len(request.Directories) != 0 || request.Check != nil || request.Inspect == nil {
			return request, errors.New("invalid inspector request")
		}
		_, err := desktopinspect.DecodeRequest(request.Inspect)
		return request, err
	}
	if request.Inspect != nil {
		return request, errors.New("unexpected inspector payload")
	}

	if request.System == "" {
		request.System = map[string]string{"linux": "Linux", "freebsd": "FreeBSD", "openbsd": "OpenBSD"}[runtime.GOOS]
	}
	if request.Operation != "apps" && request.Operation != "check" && request.Operation != "files" && request.System != "Linux" && request.System != "FreeBSD" && request.System != "OpenBSD" {
		return request, errors.New("unsupported system")
	}

	if request.Operation != "check" && request.Check != nil {
		return request, errors.New("unexpected configuration check")
	}
	if request.Operation != "files" && request.Path != "" {
		return request, errors.New("unexpected file menu path")
	}
	switch request.Operation {
	case "files":
		if request.Path == "" || request.Action != "" || len(request.Directories) != 0 {
			return request, errors.New("invalid file menu request")
		}
	case "check":
		if request.Action != "" || len(request.Directories) != 0 || request.Check == nil {
			return request, errors.New("invalid configuration check request")
		}
		if _, err := desktopcheck.DecodeRequest(request.Check); err != nil {
			return request, err
		}
	case "apps":
		if request.Action != "" {
			return request, errors.New("apps has no action")
		}
	case "status":
		if request.Action != "" || len(request.Directories) != 0 {
			return request, errors.New("status has no arguments")
		}
	case "volume", "power":
		if len(request.Directories) != 0 {
			return request, errors.New("action has no directories")
		}
		valid := request.Action == "up" || request.Action == "down" || request.Action == "mute"
		if request.Operation == "power" {
			valid = request.Action == "poweroff" || request.Action == "reboot"
		}
		if !valid {
			return request, errors.New("invalid desktop action")
		}
	default:
		return request, errors.New("unknown desktop operation")
	}
	return request, nil
}

type Runner func(context.Context, []string, time.Duration) (string, error)

type Environment struct {
	Run      Runner
	LookPath func(string) (string, error)
	Read     func(string) ([]byte, error)
	Entries  func(string) ([]string, error)
	Query    func(context.Context, string) (json.RawMessage, error)
}

func NativeEnvironment() Environment {
	endpoint := transport.SocketPath("", os.Getenv, os.Getuid())
	return Environment{Run: command, LookPath: exec.LookPath, Read: readBounded, Entries: entryNames,
		Query: func(ctx context.Context, text string) (json.RawMessage, error) {
			return transport.Query(ctx, endpoint, text)
		}}
}

var errOutputLimit = errors.New("desktop command output exceeds limit")

// ErrAbandonedJob requires the executable owner to retire its process group.
var ErrAbandonedJob = errors.New("desktop backend exceeded its ownership budget")

type boundedOutput struct {
	bytes.Buffer
	overflow bool
}

func (b *boundedOutput) Write(p []byte) (int, error) {
	if len(p) > CommandLimit-b.Len() {
		b.overflow = true
		return 0, errOutputLimit
	}
	return b.Buffer.Write(p)
}

func command(ctx context.Context, argv []string, timeout time.Duration) (string, error) {
	ctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()
	cmd := exec.CommandContext(ctx, argv[0], argv[1:]...)
	cmd.Env = append(os.Environ(), "LC_ALL=C")
	// A descendant holding the stdout pipe cannot extend this worker forever.
	// The supervising controller owns the worker process group on outer failure.
	cmd.WaitDelay = 100 * time.Millisecond
	var output boundedOutput
	cmd.Stdout = &output
	err := cmd.Run()
	if output.overflow {
		return "", errOutputLimit
	}
	if ctx.Err() != nil {
		return "", ctx.Err()
	}
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(strings.ToValidUTF8(output.String(), "\uFFFD")), nil
}

func Run(ctx context.Context, request Request, env Environment) (any, error) {
	request, err := validateRequest(request)
	if err != nil {
		return nil, err
	}
	var abandoned bool
	runner := env.Run
	env.Run = func(ctx context.Context, argv []string, deadline time.Duration) (string, error) {
		value, err := runner(ctx, argv, deadline)
		if errors.Is(err, context.DeadlineExceeded) || errors.Is(err, context.Canceled) || errors.Is(err, exec.ErrWaitDelay) || errors.Is(err, errOutputLimit) {
			abandoned = true
		}
		return value, err
	}
	var value any
	switch request.Operation {
	case "inspect":
		inspection, _ := desktopinspect.DecodeRequest(request.Inspect)
		value, err = desktopinspect.Collect(ctx, inspection, env.Query)
	case "files":
		value, err = fileMenu(ctx, request.Path)
	case "check":
		check, _ := desktopcheck.DecodeRequest(request.Check)
		value, err = desktopcheck.Check(ctx, check)
	case "apps":
		value, err = applications(ctx, request.Directories, env)
	case "status":
		value = collectStatus(ctx, request.System, env)
	case "volume":
		value = volume(ctx, request.System, request.Action, env)
	case "power":
		value = power(ctx, request.System, request.Action, env)
	default:
		err = fmt.Errorf("unknown desktop operation %q", request.Operation)
	}
	if abandoned {
		return nil, ErrAbandonedJob
	}
	return value, err
}

func output(ctx context.Context, env Environment, argv ...string) string {
	value, err := env.Run(ctx, argv, time.Second)
	if err != nil {
		return ""
	}
	return value
}
func available(env Environment, name string) bool { _, err := env.LookPath(name); return err == nil }

func volume(ctx context.Context, system, action string, env Environment) string {
	if system == "OpenBSD" && available(env, "sndioctl") {
		setting := "output.level=-0.05"
		if action == "up" {
			setting = "output.level=+0.05"
		}
		if action == "mute" {
			current := output(ctx, env, "sndioctl", "-n", "output.mute")
			if current != "0" && current != "1" {
				return ""
			}
			setting = "output.mute=1"
			if current == "1" {
				setting = "output.mute=0"
			}
		}
		return output(ctx, env, "sndioctl", setting)
	}
	if system == "FreeBSD" && available(env, "mixer") {
		setting := map[string]string{"up": "vol.volume=+5%", "down": "vol.volume=-5%", "mute": "vol.mute=toggle"}[action]
		return output(ctx, env, "mixer", setting)
	}
	if available(env, "amixer") {
		return output(ctx, env, "amixer", "sset", "Master", map[string]string{"up": "5%+", "down": "5%-", "mute": "toggle"}[action])
	}
	return ""
}

func power(ctx context.Context, system, action string, env Environment) string {
	flag := "-r"
	if action == "poweroff" {
		flag = "-p"
	}
	argv := []string{"shutdown", flag, "now"}
	if system == "Linux" && available(env, "systemctl") {
		argv = []string{"systemctl", action}
	}
	_, err := env.Run(ctx, argv, 3*time.Second)
	if err == nil {
		return "Power request sent."
	}
	var failure *exec.ExitError
	if errors.As(err, &failure) {
		return "Power request denied by the system. Check session permissions."
	}
	return "Power service is unavailable or did not respond."
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
	"unicode"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/runtimepath"
	"github.com/fvwmorg/FvwmPrompt/internal/sessionowner"
)

// Set by the install/build owner as a fallback for build-tree invocation.
// Installed images prefer their fixed sibling, never PATH or environment.
var wmPath string
var busHelperPath string

const usage = "usage: fvwm-session [--no-autostart] [-- FVWM_ARGS...]\n"

func options(args []string) (bool, []string, error) {
	disabled := false
	for i, arg := range args {
		switch arg {
		case "--no-autostart":
			if disabled {
				return false, nil, errors.New("duplicate --no-autostart")
			}
			disabled = true
		case "--":
			return disabled, append([]string(nil), args[i+1:]...), nil
		default:
			return false, nil, fmt.Errorf("unknown wrapper argument %q (place FVWM arguments after --)", arg)
		}
	}
	return disabled, nil, nil
}

func corePath(configured, self string) (string, error) {
	adjacent := filepath.Join(filepath.Dir(self), "fvwm3")
	if _, err := os.Lstat(adjacent); err == nil {
		info, err := os.Stat(adjacent)
		if err != nil || !info.Mode().IsRegular() || info.Mode().Perm()&0111 == 0 || syscall.Access(adjacent, 1) != nil {
			return "", errors.New("adjacent fvwm3 is not an executable regular file")
		}
		return adjacent, nil
	} else if !os.IsNotExist(err) {
		return "", err
	}
	if configured == "" {
		return "", errors.New("adjacent fvwm3 is unavailable and no build path is configured")
	}
	if !filepath.IsAbs(configured) || filepath.Clean(configured) != configured {
		return "", errors.New("build-configured fvwm3 path must be absolute and clean")
	}
	return configured, nil
}

func reportScanError(output io.Writer, err error) {
	message := "scan refused: " + strings.Map(func(c rune) rune {
		if unicode.IsControl(c) {
			return ' '
		}
		return c
	}, err.Error())
	if len(message) > 511 {
		message = message[:511]
		for !utf8.ValidString(message) {
			message = message[:len(message)-1]
		}
	}
	fmt.Fprintln(output, message)
}

func run(args, environment []string, diagnostics io.Writer) int {
	if len(args) == 1 && args[0] == sessionowner.ProviderWorkerArgument {
		if err := sessionowner.ProviderDiscovery(environment, os.Stdout); err != nil {
			reportScanError(diagnostics, err)
			return 1
		}
		return 0
	}
	if len(args) == 1 && args[0] == sessionowner.WorkerArgument {
		if err := sessionowner.Scan(context.Background(), environment, os.Stdout); err != nil {
			reportScanError(diagnostics, err)
			return 1
		}
		return 0
	}
	if len(args) == 1 && (args[0] == "--help" || args[0] == "-h") {
		fmt.Fprint(diagnostics, usage)
		return 0
	}
	disabled, wmArgs, err := options(args)
	if err != nil {
		fmt.Fprintln(diagnostics, "fvwm-session:", err)
		fmt.Fprint(diagnostics, usage)
		return 2
	}
	self, err := runtimepath.Executable(os.Args[0])
	if err != nil {
		fmt.Fprintln(diagnostics, "fvwm-session: self path unavailable:", err)
		return 1
	}
	core, err := corePath(wmPath, self)
	if err != nil {
		fmt.Fprintln(diagnostics, "fvwm-session:", err)
		return 1
	}
	signals := make(chan os.Signal, 4)
	signal.Notify(signals, syscall.SIGTERM, syscall.SIGINT, syscall.SIGHUP, syscall.SIGQUIT)
	defer signal.Stop(signals)
	return sessionowner.Run(sessionowner.Config{WMPath: core, SelfPath: self, Args: wmArgs, Environment: environment, NoAutostart: disabled, Signals: signals, Diagnostics: diagnostics, BusHelperPath: busHelperPath})
}

func main() { os.Exit(run(os.Args[1:], os.Environ(), os.Stderr)) }

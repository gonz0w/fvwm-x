// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"runtime"
	"strconv"
	"syscall"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopcheck"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopcontroller"
	"github.com/fvwmorg/FvwmPrompt/internal/runtimepath"
)

var dataDirectory = "/usr/local/share/fvwm3"
var pngEnabled = "true"

func environment(key, fallback string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return fallback
}
func helperBeside(directory, name string) string {
	path := filepath.Join(directory, name)
	if _, err := os.Stat(path); err == nil {
		return path
	}
	if name == "fvwm-file-browser" {
		// The build peer is adjacent, but installs put this optional
		// application in bindir and FvwmDesktop in the module directory.
		if command, err := exec.LookPath(name); err == nil {
			return command
		}
	}
	if name == "fvwm-desktop-capture" {
		// Meson keeps the C helper in bin, while Go executables live in
		// bin/FvwmPrompt. Installed helpers remain adjacent and win above.
		fallback := filepath.Join(directory, "..", name)
		if info, err := os.Stat(fallback); err == nil && info.Mode().IsRegular() {
			return fallback
		}
	}
	return path
}

func adjacent(name, override string) (string, error) {
	if value := os.Getenv(override); value != "" {
		return value, nil
	}
	self, err := runtimepath.Executable(os.Args[0])
	if err != nil {
		return "", err
	}
	return helperBeside(filepath.Dir(self), name), nil
}

func check(ctx context.Context, args []string, out, stderr io.Writer) int {
	flags := flag.NewFlagSet("FvwmDesktop", flag.ContinueOnError)
	flags.SetOutput(stderr)
	path := flags.String("check-config", "", "literal configuration file")
	jsonOutput := flags.Bool("json", false, "JSON diagnostics")
	if err := flags.Parse(args); err != nil || *path == "" || flags.NArg() != 0 {
		return 2
	}
	home, _ := os.UserHomeDir()
	cwd, err := os.Getwd()
	if err != nil {
		fmt.Fprintln(stderr, err)
		return 1
	}
	request := desktopcheck.Request{Path: *path, Userdir: environment("FVWM_USERDIR", filepath.Join(home, ".fvwm")), Datadir: environment("FVWM_DATADIR", dataDirectory), Cwd: cwd}
	helper, err := adjacent("fvwm-desktop-worker", "FVWM_DESKTOP_WORKER")
	if err != nil {
		fmt.Fprintln(stderr, "FvwmDesktop: "+err.Error())
		return 1
	}
	result, err := desktopcontroller.CheckConfiguration(ctx, helper, request)
	if err != nil {
		fmt.Fprintln(stderr, "FvwmDesktop: "+err.Error())
		return 1
	}
	printReport(out, result, *jsonOutput)
	if result.Errors > 0 {
		return 1
	}
	if result.Unchecked > 0 {
		return 2
	}
	return 0
}
func printReport(out io.Writer, result desktopcheck.Result, jsonOutput bool) {
	if jsonOutput {
		data, _ := json.Marshal(result)
		fmt.Fprintln(out, string(data))
	} else {
		fmt.Fprintln(out, "Desktop settings subset only; configuration was not executed.")
		for _, item := range result.Diagnostics {
			path, _ := json.Marshal(item.Path)
			span := strconv.Itoa(item.Line)
			if item.EndLine != item.Line {
				span += "-" + strconv.Itoa(item.EndLine)
			}
			fmt.Fprintln(out, string(path)+":"+span+": "+item.Severity+": "+item.Message)
			for i := len(item.IncludedFrom) - 1; i >= 0; i-- {
				parent := item.IncludedFrom[i]
				path, _ := json.Marshal(parent.Path)
				span := strconv.Itoa(parent.Line)
				if parent.EndLine != parent.Line {
					span += "-" + strconv.Itoa(parent.EndLine)
				}
				fmt.Fprintf(out, "  Read from %s:%s\n", path, span)
			}
		}
		fmt.Fprintf(out, "%s: %d checked directives, %d errors, %d unchecked\n", result.Status, result.CheckedDirectives, result.Errors, result.Unchecked)
	}
}

func run(ctx context.Context, args []string) error {
	if len(args) < 5 {
		return fmt.Errorf("FvwmDesktop must be started with Module FvwmDesktop.")
	}
	output, err := strconv.Atoi(args[0])
	if err != nil {
		return err
	}
	input, err := strconv.Atoi(args[1])
	if err != nil {
		return err
	}
	home, _ := os.UserHomeDir()
	data := environment("FVWM_DATADIR", dataDirectory)
	system := map[string]string{"linux": "Linux", "freebsd": "FreeBSD", "openbsd": "OpenBSD"}[runtime.GOOS]
	worker, err := adjacent("fvwm-desktop-worker", "FVWM_DESKTOP_WORKER")
	if err != nil {
		return err
	}
	capture, err := adjacent("fvwm-desktop-capture", "FVWM_DESKTOP_CAPTURE")
	if err != nil {
		return err
	}
	// The optional browser peer is checked only when its explicit action runs.
	browser, _ := adjacent("fvwm-file-browser", "FVWM_FILE_BROWSER")
	return desktopcontroller.Run(desktopcontroller.Options{Context: ctx, Root: environment("FVWM_DESKTOP_ROOT", filepath.Join(data, "default-config")), StatePath: filepath.Join(environment("FVWM_USERDIR", filepath.Join(home, ".fvwm")), "desktop-state.json"), System: system, Worker: worker, Capture: capture, Browser: browser, PNG: pngEnabled != "false", Input: input, Output: output})
}
func main() {
	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGTERM, syscall.SIGINT, syscall.SIGHUP)
	defer stop()
	// An ignored SIGCHLD can survive exec from a module launcher. Install a
	// non-reaping Go handler before starting workers so Wait4 retains ownership.
	childSignals := make(chan os.Signal, 1)
	signal.Notify(childSignals, syscall.SIGCHLD)
	defer signal.Stop(childSignals)

	if len(os.Args) > 1 && os.Args[1] == "--check-config" {
		os.Exit(check(ctx, os.Args[1:], os.Stdout, os.Stderr))
	}
	if err := run(ctx, os.Args[1:]); err != nil {
		fmt.Fprintln(os.Stderr, "FvwmDesktop: "+err.Error())
		os.Exit(1)
	}
}

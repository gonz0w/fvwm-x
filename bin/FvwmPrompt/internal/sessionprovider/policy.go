// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package sessionprovider selects fixed native commands without executing them.
// The caller owns executable discovery, process lifetime and authorization.
package sessionprovider

import (
	"errors"
	"path/filepath"
	"strings"
	"unicode/utf8"
)

func resolve(lookPath func(string) (string, error), name string) (string, error) {
	if lookPath == nil {
		return "", errors.New("executable discovery unavailable")
	}
	path, err := lookPath(name)
	if err != nil {
		return "", err
	}
	if len(path) > 4096 || !utf8.ValidString(path) || !filepath.IsAbs(path) || filepath.Clean(path) != path || strings.ContainsAny(path, "\x00\r\n") {
		return "", errors.New("executable must have a clean absolute path")
	}
	return path, nil
}

// NativePlan returns a literal command for poweroff or reboot. An installed
// tool does not establish permission, and a failed request must not be replayed.
func NativePlan(system, action string, lookPath func(string) (string, error)) ([]string, error) {
	if action != "poweroff" && action != "reboot" {
		return nil, errors.New("unsupported power action")
	}
	if system != "Linux" && system != "FreeBSD" && system != "OpenBSD" {
		return nil, errors.New("unsupported power platform")
	}
	if system == "Linux" {
		if path, err := resolve(lookPath, "systemctl"); err == nil {
			return []string{path, action}, nil
		}
	}
	path, err := resolve(lookPath, "shutdown")
	if err != nil {
		return nil, err
	}
	flag := "-r"
	if action == "poweroff" {
		flag = "-p"
	}
	return []string{path, flag, "now"}, nil
}

// LockerPlan discovers a fixed locker. Successful discovery, process start and
// process lifetime never establish secure readiness for coordinated suspend.
func LockerPlan(lookPath func(string) (string, error)) ([]string, error) {
	for _, name := range []string{"xlock", "slock"} {
		if path, err := resolve(lookPath, name); err == nil {
			return []string{path}, nil
		}
	}
	return nil, errors.New("no supported locker installed")
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"errors"
	"strings"
)

// effectiveDisplay mirrors only the argument arity of fvwm3.c's main option
// loop. Values stay literal; in particular a command/configuration value that
// looks like -d must not become an option here. Unknown future core options
// require an explicit direct-core fallback rather than guessed ownership.
func effectiveDisplay(args []string, inherited string) (string, error) {
	display := inherited
	for i := 0; i < len(args); i++ {
		switch args[i] {
		case "-d", "-display", "--display":
			i++
			if i >= len(args) {
				return "", errors.New("missing core display option value")
			}
			display = args[i]
			// XOpenDisplay("") uses the inherited DISPLAY, just like NULL.
			if display == "" {
				display = inherited
			}
		case "-f", "-c", "-cmd", "--cmd",
			"-i", "-clientid", "--clientid", "-clientId", "--clientId",
			"-F", "-restore", "--restore",
			"-I", "-visualid", "--visualid", "-visualId", "--visualId",
			"-C", "-visual", "--visual",
			"-l", "-color-limit", "--color-limit", "-o", "--output-file":
			i++
			if i >= len(args) {
				return "", errors.New("missing core option value")
			}
		case "-debug_stack_ring", "--debug-stack-ring",
			"-r", "-replace", "--replace",
			"-L", "-strict-color-limit", "--strict-color-limit",
			"-A", "-allocate-palette", "--allocate-palette",
			"-S", "-static-palette", "--static-palette",
			"-P", "-visual-palette", "--visual-palette",
			"-v", "--verbose":
		case "-h", "-?", "--help", "-V", "-version", "--version":
			return "", errors.New("core informational invocation has no session startup")
		default:
			return "", errors.New("unrecognized core option; display selection is uncertain")
		}
	}
	return display, nil
}

func displayEnvironment(environment []string, display string) []string {
	result := make([]string, 0, len(environment)+1)
	for _, entry := range environment {
		if key, _, _ := strings.Cut(entry, "="); key != "DISPLAY" {
			result = append(result, entry)
		}
	}
	return append(result, "DISPLAY="+display)
}

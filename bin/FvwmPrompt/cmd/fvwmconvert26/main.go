// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"fmt"
	"io"
	"os"
	"strings"
)

const usage = "fvwm-convert-2.6 [-f] [-h] source-file destination-file\n"

func run(args []string, out, diagnostics io.Writer) int {
	var files []string
	follow, positional := false, false
	for _, arg := range args {
		if positional || arg == "-" || !strings.HasPrefix(arg, "-") {
			files = append(files, arg)
			continue
		}
		if arg == "--" {
			positional = true
			continue
		}
		option := strings.ToLower(strings.TrimPrefix(strings.TrimPrefix(arg, "-"), "-"))
		name, _, value := strings.Cut(option, "=")
		switch {
		case !value && (name == "h" || (name != "" && strings.HasPrefix("help", name))):
			fmt.Fprint(out, usage)
			return 0
		case !value && (name == "f" || (name != "" && strings.HasPrefix("follow-read", name))):
			follow = true
		default:
			if value && (name == "h" || name == "f" || strings.HasPrefix("help", name) || strings.HasPrefix("follow-read", name)) {
				if name == "h" || strings.HasPrefix("help", name) {
					name = "help"
				} else {
					name = "follow-read"
				}
				fmt.Fprintf(diagnostics, "Option %s does not take an argument\n", name)
			} else {
				fmt.Fprintf(diagnostics, "Unknown option: %s\n", name)
			}
			fmt.Fprint(out, usage)
			return 0
		}
	}
	if len(files) < 1 || len(files) > 2 {
		fmt.Fprint(out, usage)
		return 0
	}
	job, err := newJob(follow, out, diagnostics)
	if err != nil {
		fmt.Fprintln(diagnostics, err)
		return 1
	}
	destination := ""
	if len(files) == 2 {
		destination = files[1]
	}
	if err := job.process(files[0], destination); err != nil {
		fmt.Fprintln(diagnostics, err)
		return errorStatus(err)
	}
	return 0
}

func main() {
	os.Exit(run(os.Args[1:], os.Stdout, os.Stderr))
}

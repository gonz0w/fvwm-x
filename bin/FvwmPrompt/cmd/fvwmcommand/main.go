// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// FvwmCommand sends commands through the same bounded transport as Prompt.
package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/transport"
)

var version = "development"

type options struct {
	stdin, info, detailed   bool
	socket, action, request string
	commands                []string
}

func usage(out io.Writer, getenv func(string) string) {
	name := getenv("FVWMMFL_SOCKET")
	if name == "" {
		name = "None"
	}
	fmt.Fprintln(out, `Usage: FvwmCommand [OPTION] [COMMAND]...
Send commands to fvwm3 via FvwmMFL

  -c                  read and send commands from stdin
                      The COMMAND if any is ignored.
  -f <file name>      use unix socket <file name> to connect to FvwmMFL
  -i <info level>     0 - default
                      >0 - print FvwmMFL feedback
  -d                  print detailed diagnostic messages
  -v                  print version number
  --request JSON      send one structured MFL request, print its correlated reply

Default unix socket name is:`, name)
}

func parse(args []string) (options, error) {
	var o options
	long := []string{"help", "version", "verbose", "cmds", "detailed", "info", "socket", "request"}
	for len(args) != 0 {
		arg := args[0]
		args = args[1:]
		if arg == "--" {
			o.commands = args
			return o, nil
		}
		if arg == "-" || !strings.HasPrefix(arg, "-") {
			o.commands = append([]string{arg}, args...)
			return o, nil
		}
		letters := arg[1:]
		value, supplied := "", false
		if strings.HasPrefix(arg, "--") {
			name := arg[2:]
			if i := strings.IndexByte(name, '='); i >= 0 {
				value, supplied, name = name[i+1:], true, name[:i]
			}
			matches := []string{}
			for _, candidate := range long {
				if candidate == name {
					matches = []string{candidate}
					break
				}
				if name != "" && strings.HasPrefix(candidate, name) {
					matches = append(matches, candidate)
				}
			}
			if len(matches) != 1 {
				return o, fmt.Errorf("unknown or ambiguous option --%s", name)
			}
			letters = map[string]string{"help": "h", "version": "v", "verbose": "d",
				"cmds": "c", "detailed": "d", "info": "i", "socket": "f", "request": "R"}[matches[0]]
			if supplied && letters != "i" && letters != "f" && letters != "R" {
				return o, fmt.Errorf("option --%s does not take a value", name)
			}
		}
		for len(letters) != 0 {
			letter := letters[0]
			letters = letters[1:]
			if letter == 'i' || letter == 'f' || letter == 'w' || letter == 'R' {
				if !supplied {
					if letters != "" {
						value, letters = letters, ""
					} else if len(args) != 0 {
						value, args = args[0], args[1:]
					} else {
						return o, fmt.Errorf("option -%c requires a value", letter)
					}
				}
			}
			switch letter {
			case 'h':
				o.action = "help"
			case 'v':
				o.action = "version"
			case 'c':
				o.stdin = true
			case 'd':
				o.detailed = true
			case 'i':
				o.info = value != "0"
			case 'f':
				o.socket = value
			case 'R':
				if o.request != "" || value == "" {
					return o, errors.New("request requires one nonempty JSON object")
				}
				o.request = value
			case 'F', 'm', 'r', 'S', 'w':
				// Retained FVWM2 options intentionally have no effect.
			default:
				return o, fmt.Errorf("option -%c not recognized", letter)
			}
			supplied, value = false, ""
		}
	}
	return o, nil
}

// Socket discovery mirrors the current MFL producer; it does not probe the
// retired shared /tmp/fvwmmfl directory. Explicit endpoints take precedence.
func socketPath(explicit string, getenv func(string) string, uid int) string {
	return transport.SocketPath(explicit, getenv, uid)
}

func execute(o options, input io.Reader, output io.Writer, getenv func(string) string) error {
	if o.request != "" {
		if o.stdin || o.info || o.detailed || len(o.commands) != 0 {
			return errors.New("request mode cannot be combined with commands, stdin or feedback options")
		}
		return requestOnce(o, output, getenv)
	}
	endpoint := socketPath(o.socket, getenv, os.Getuid())
	if o.detailed {
		fmt.Fprintln(output, "Connecting to", endpoint)
	}
	conn, err := net.DialTimeout("unix", endpoint, transport.WriteTimeout)
	if err != nil {
		return fmt.Errorf("unable to connect to FvwmMFL (start Module FvwmMFL): %w", err)
	}
	session := transport.New(conn)
	first := make(chan json.RawMessage, 1)
	finished := make(chan error, 1)
	readerDone := make(chan struct{})
	go func() {
		defer close(readerDone)
		seen := false
		finished <- session.Read(func(record json.RawMessage) error {
			if !seen {
				first <- record
				seen = true
			}
			return nil
		})
	}()
	// The reader drains unsolicited events during piped input without retaining
	// an event queue. Join it on every exit; Close interrupts a blocked read.
	defer func() { session.Close(); <-readerDone }()
	if o.stdin {
		scanner := bufio.NewScanner(input)
		scanner.Buffer(make([]byte, transport.MaxCommand+2), transport.MaxCommand+2)
		for scanner.Scan() {
			if err := session.Send(scanner.Text()); err != nil {
				return err
			}
		}
		if err := scanner.Err(); err != nil {
			return fmt.Errorf("read command input: %w", err)
		}
	} else if err := session.Send(strings.Join(o.commands, " ")); err != nil {
		return err
	}
	deadline := time.NewTimer(transport.WriteTimeout)
	defer deadline.Stop()
	var record json.RawMessage
	select {
	case record = <-first:
	case err = <-finished:
		// Resolve profile/EOF races by preferring a complete first object
		// already published.
		select {
		case record = <-first:
		default:
			if err == nil {
				err = errors.New("MFL disconnected before its first JSON object")
			}
			return err
		}
	case <-deadline.C:
		return errors.New("timed out waiting for MFL JSON (5 seconds)")
	}
	if o.detailed {
		fmt.Fprintln(output, "Sent commands; received a complete MFL JSON object")
	}
	if o.info {
		decoder := json.NewDecoder(bytes.NewReader(record))
		decoder.UseNumber()
		var object any
		if err := decoder.Decode(&object); err != nil {
			return err
		}
		encoder := json.NewEncoder(output)
		encoder.SetIndent("", "    ")
		encoder.SetEscapeHTML(false)
		return encoder.Encode(object)
	}
	return nil
}

func run(args []string, input io.Reader, output, diagnostic io.Writer, getenv func(string) string) int {
	o, err := parse(args)
	if err != nil {
		fmt.Fprintln(diagnostic, err)
		usage(output, getenv)
		return 2
	}
	switch o.action {
	case "help":
		usage(output, getenv)
		return 0
	case "version":
		fmt.Fprintln(output, "FvwmCommand", version)
		return 0
	}
	if err := execute(o, input, output, getenv); err != nil {
		fmt.Fprintln(diagnostic, "FvwmCommand:", err)
		return 1
	}
	return 0
}

func main() {
	os.Exit(run(os.Args[1:], os.Stdin, os.Stdout, os.Stderr, os.Getenv))
}

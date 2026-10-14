// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"net"
	"os"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
	"time"

	"github.com/ergochat/readline"
	"github.com/fatih/color"
	"github.com/fvwmorg/FvwmPrompt/transport"
)

type connectionProfileData struct {
	ConnectionProfile *struct {
		Version     string `json:"version"`
		VersionInfo string `json:"version_info"`
	} `json:"connection_profile"`
}

func profileEndError(ready <-chan struct{}, err error) error {
	if err != nil {
		return err
	}
	// The reader can publish the profile and EOF before startup selects.
	// A clean EOF after a profile is normal, whichever channel wins.
	select {
	case <-ready:
		return nil
	default:
		return fmt.Errorf("MFL disconnected before its connection profile")
	}
}

func run() error {
	prompt := flag.String("p", ">>> ", "The prompt")
	flag.Parse()
	conn, err := net.DialTimeout("unix", os.Getenv("FVWMMFL_SOCKET"), transport.WriteTimeout)
	if err != nil {
		return fmt.Errorf("unable to connect to FvwmMFL (start Module FvwmMFL): %w", err)
	}
	session := transport.New(conn)
	defer session.Close()
	if flag.NArg() != 0 {
		// One-shot commands complete their write before the process exits.
		// MFL has no execution acknowledgement for ordinary commands.
		return session.Send(strings.Join(flag.Args(), " "))
	}

	history := ""
	if userdir := os.Getenv("FVWM_USERDIR"); userdir != "" {
		history = filepath.Join(userdir, ".FvwmConsole-History")
	}
	red := color.New(color.FgRed).SprintFunc()
	rl, err := readline.NewFromConfig(&readline.Config{
		Prompt: red(*prompt), HistoryFile: history,
	})
	if err != nil {
		return fmt.Errorf("initialise readline: %w", err)
	}
	defer rl.Close()

	signals := make(chan os.Signal, 1)
	signal.Notify(signals, os.Interrupt, syscall.SIGTERM)
	defer signal.Stop(signals)
	signalDone := make(chan struct{})
	defer close(signalDone)
	go func() {
		select {
		case <-signals:
			session.Close()
			rl.Close()
		case <-signalDone:
		}
	}()

	ready := make(chan struct{})
	readerDone := make(chan error, 1)
	go func() {
		shown := false
		err := session.Read(func(record json.RawMessage) error {
			var cp connectionProfileData
			if err := json.Unmarshal(record, &cp); err != nil {
				return err
			}
			if cp.ConnectionProfile != nil && !shown {
				shown = true
				fmt.Fprintf(rl.Stderr(), "*FvwmPrompt %s (%s)\n\n",
					cp.ConnectionProfile.Version, cp.ConnectionProfile.VersionInfo)
				fmt.Fprintln(rl.Stderr(), "Press ^D or type 'exit' to end this session")
				close(ready)
			}
			return nil
		})
		readerDone <- err
		rl.Close()
	}()
	// Avoid a sleep-based startup race; the initial profile is a bounded wait.
	timer := time.NewTimer(transport.WriteTimeout)
	defer timer.Stop()
	select {
	case <-ready:
	case err := <-readerDone:
		return profileEndError(ready, err)
	case <-timer.C:
		session.Close()
		<-readerDone
		return fmt.Errorf("timed out waiting for the MFL connection profile")
	}

	interruptCount := 0
	for {
		line, err := rl.ReadLine()
		if err == readline.ErrInterrupt {
			interruptCount++
			if interruptCount < 2 {
				fmt.Fprintln(rl.Stderr(), "Input Ctrl-c once more to exit")
				continue
			}
			break
		}
		if err != nil {
			break
		}
		interruptCount = 0
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		if line == "exit" {
			break
		}
		if strings.EqualFold(line, "Quit") {
			fmt.Fprintln(rl.Stderr(), "Quit the window manager? [y/N]")
			answer, err := rl.ReadLine()
			if err != nil {
				break
			}
			if !strings.EqualFold(strings.TrimSpace(answer), "y") {
				continue
			}
		}
		if err := session.Send(line); err != nil {
			// A rejected local command leaves the connection usable.
			fmt.Fprintln(rl.Stderr(), "FvwmPrompt:", err)
			select {
			case <-session.Done():
				session.Close()
				<-readerDone
				return err
			default:
			}
		}
	}
	session.Close()
	err = <-readerDone
	if err != nil && !errors.Is(err, net.ErrClosed) {
		return err
	}
	return nil
}

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, "FvwmPrompt:", err)
		os.Exit(1)
	}
}

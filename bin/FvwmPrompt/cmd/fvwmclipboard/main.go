// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"strconv"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/clipboard"
	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
)

const usage = "usage: fvwm-clipboard [status | list [--offset N] [--limit N] | pin|unpin|remove --epoch E --item I --generation G | clear --epoch E | help]\n"

type requester func(context.Context, sessionservice.RequestMessage) (sessionservice.Reply, error)

type number struct {
	value uint64
	set   bool
}

func (n *number) String() string { return strconv.FormatUint(n.value, 10) }
func (n *number) Set(text string) error {
	if n.set || text == "" || len(text) > 1 && text[0] == '0' {
		return errors.New("expected one decimal integer")
	}
	for _, r := range text {
		if r < '0' || r > '9' {
			return errors.New("expected a decimal integer")
		}
	}
	value, err := strconv.ParseUint(text, 10, 64)
	if err != nil {
		return errors.New("decimal integer exceeds uint64")
	}
	n.value, n.set = value, true
	return nil
}

type options struct {
	command                                string
	epoch, item, generation, offset, limit number
}

func parse(args []string) (options, error) {
	opt := options{command: "status", limit: number{value: clipboard.MaxPage}}
	if len(args) == 0 {
		return opt, nil
	}
	opt.command = args[0]
	flags := flag.NewFlagSet("fvwm-clipboard", flag.ContinueOnError)
	flags.SetOutput(io.Discard)
	switch opt.command {
	case "status":
	case "list":
		flags.Var(&opt.offset, "offset", "configured slot offset")
		flags.Var(&opt.limit, "limit", "configured slots per page")
	case "pin", "unpin", "remove":
		flags.Var(&opt.epoch, "epoch", "native owner epoch")
		flags.Var(&opt.item, "item", "exact item ID")
		flags.Var(&opt.generation, "generation", "exact item generation")
	case "clear":
		flags.Var(&opt.epoch, "epoch", "native owner epoch")
	default:
		return opt, errors.New("unknown metadata command")
	}
	if err := flags.Parse(args[1:]); err != nil {
		return opt, err
	}
	if flags.NArg() != 0 {
		return opt, errors.New("unexpected command arguments")
	}
	if opt.command == "list" && (opt.offset.value > 4096 || opt.limit.value == 0 || opt.limit.value > clipboard.MaxPage) {
		return opt, errors.New("list needs offset 0..4096 and limit 1..16")
	}
	if opt.command == "pin" || opt.command == "unpin" || opt.command == "remove" {
		if opt.epoch.value == 0 || opt.item.value == 0 || opt.generation.value == 0 {
			return opt, errors.New("action needs exact positive epoch/item/generation")
		}
	}
	if opt.command == "clear" && opt.epoch.value == 0 {
		return opt, errors.New("clear needs the exact positive owner epoch")
	}
	return opt, nil
}

func run(args []string, output, diagnostics io.Writer, request requester) int {
	if len(args) == 1 && (args[0] == "help" || args[0] == "--help" || args[0] == "-h") {
		fmt.Fprint(output, usage)
		return 0
	}
	opt, err := parse(args)
	if err != nil {
		fmt.Fprintln(diagnostics, "fvwm-clipboard:", err)
		fmt.Fprint(diagnostics, usage)
		return 2
	}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	call := func(message sessionservice.RequestMessage) (sessionservice.Reply, error) {
		status, err := request(ctx, sessionservice.RequestMessage{Version: sessionservice.Version, Verb: "status"})
		if err != nil {
			return sessionservice.Reply{}, errors.New("session control request failed")
		}
		if !sessionservice.Identity(status.Generation) || !sessionservice.Identity(status.Operation) {
			return sessionservice.Reply{}, errors.New("invalid session control identity")
		}
		message.Version, message.Generation, message.Operation = sessionservice.Version, status.Generation, status.Operation
		reply, err := request(ctx, message)
		if err != nil {
			return sessionservice.Reply{}, errors.New("session control request failed")
		}
		return reply, nil
	}
	emit := func(value any, success bool) int {
		if err := json.NewEncoder(output).Encode(value); err != nil {
			fmt.Fprintln(diagnostics, "fvwm-clipboard: output failed")
			return 1
		}
		if success {
			return 0
		}
		return 1
	}
	capabilities := func() (*clipboard.Capabilities, error) {
		reply, err := call(sessionservice.RequestMessage{Verb: "clipboard-status"})
		if err != nil {
			return nil, err
		}
		if reply.ClipboardCapabilities == nil || reply.ClipboardCapabilities.Validate() != nil {
			return nil, errors.New("invalid clipboard capability reply")
		}
		return reply.ClipboardCapabilities, nil
	}
	fail := func(err error) int { fmt.Fprintln(diagnostics, "fvwm-clipboard:", err); return 1 }
	if opt.command == "status" || opt.command == "list" {
		caps, err := capabilities()
		if err != nil {
			return fail(err)
		}
		if opt.command == "status" {
			return emit(caps, true)
		}
		if caps.State != "ready" {
			return emit(caps, false)
		}
		pageRequest := clipboard.PageRequest{Epoch: caps.Epoch, Offset: opt.offset.value, Limit: opt.limit.value}
		reply, err := call(sessionservice.RequestMessage{Verb: "clipboard-list", ClipboardPage: &pageRequest})
		if err != nil {
			return fail(err)
		}
		if reply.ClipboardPage == nil || reply.ClipboardPage.Validate(pageRequest) != nil {
			return fail(errors.New("invalid clipboard page reply"))
		}
		return emit(reply.ClipboardPage, reply.ClipboardPage.Result == clipboard.OK)
	}
	native := clipboard.Request{Ref: clipboard.Ref{Epoch: opt.epoch.value, Item: opt.item.value, Generation: opt.generation.value}}
	switch opt.command {
	case "pin":
		native.Verb, native.Value = clipboard.Pin, 1
	case "unpin":
		native.Verb = clipboard.Pin
	case "remove":
		native.Verb = clipboard.Remove
	case "clear":
		native.Verb = clipboard.Clear
	}
	reply, err := call(sessionservice.RequestMessage{Verb: "clipboard", Clipboard: &native})
	if err != nil {
		return fail(err)
	}
	if reply.Clipboard == nil || reply.Clipboard.ValidateFor(native) != nil {
		return fail(errors.New("invalid clipboard action reply"))
	}
	return emit(reply.Clipboard, reply.Clipboard.Result == clipboard.OK)
}

func main() { os.Exit(run(os.Args[1:], os.Stdout, os.Stderr, sessionservice.Request)) }

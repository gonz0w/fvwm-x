// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"bytes"
	"context"
	"github.com/fvwmorg/FvwmPrompt/internal/clipboard"
	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
	"io"
	"strings"
	"testing"
)

func TestClipboardAdmitsMetadataWithoutBusAndRejectsReplay(t *testing.T) {
	cfg := defaults(Config{Environment: serviceEnvironment(t, sessionservice.Defaults())})
	state, err := newServices(&cfg, &reporter{output: io.Discard})
	if err != nil {
		t.Fatal(err)
	}
	message := sessionservice.RequestMessage{Version: sessionservice.Version, Generation: state.snapshot.Generation, Operation: state.snapshot.Operation, Verb: "clipboard", Clipboard: &clipboard.Request{Verb: clipboard.Status, Ref: clipboard.Ref{Epoch: 19}}}
	request := controlRequest{ctx: context.Background(), message: message, reply: make(chan sessionservice.Reply, 1)}
	start, _ := state.admit(request, cfg, false)
	if !start {
		t.Fatal("metadata request depended on optional bus")
	}
	job := runClipboardJob(request, nil)
	reply := state.complete(job)
	if reply.Outcome != "unavailable" || reply.Clipboard != nil {
		t.Fatalf("disabled owner falsely ready %+v", reply)
	}
	if start, reply := state.admit(request, cfg, false); start || reply.Outcome != "stale" {
		t.Fatal("replayed clipboard control admitted")
	}
}

func TestClipboardCleanupSurvivesDiagnosticLimit(t *testing.T) {
	var output bytes.Buffer
	report := reporter{output: &output, clipboardTerminalReserved: true, busTerminalReserved: true}
	for i := 0; i < maxDiagnostics*2; i++ {
		report.write("ordinary observation")
	}
	report.clipboardTerminal("clipboard owner cleanup: started=true reaped=true clean=false wait-confirmed=true")
	report.clipboardTerminal("duplicate")
	report.busTerminal("owned bus terminal")
	report.terminal("WM terminal")
	lines := strings.Split(strings.TrimSpace(output.String()), "\n")
	if len(lines) != maxDiagnostics || strings.Count(output.String(), "clipboard owner cleanup:") != 1 || strings.Contains(output.String(), "duplicate") {
		t.Fatalf("lost/unbounded terminal report: %q", output.String())
	}
}

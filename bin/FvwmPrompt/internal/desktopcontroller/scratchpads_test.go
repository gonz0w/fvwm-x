// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
package desktopcontroller

import (
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"reflect"
	"strings"
	"testing"
	"time"
)

func TestScratchpadDefinitionLiteralFingerprintAndBounds(t *testing.T) {
	d, err := parseScratchpad(`term "Term Class" - terminal --title 'a b;$HOME' ""`)
	if err != nil || d.Name != "term" || d.Class != "Term Class" || d.Instance != "" || !reflect.DeepEqual(d.Argv, []string{"terminal", "--title", "a b;$HOME", ""}) {
		t.Fatal(d, err)
	}
	again, _ := parseScratchpad(`term 'Term Class' - terminal --title "a b;$HOME" ''`)
	if d.Fingerprint != again.Fingerprint || len(d.Fingerprint) != 64 {
		t.Fatal("equivalent literal words changed identity")
	}
	changed, _ := parseScratchpad(`term 'Term Class' instance terminal --title "a b;$HOME" ''`)
	if d.Fingerprint == changed.Fingerprint {
		t.Fatal("instance did not change canonical fingerprint")
	}
	for _, bad := range []string{"", "term Class -", "bad/name Class - app", "term '' - app", "term Class - ''", "term Class - 'unfinished", "term Class - app\x00arg", "term Class - app\xff", strings.Repeat("n", 33) + " Class - app", "term '" + strings.Repeat("界", 43) + "' - app"} {
		if _, err := parseScratchpad(bad); err == nil {
			t.Fatal("invalid definition admitted", bad)
		}
	}
}

func scratchpadCoreReply(t *testing.T, d *Controller, s *scratchpad, state, action string, reasons ...string) {
	t.Helper()
	q := s.pending
	if q == nil {
		t.Fatal("no pending scratchpad request")
	}
	var lease any
	if state == "pending" || action == "launch" {
		lease = strings.Repeat("b", 48)
	}
	reason := state
	if len(reasons) > 0 {
		reason = reasons[0]
	}
	data, _ := json.Marshal(map[string]any{"result": map[string]any{
		"version": 1, "name": s.definition.Name, "state": state,
		"desired_visible": state != "unmatched", "action": action, "lease": lease,
		"target": nil, "reason": reason}})
	parts := 0
	for offset := 0; offset < len(data); offset += 256 {
		message := fmt.Sprintf("MFL_CORE_1 %s %016x part %d %s", d.scratchpads.cookie, q.serial, parts, hex.EncodeToString(data[offset:min(offset+256, len(data))]))
		if !d.scratchpadReply(message) {
			t.Fatal("scratchpad part ignored")
		}
		parts++
	}
	d.scratchpadReply(fmt.Sprintf("MFL_CORE_1 %s %016x end %d", d.scratchpads.cookie, q.serial, parts))
}

func TestScratchpadLeaseLaunchCoalescingAndReplay(t *testing.T) {
	d, commands := fixture(t)
	if err := d.configureScratchpad(`term Term - terminal 'literal;$HOME'`); err != nil {
		t.Fatal(err)
	}
	starts := 0
	d.scratchpads.launch = func(argv []string) error {
		starts++
		if !reflect.DeepEqual(argv, []string{"terminal", "literal;$HOME"}) {
			t.Fatal("argv was interpreted", argv)
		}
		return nil
	}
	if err := d.scratchpadAction("term", "toggle"); err != nil {
		t.Fatal(err)
	}
	s := d.scratchpads.definitions[0]
	first := s.pending.serial
	for i := 0; i < 3; i++ {
		_ = d.scratchpadAction("term", "toggle")
	}
	if s.pending.serial != first || !s.queuedToggle {
		t.Fatal("rapid toggles replaced first launch permission")
	}
	scratchpadCoreReply(t, d, s, "pending", "launch")
	if starts != 1 || s.pending == nil || s.pending.verb != "launched" {
		t.Fatal("launch was not acknowledged once", starts, s.pending)
	}
	if !d.scratchpadReply(fmt.Sprintf("MFL_CORE_1 %s %016x end 1", d.scratchpads.cookie, first)) || starts != 1 {
		t.Fatal("replayed launch permission was executed")
	}
	scratchpadCoreReply(t, d, s, "pending", "none")
	if s.pending == nil || s.pending.verb != "toggle" || starts != 1 {
		t.Fatal("latest visibility intent was not coalesced", s.pending)
	}
	text := strings.Join(*commands, "\n")
	if !strings.Contains(text, "Send_Scratchpad v1 ") || strings.Contains(text, "literal;$HOME") {
		t.Fatal("argv entered native command wire", text)
	}
}

func TestScratchpadDefiniteFailureRestartAndEventMaskUnion(t *testing.T) {
	d, commands := fixture(t)
	_ = d.configureScratchpad(`term Term - missing-app`)
	starts := 0
	d.scratchpads.launch = func([]string) error {
		starts++
		return errors.New("executable unavailable")
	}
	_ = d.scratchpadAction("term", "toggle")
	s := d.scratchpads.definitions[0]
	scratchpadCoreReply(t, d, s, "pending", "launch")
	if s.pending == nil || s.pending.verb != "launch-failed" || s.queuedToggle {
		t.Fatal("definite failure retained launch permission", s.pending)
	}
	before := len(*commands)
	scratchpadCoreReply(t, d, s, "unmatched", "none", "launch-failed")
	if s.pending != nil || s.state != "unmatched" || s.reason != "launch-failed" || s.desired || s.queuedToggle {
		t.Fatal("definite failure reply became uncertain", s)
	}
	if starts != 1 || len(*commands) != before {
		t.Fatal("definite failure implicitly retried the executable", starts)
	}
	_ = d.startPalette()
	d.closePalette()
	if got := (*commands)[len(*commands)-1]; got != fmt.Sprintf("SET_MASK %d", regularDesktopMask|paletteEventMask) {
		t.Fatal("closing view disabled configured scratchpads", got)
	}
	// A fresh helper asks the core for observation; it never infers permission
	// to relaunch from missing local state or from a status reply.
	fresh, _ := fixture(t)
	_ = fresh.configureScratchpad(`term Term - missing-app`)
	_ = fresh.scratchpadAction("term", "status")
	newState := fresh.scratchpads.definitions[0]
	scratchpadCoreReply(t, fresh, newState, "uncertain", "none")
	if newState.state != "uncertain" || newState.pending != nil {
		t.Fatal("helper restart lost core uncertainty", newState)
	}
}

func TestScratchpadConfigurationCapacityAndPendingTimeout(t *testing.T) {
	d, _ := fixture(t)
	for i := 0; i < 16; i++ {
		if err := d.configureScratchpad(fmt.Sprintf("s%d Class - app", i)); err != nil {
			t.Fatal(err)
		}
	}
	if err := d.configureScratchpad("overflow Class - app"); err == nil || len(d.scratchpads.definitions) != 16 {
		t.Fatal("definition cap lost")
	}
	if err := d.configureScratchpad("s0 Class - changed"); err == nil {
		t.Fatal("duplicate name replaced active definition")
	}
	starts := 0
	d.scratchpads.launch = func([]string) error { starts++; return nil }
	_ = d.scratchpadAction("s0", "status")
	s := d.scratchpads.definitions[0]
	scratchpadCoreReply(t, d, s, "pending", "launch")
	if starts != 0 || s.state != "uncertain" {
		t.Fatal("status granted a launch")
	}
	_ = d.scratchpadAction("s1", "toggle")
	s = d.scratchpads.definitions[1]
	s.pending.deadline = time.Now().Add(-time.Second)
	d.scratchpads.dirty = false
	d.scratchpadTick()
	if starts != 0 || s.pending != nil || s.state != "uncertain" {
		t.Fatal("reply timeout retried a launch")
	}
}

func TestScratchpadManagerCloseRetiresOnlyAnOwnedView(t *testing.T) {
	d, commands := fixture(t)
	before := len(*commands)
	d.closeScratchpads()
	if len(*commands) != before {
		t.Fatal("absent scratchpad manager emitted teardown")
	}
	if err := d.startScratchpads(); err != nil {
		t.Fatal(err)
	}
	before = len(*commands)
	d.Close()
	if len(*commands) != before+1 || (*commands)[before] != "KillModule FvwmForm SlateScratchpadsForm" || d.scratchpads.view != "" || d.scratchpads.frame != "" {
		t.Fatal("owned scratchpad manager was not retired exactly once")
	}
	d.closeScratchpads()
	d.Close()
	if len(*commands) != before+1 {
		t.Fatal("closed manager/controller repeated teardown")
	}
}

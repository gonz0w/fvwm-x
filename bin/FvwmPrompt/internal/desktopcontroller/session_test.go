// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestSessionPowerHasIndependentBoundedAdmission(t *testing.T) {
	d, commands := fixture(t)
	d.jobs["volume"] = &job{key: "volume"}
	if err := d.handle("session", "poweroff"); err != nil {
		t.Fatal(err)
	}
	if d.jobs["power"] == nil || len(d.queue) != 1 || d.queue[0].key != "power" {
		t.Fatal("power request lost behind audio job")
	}
	before := len(*commands)
	if err := d.handle("session", "reboot"); err != nil {
		t.Fatal(err)
	}
	if len(d.queue) != 1 || !strings.Contains(literalCommands(t, (*commands)[before:]), "already pending") {
		t.Fatal("duplicate power request not explicitly refused")
	}
	d.jobs["other"] = &job{}
	d.jobs["last"] = &job{}
	delete(d.jobs, "power")
	d.jobs["full"] = &job{}
	before = len(*commands)
	_ = d.handle("session", "reboot")
	if !strings.Contains(literalCommands(t, (*commands)[before:]), "busy") {
		t.Fatal("full admission silently dropped power action")
	}
	before = len(d.queue)
	_ = d.handle("session", "reboot; bogus")
	if len(d.queue) != before {
		t.Fatal("invalid action submitted")
	}
}

func TestLockerExitIsOwnedByEventLoopAndAllowsRetry(t *testing.T) {
	d, commands := fixture(t)
	bin := t.TempDir()
	marker := filepath.Join(bin, "started")
	gate := filepath.Join(bin, "finish")
	t.Setenv("FVWM_TEST_LOCK_MARKER", marker)
	t.Setenv("FVWM_TEST_LOCK_GATE", gate)
	locker := filepath.Join(bin, "xlock")
	if err := os.WriteFile(locker, []byte("#!/bin/sh\necho x >> \"$FVWM_TEST_LOCK_MARKER\"\nwhile test ! -f \"$FVWM_TEST_LOCK_GATE\"; do /bin/sleep .01; done\nexit 7\n"), 0700); err != nil {
		t.Fatal(err)
	}
	t.Setenv("PATH", bin)
	t.Cleanup(func() { _ = os.WriteFile(gate, nil, 0600) })
	_ = d.handle("lock", "")
	if !d.lockerPending {
		t.Fatalf("locker lifetime not retained: %s", literalCommands(t, *commands))
	}
	before := len(*commands)
	_ = d.handle("lock", "")
	if !strings.Contains(literalCommands(t, (*commands)[before:]), "already pending") {
		t.Fatal("duplicate locker not refused")
	}
	if err := os.WriteFile(gate, nil, 0600); err != nil {
		t.Fatal(err)
	}
	deadline := time.Now().Add(2 * time.Second)
	for d.lockerPending && time.Now().Before(deadline) {
		d.pollLocker()
		time.Sleep(time.Millisecond)
	}
	if d.lockerPending || !strings.Contains(literalCommands(t, *commands), "protection is unconfirmed") {
		t.Fatal("locker failure lost")
	}
	data, _ := os.ReadFile(marker)
	if string(data) != "x\n" || len(d.spawnSlots) != 0 {
		t.Fatal("duplicate launch or retained application slot")
	}
	if err := os.Remove(locker); err != nil {
		t.Fatal(err)
	}
	_ = d.handle("lock", "")
	if !strings.Contains(literalCommands(t, *commands), "NOT been locked") {
		t.Fatal("missing locker not explicit")
	}
	if err := os.WriteFile(locker, []byte("#!/no-such-interpreter\n"), 0700); err != nil {
		t.Fatal(err)
	}
	_ = d.handle("lock", "")
	if d.lockerPending || len(d.spawnSlots) != 0 || !strings.Contains(literalCommands(t, *commands), "Could not start locker") {
		t.Fatal("construction refusal leaked ownership")
	}
}

func TestPowerPublicationNeverUsesAudioFallback(t *testing.T) {
	d, commands := fixture(t)
	if err := d.publishJob(job{key: "power", result: ""}); err == nil {
		t.Fatal("empty power response accepted")
	}
	if err := d.publishJob(job{key: "power", result: "Power request denied by the system."}); err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(literalCommands(t, *commands), "Power request denied") {
		t.Fatal("power response was not published")
	}
}

func TestDisabledCaptureRefusesBeforeJobAdmission(t *testing.T) {
	d, commands := fixture(t)
	d.png = false
	d.screenshot()
	if len(d.jobs) != 0 || len(d.queue) != 0 {
		t.Fatal("disabled capture started a helper job")
	}
	if !strings.Contains(literalCommands(t, *commands), "PNG support is disabled") {
		t.Fatal("disabled capture did not explain unavailability")
	}
}

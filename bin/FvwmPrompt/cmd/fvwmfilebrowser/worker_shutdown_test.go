// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"os"
	"path/filepath"
	"sync"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

func TestWorkerShutdownCannotReturnBeforeOwnerCompletion(t *testing.T) {
	process := &fileWorkerProcess{done: make(chan struct{}), stop: make(chan struct{})}
	cancelled, returned := make(chan struct{}), make(chan struct{})
	var complete sync.Once
	finish := func() { complete.Do(func() { close(process.done) }) }
	t.Cleanup(finish)
	b := browser{worker: process, active: true, cancel: func() { close(cancelled) }, pending: &pendingWork{serial: 2}}
	go func() { b.shutdownWorker(); close(returned) }()
	<-cancelled
	select {
	case <-returned:
		t.Fatal("shutdown abandoned the outstanding process owner")
	default:
	}
	finish()
	<-returned
	if b.active || b.worker != nil || b.cancel != nil || b.pending != nil {
		t.Fatal("completed shutdown retained ordinary worker ownership")
	}
	b.shutdownWorker()
}

func TestWorkerShutdownJoinsHeldClassifierWithBlockedResultDelivery(t *testing.T) {
	for _, capacity := range []int{0, 1} {
		t.Run(string(rune('0'+capacity)), func(t *testing.T) {
			executable := classifierFixture(t, "hang", "")
			root := t.TempDir()
			if err := os.WriteFile(filepath.Join(root, "selected"), []byte("text"), 0600); err != nil {
				t.Fatal(err)
			}
			snapshot, err := filemodel.Scan(root, false)
			if err != nil {
				t.Fatal(err)
			}
			heartbeat := filepath.Join(t.TempDir(), "held-classifier")
			t.Setenv("FVWM_TEST_ROLE_HEARTBEAT", heartbeat)
			b := browser{executable: executable, snapshot: snapshot, results: make(chan completedWork, capacity)}
			if capacity != 0 {
				b.results <- completedWork{}
			}
			// Every failure after handoff still joins the process owner; no test
			// timeout or unconsumed result may leave a separate worker PGID alive.
			t.Cleanup(b.shutdownWorker)
			if err := b.queue(workRequest{Operation: "file-roles", Snapshot: snapshot, ID: snapshot.Items[0].ID}, "file-roles"); err != nil {
				t.Fatal(err)
			}
			process := b.worker
			until := time.Now().Add(time.Second)
			for time.Now().Before(until) {
				if _, err := os.Stat(heartbeat); err == nil {
					break
				}
				time.Sleep(time.Millisecond)
			}
			if _, err := os.Stat(heartbeat); err != nil {
				t.Fatal("classifier descendant was not running before shutdown", err)
			}
			b.shutdownWorker()
			select {
			case <-process.done:
			default:
				t.Fatal("shutdown returned before startWorker's actual Wait owner")
			}
			if b.active || b.worker != nil || b.cancel != nil || b.pending != nil {
				t.Fatal("worker slot survived joined shutdown")
			}
			heartbeatStopped(t, heartbeat)
		})
	}
}

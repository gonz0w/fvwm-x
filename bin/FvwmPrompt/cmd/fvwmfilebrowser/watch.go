// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"context"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

// Only the browser loop mutates this state. The observer owns blocking work;
// its one bounded channel carries invalidations, never authoritative entries.
type directoryWatch struct {
	enabled, active, ready, failed, dirty bool
	serial                                uint64
	current, desired                      *watchRequest
	cancel                                context.CancelFunc
	events                                chan watchEvent
	text                                  string
	armDeadline, nextRefresh, reconcile   time.Time
}

func sameWatch(a, b *watchRequest) bool {
	return a != nil && b != nil && bytes.Equal(a.Path, b.Path) &&
		a.Root.Device == b.Root.Device && a.Root.Inode == b.Root.Inode
}

func (w *directoryWatch) fail(text string) {
	w.failed, w.ready, w.dirty = true, false, false
	w.text = text + "; manual F5 or toggle F8 to retry."
	if w.cancel != nil {
		w.cancel()
	}
}

func (w *directoryWatch) desire(snapshot *filemodel.Snapshot, retry bool) {
	var request *watchRequest
	if w.enabled && snapshot != nil {
		request = &watchRequest{append([]byte(nil), snapshot.Path...), snapshot.Root}
	}
	changed := !sameWatch(w.desired, request)
	if changed || retry {
		w.failed = false
	}
	if changed {
		w.ready, w.dirty = false, false
		w.nextRefresh = time.Time{}
	}
	w.desired = request
	if request == nil {
		w.text = "Watching: off (F8 enables). Refresh: F5."
	} else if !w.failed && !w.ready {
		w.text = "Watching: starting; manual F5 remains available."
	}
	if w.active && (!sameWatch(w.current, request) || w.failed || retry && !w.ready) {
		w.cancel()
	}
}

func (w *directoryWatch) tick(now time.Time, executable string) bool {
	if !w.active && !w.failed && w.desired != nil {
		if w.serial == ^uint64(0) {
			w.fail("Watch generation exhausted")
			return true
		}
		w.serial++
		request := *w.desired
		request.Path = append([]byte(nil), request.Path...)
		w.current, w.active = &request, true
		w.ready = false
		w.armDeadline = now.Add(workerLimit)
		ctx, cancel := context.WithCancel(context.Background())
		w.cancel = cancel
		go watchProcess(ctx, executable, request, w.serial, w.events)
		return true
	}
	if w.active && !w.ready && !w.failed && !w.armDeadline.IsZero() &&
		!now.Before(w.armDeadline) && sameWatch(w.current, w.desired) {
		w.fail("Watch startup timed out")
		return true
	}
	if w.ready && !now.Before(w.reconcile) {
		w.dirty = true
		w.reconcile = now.Add(30 * time.Second)
	}
	return false
}

func (w *directoryWatch) event(event watchEvent, now time.Time) bool {
	if event.serial != w.serial || !w.active {
		return false
	}
	if event.state == "done" {
		w.cancel()
		w.cancel, w.current, w.active, w.ready = nil, nil, false, false
		return true
	}
	// A cancelled observer can still have one buffered record. It cannot
	// invalidate the replacement directory or undo a manual fallback.
	if w.failed || !sameWatch(w.current, w.desired) {
		return false
	}
	switch event.state {
	case "ready":
		w.ready, w.dirty = true, true
		w.reconcile = now.Add(30 * time.Second)
		w.text = "Watching: native + 30s reconciliation (F8 disables)."
		return true
	case "changed":
		if w.ready && !w.dirty {
			w.dirty = true
			if due := now.Add(250 * time.Millisecond); w.nextRefresh.Before(due) {
				w.nextRefresh = due
			}
		}
	case "expired":
		w.ready, w.dirty = false, false
		w.armDeadline = time.Time{}
		w.text = "Watching: renewing; manual F5 remains available."
		return true
	case "lost":
		w.fail("Folder watch lost")
		return true
	case "unavailable":
		w.fail("Folder watch unavailable")
		return true
	}
	return false
}

func (w *directoryWatch) takeRefresh(now time.Time, busy bool) *watchRequest {
	if busy || !w.ready || !w.dirty || w.failed ||
		!sameWatch(w.current, w.desired) || now.Before(w.nextRefresh) {
		return nil
	}
	w.dirty = false
	w.nextRefresh = now.Add(time.Second)
	return w.desired
}

func (b *browser) watchText() string {
	if b.watch == nil || b.watch.text == "" {
		return "Watching: off (F8 enables). Refresh: F5."
	}
	return b.watch.text
}

func (w *directoryWatch) state() string {
	if w == nil || !w.enabled {
		return "off"
	}
	if w.failed {
		return "manual"
	}
	if w.ready {
		return "native"
	}
	return "starting"
}

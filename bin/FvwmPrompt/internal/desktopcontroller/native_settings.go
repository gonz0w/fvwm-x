// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"errors"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

type nativeControl struct {
	helper                                             string
	slot                                               *nativeSupervisor
	observation, published                             desktopsettings.NativeObservation
	desired, sent                                      nativeDesired
	initialized, enabled, retry, stopping, shutdownAck bool
	observationKnown                                   bool
	seq, inflight                                      uint64
	deadline, stopDeadline                             time.Time
}

func (d *Controller) nativeObservation() desktopsettings.NativeObservation {
	observation := d.native.observation
	if observation.Readiness == "" {
		observation.Readiness = "off"
	}
	if observation.ResourceState == "" {
		observation.ResourceState = "unavailable"
	}
	if observation.Reason == "" {
		observation.Reason = "none"
	}
	return observation
}

func (d *Controller) nativePublishObservation() {
	if d.closed {
		return
	}
	n := &d.native
	observation := d.nativeObservation()
	if n.observationKnown && n.published == observation {
		return
	}
	n.published, n.observationKnown = observation, true
	if d.ready {
		previous := d.generation
		d.generation = newID(16)
		// Native-only observations change the public snapshot, not the saved
		// preference basis. Preserve only already-valid drafts/preview snapshots;
		// a stale generation or revision must never become usable again.
		if e := d.editor; e != nil && e.generation == previous && time.Now().Before(e.deadline) {
			e.generation = d.generation
		}
		if c := d.preview.control; c != nil && c.saved != nil &&
			c.saved.generation == previous && sameRevision(c.saved.revision, d.revision) {
			c.saved.generation = d.generation
			if c.editor != nil && c.editor.generation == previous {
				c.editor.generation = d.generation
			}
		}
		d.staleInspection("Native application preferences changed; refresh for current observations.")
		_ = d.paletteProviders()
	}
	if d.editor != nil && len(d.editor.keys) > 0 && desktopsettings.ApplicationKey(d.editor.keys[0]) {
		for _, label := range [][2]string{{"native_state", "Native: " + observation.Readiness},
			{"native_resource", "Cursor resources: " + observation.ResourceState}, {"native_reason", "Reason: " + observation.Reason}} {
			d.emit("SendToModule SlateSettingsForm SetLabel " + label[0] + " " + quote(label[1]))
		}
	}
	if d.ready {
		_ = d.boundSettingsMenus("all")
		d.announceSettings()
	}
}

func (d *Controller) nativeStop(reason string) {
	n := &d.native
	if n.slot == nil {
		return
	}
	if !n.stopping {
		n.stopping = true
		n.stopDeadline = time.Now().Add(1500 * time.Millisecond)
		n.slot.close() // EOF is the canonical cleanup request.
	}
	n.inflight = 0
	if reason != "" {
		n.observation.Reason = reason
	}
	if n.observation.Readiness == "ready" || n.observation.Readiness == "starting" || !n.enabled {
		n.observation.Readiness = "stopping"
	}
}

// explicit permits one attempt after an actual settings change/reload or retry.
// A failed run never creates a restart loop; only its latest desired tuple is
// retained while stopping, and a replacement waits for confirmed actual reap.
func (d *Controller) applyNativeSettings(explicit bool) {
	n := &d.native
	desired := nativeTuple(d.effective)
	enabled := d.effective["native_settings_enabled"] == true
	initial := !n.initialized
	changed := n.desired != desired || n.enabled != enabled
	n.initialized, n.enabled, n.desired = true, enabled, desired
	if !enabled {
		n.retry = false
		if n.slot != nil {
			d.nativeStop("")
		} else if n.observation.Run == "" {
			n.observation = desktopsettings.NativeObservation{Readiness: "off", ResourceState: "inherited", Reason: "none"}
		} else {
			n.observation.Readiness = "off"
			if !n.shutdownAck {
				n.observation.ResourceState, n.observation.Reason = "unavailable", "cleanup-unconfirmed"
			}
		}
		d.nativePublishObservation()
		return
	}
	if n.slot != nil && !n.stopping && (explicit || changed && n.observation.ResourceState == "conflict") {
		n.retry = true
		d.nativeStop("")
	}
	if initial || changed || explicit {
		if n.slot == nil {
			n.retry = true
		} else if n.stopping {
			n.retry = true
		}
	}
	if n.slot == nil && n.retry && !d.closed {
		n.retry = false
		run := newID(16)
		n.slot = startNativeSupervisor(n.helper, run)
		n.seq, n.inflight, n.stopping, n.shutdownAck = 0, 0, false, false
		n.observation = desktopsettings.NativeObservation{Readiness: "starting", ResourceState: "unavailable", Run: run, Reason: "none"}
		d.nativeSend(time.Now())
	} else if !n.stopping && n.slot != nil && n.inflight == 0 && n.sent != desired {
		d.nativeSend(time.Now())
	}
	d.nativePublishObservation()
}

func (d *Controller) retryNativeSettings() error {
	if d.closed || d.effective["native_settings_enabled"] != true {
		return errors.New("Enable native application preferences before retrying")
	}
	if d.native.slot != nil {
		d.native.retry = true
		d.nativeStop("")
	}
	d.applyNativeSettings(true)
	return nil
}

func (d *Controller) nativeSend(now time.Time) {
	n := &d.native
	if n.slot == nil || n.stopping || n.inflight != 0 {
		return
	}
	if n.seq == ^uint64(0) {
		d.nativeStop("sequence-exhausted")
		return
	}
	seq := n.seq + 1
	line, err := nativeApply(d.instance, n.slot.run, seq, n.desired)
	if err != nil {
		d.nativeStop("invalid-input")
		return
	}
	select {
	case n.slot.input <- line:
		n.seq, n.inflight, n.sent = seq, seq, n.desired
		n.deadline = now.Add(2 * time.Second)
		if seq == 1 {
			n.deadline = now.Add(3 * time.Second)
		}
	default:
		d.nativeStop("helper-io")
	}
}

func (d *Controller) nativeEvent(event nativeEvent) {
	n := &d.native
	if n.slot == nil || event.run != n.slot.run || event.run != n.observation.Run {
		return
	}
	if event.failure != "" {
		if n.stopping && event.failure != "stop-unknown" {
			return
		}
		n.observation.Readiness = "failed"
		if event.failure == "stop-unknown" {
			n.observation.Readiness = "stop-unknown"
		}
		d.nativeStop(event.failure)
		return
	}
	r := event.result
	if r == nil || r.desktop != d.instance || r.run != n.slot.run || r.seq != n.seq {
		return
	}
	if n.stopping {
		if r.xstate == "off" {
			n.shutdownAck = true
			n.observation.ResourceState = r.resource
			if n.observation.Readiness == "stopping" || !n.enabled {
				n.observation.Reason = r.reason
			}
		}
		return
	}
	// Terminal ownership loss may arrive after the matching APPLY acknowledgment.
	// A ready response without in-flight work is only a harmless duplicate.
	if n.inflight == 0 && r.xstate == "ready" {
		return
	}
	n.observation = desktopsettings.NativeObservation{Readiness: r.xstate,
		ResourceState: r.resource, Run: r.run, Reason: r.reason, Sequence: r.seq, XSerial: r.xserial}
	n.inflight = 0
	if r.xstate != "ready" {
		d.nativeStop(r.reason)
	}
}

func (d *Controller) nativeTick(now time.Time) {
	n := &d.native
	if n.slot == nil {
		return
	}
	before := n.observation
	for i := 0; i < 16; i++ {
		select {
		case event := <-n.slot.events:
			d.nativeEvent(event)
		default:
			select {
			case event := <-n.slot.fault:
				d.nativeEvent(event)
			default:
				i = 16
			}
		}
	}
	// Completion has its own retained cell. Drain preceding bounded observations
	// before releasing the slot; no terminal event can be lost behind a full queue.
	if len(n.slot.events) == 0 && len(n.slot.fault) == 0 {
		select {
		case terminal := <-n.slot.terminal:
			if terminal.run == n.slot.run {
				if terminal.reaped {
					n.slot = nil
					n.inflight, n.stopping = 0, false
					if !n.enabled {
						n.observation.Readiness = "off"
						if !n.shutdownAck {
							n.observation.ResourceState, n.observation.Reason = "unavailable", "cleanup-unconfirmed"
						}
					} else if n.observation.Readiness == "ready" || n.observation.Readiness == "starting" || n.observation.Readiness == "stopping" {
						n.observation.Readiness, n.observation.Reason = "failed", "helper-exited"
					}
					if n.retry && n.enabled {
						d.applyNativeSettings(false)
					}
				} else {
					n.observation.Readiness, n.observation.Reason = "stop-unknown", "stop-unknown"
					n.stopping = true
				}
			}
		default:
		}
	}
	if n.slot != nil {
		if n.stopping && !now.Before(n.stopDeadline) {
			n.observation.Readiness, n.observation.Reason = "stop-unknown", "stop-unknown"
		} else if !n.stopping && n.inflight != 0 && !now.Before(n.deadline) {
			n.observation.Readiness = "failed"
			d.nativeStop("timeout")
		} else if !n.stopping && n.inflight == 0 && n.sent != n.desired {
			d.nativeSend(now)
		}
	}
	if before != n.observation {
		d.nativePublishObservation()
	}
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import "github.com/fvwmorg/FvwmPrompt/internal/sessionservice"

func (state *serviceState) reply(outcome, message string, id uint32) sessionservice.Reply {
	snapshot := state.snapshot
	snapshot.Items = append([]sessionservice.Item(nil), snapshot.Items...)
	return sessionservice.Reply{Version: 1, Generation: snapshot.Generation, Operation: snapshot.Operation, Outcome: outcome, Error: detail(message), NotificationID: id, Status: snapshot}
}

// admit runs only in the parent's event loop. Exactly one service mutation can
// remain active; each admitted mutation consumes the current opaque CAS token.
func (state *serviceState) admit(request controlRequest, cfg Config, busy bool) (bool, sessionservice.Reply) {
	message := request.message
	if err := message.Validate(); err != nil {
		return false, state.reply("rejected", err.Error(), 0)
	}
	if message.Verb == "status" {
		return false, state.reply("ready", "", 0)
	}
	if message.Generation != state.snapshot.Generation || message.Operation != state.snapshot.Operation {
		return false, state.reply("stale", "session control identity is stale; refresh status", 0)
	}
	if request.ctx.Err() != nil {
		return false, state.reply("cancelled", "session request was cancelled", 0)
	}
	if busy {
		return false, state.reply("busy", "a session service operation is already active", 0)
	}
	// A known owned-daemon exit is terminal for this login. Reject before
	// consuming a control token or replacing failed/unavailable with starting.
	// Configuration and quiet DND suppression do not invoke an adapter.
	if state.bus != nil && state.bus.reaped && message.Verb != "configure" && !(message.Verb == "notify" && state.snapshot.Config.DND) {
		return false, state.reply("unavailable", "owned session bus exited; adapter work requires a new login", 0)
	}
	if message.Verb == "close" && !state.ids[message.NotificationID] {
		return false, state.reply("rejected", "notification ID is not owned by this session/server", 0)
	}
	operation, err := randomIdentity(16)
	if err != nil {
		return false, state.reply("failed", err.Error(), 0)
	}
	state.snapshot.Operation = operation
	if message.Verb == "configure" {
		state.snapshot.Config = *message.Config
		state.refreshNotifications()
		return false, state.reply("applied", "", 0)
	}
	if message.Verb == "notify" && state.snapshot.Config.DND {
		return false, state.reply("suppressed", "", 0)
	}
	if (message.Verb == "notify" || message.Verb == "close") && state.snapshot.Config.Notifications != "external" {
		return false, state.reply("unavailable", "external notifications are disabled", 0)
	}
	index := 2
	if message.Verb != "reprobe" {
		index = 3
	}
	state.snapshot.Items[index].State = "starting"
	if index == 2 {
		state.snapshot.Items[index].Operation = operation
	}
	state.snapshot.Items[index].Attempts++
	return true, sessionservice.Reply{}
}

func (state *serviceState) complete(job serviceJob) sessionservice.Reply {
	// An owned daemon is never replaced in this parent lifetime. A probe that
	// completed before its Wait result cannot revive that terminal lifecycle.
	if state.bus != nil && state.bus.reaped {
		return state.reply("failed", "owned session bus exited; late adapter result discarded", 0)
	}
	index := 2
	if job.request.message.Verb != "reprobe" {
		index = 3
	}
	if job.probe.BusID != "" && (state.identity.BusID == "" || state.identity.BusID == job.probe.BusID) {
		ownership := state.snapshot.Items[2].Ownership
		if ownership == "none" {
			ownership = "borrowed"
		}
		state.adoptProbe(job.probe, ownership)
	}
	if job.err != nil {
		state.snapshot.Items[index].State = "failed"
		state.snapshot.Items[index].Detail = detail(job.err.Error())
		if job.probe.BusID == "" || state.identity.BusID != "" && state.identity.BusID != job.probe.BusID {
			state.snapshot.Items[2].State = "failed"
			state.snapshot.Items[2].Detail = detail(job.err.Error())
			state.snapshot.Items[3].State = "unavailable"
			state.ids = make(map[uint32]bool)
		}
		return state.reply(job.outcome, job.err.Error(), 0)
	}
	if job.request.message.Verb == "notify" {
		state.ids[job.result.NotificationID] = true
	}
	if job.request.message.Verb == "close" {
		delete(state.ids, job.request.message.NotificationID)
	}
	return state.reply(job.outcome, "", job.result.NotificationID)
}

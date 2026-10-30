// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import "github.com/fvwmorg/FvwmPrompt/internal/sessionservice"

func (state *serviceState) reply(outcome, message string, id uint32) sessionservice.Reply {
	state.fenceBusLoss()
	snapshot := state.snapshot
	snapshot.Power.Capabilities = append([]sessionservice.PowerCapability(nil), snapshot.Power.Capabilities...)
	snapshot.Items = append([]sessionservice.Item(nil), snapshot.Items...)
	return sessionservice.Reply{Version: sessionservice.Version, Generation: snapshot.Generation, Operation: snapshot.Operation, Outcome: outcome, Error: detail(message), NotificationID: id, Status: snapshot}
}

// admit runs only in the parent's event loop. Exactly one service mutation can
// remain active; each admitted mutation consumes the current opaque CAS token.
func (state *serviceState) admit(request controlRequest, cfg Config, busy bool) (bool, sessionservice.Reply) {
	state.fenceBusLoss()
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
	if message.Verb == "lock" && (state.snapshot.Locker.State == "starting" || state.locker != nil) {
		return false, state.reply("busy", "an observed locker is pending or running; secure readiness remains unconfirmed", 0)
	}
	if sessionservice.IsPowerVerb(message.Verb) && message.ProviderGeneration != state.snapshot.Power.Generation {
		return false, state.reply("stale", "power provider observation is stale; refresh before confirming", 0)
	}
	if message.Verb == "suspend" {
		return false, state.reply("unavailable", "coordinated suspend refused: secure locker readiness is unconfirmed", 0)
	}
	if message.Verb == "poweroff" || message.Verb == "reboot" {
		i := 0
		if message.Verb == "reboot" {
			i = 1
		}
		power := state.snapshot.Power
		if power.State != "ready" || len(power.Capabilities) != 3 {
			return false, state.reply("unavailable", "refresh power provider before requesting an action", 0)
		}
		permission := power.Capabilities[i].Permission
		if power.Provider == "login1" && permission != "yes" || power.Provider == "portable" && permission != "unknown" {
			outcome := "unavailable"
			switch permission {
			case "no":
				outcome = "denied"
			case "challenge":
				outcome = "authentication-required"
			case "inhibited", "inhibitor-blocked", "challenge-inhibitor-blocked":
				outcome = "inhibited"
			}
			return false, state.reply(outcome, "power capability does not authorize this request", 0)
		}
	}
	if busy {
		return false, state.reply("busy", "a session service operation is already active", 0)
	}
	// Owned-bus termination or unknown guardian loss is terminal for this login. Reject before
	// consuming a control token or replacing failed/unavailable with starting.
	// Configuration and quiet DND suppression do not invoke an adapter.
	if state.bus != nil && state.bus.ended() && (message.Verb == "reprobe" || message.Verb == "notify" || message.Verb == "close") && !(message.Verb == "notify" && state.snapshot.Config.DND) {
		return false, state.reply("unavailable", "owned session bus ended or guardian ownership was lost; adapter work requires a new login", 0)
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
		oldPower := state.snapshot.Config.Power
		state.snapshot.Config = *message.Config
		if oldPower != message.Config.Power {
			state.snapshot.Power = unavailablePower(message.Config.Power, operation, "power policy changed; explicit provider probe required")
		}
		state.refreshNotifications()
		return false, state.reply("applied", "", 0)
	}
	if message.Verb == "notify" && state.snapshot.Config.DND {
		return false, state.reply("suppressed", "", 0)
	}
	if (message.Verb == "notify" || message.Verb == "close") && state.snapshot.Config.Notifications != "external" {
		return false, state.reply("unavailable", "external notifications are disabled", 0)
	}
	if message.Verb == "power-probe" || sessionservice.IsPowerVerb(message.Verb) || message.Verb == "lock" {
		if message.Verb == "power-probe" {
			// Status remains available during the probe. Its intermediate tokens
			// must never present the previous provider as request capable.
			state.snapshot.Power = unavailablePower(state.snapshot.Config.Power, operation, "provider probe pending; power confirmations unavailable")
		}
		if message.Verb == "lock" {
			state.snapshot.Locker.State = "starting"
			state.snapshot.Locker.Detail = "locker start pending; secure readiness unconfirmed"
		}
		return true, sessionservice.Reply{}
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
	state.fenceBusLoss()
	if job.request.message.Verb == "power-probe" || sessionservice.IsPowerVerb(job.request.message.Verb) || job.request.message.Verb == "lock" {
		return state.completeProvider(job)
	}

	// An owned daemon is never replaced in this parent lifetime. A probe that
	// completed before its Wait result cannot revive that terminal lifecycle.
	if state.bus != nil && state.bus.ended() {
		return state.reply("failed", "owned session bus ended or guardian ownership was lost; late adapter result discarded", 0)
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

func (state *serviceState) completeProvider(job serviceJob) sessionservice.Reply {
	return state.completeProviderWithIdentity(job, randomIdentity)
}

func (state *serviceState) completeProviderWithIdentity(job serviceJob, identity func(int) (string, error)) sessionservice.Reply {
	verb := job.request.message.Verb
	if verb == "power-probe" {
		// A consumer may have restarted and read status while the probe was
		// pending. Neither token from that snapshot may authorize the result.
		generation, err := identity(16)
		operation := ""
		if err == nil {
			operation, err = identity(16)
		}
		if err != nil {
			state.snapshot.Power = unavailablePower(state.snapshot.Config.Power, state.snapshot.Power.Generation, "provider result refused: identity generation failed")
			state.snapshot.Power.State = "failed"
			return state.reply("failed", "provider result refused: identity generation failed: "+err.Error(), 0)
		}
		job.power.Generation = generation
		state.snapshot.Operation = operation
	}

	if verb == "lock" {
		if job.locker != nil {
			state.locker = job.locker
			state.snapshot.Locker.State = "running"
			state.snapshot.Locker.Generation = state.snapshot.Operation
			state.snapshot.Locker.Detail = "observed locker child running; secure readiness unconfirmed"
		} else {
			state.snapshot.Locker.State = "failed"
			state.snapshot.Locker.Detail = detail(job.err.Error())
		}
	} else {
		state.snapshot.Power = job.power
		if job.err != nil && verb == "power-probe" {
			state.snapshot.Power.State = "failed"
			state.snapshot.Power.Detail = detail(job.err.Error())
		}
		if verb == "power-probe" {
			state.plans = job.plans
			if state.locker == nil {
				state.snapshot.Locker.Generation = state.snapshot.Operation
				state.snapshot.Locker.State = "unavailable"
				state.snapshot.Locker.Detail = "xlock/slock unavailable; secure readiness unconfirmed"
				if len(job.plans.Locker) > 0 {
					state.snapshot.Locker.State = "installed"
					state.snapshot.Locker.Detail = "fixed locker installed; secure readiness unconfirmed"
				}
			}
		}
	}
	message := ""
	if job.err != nil {
		message = job.err.Error()
	}
	return state.reply(job.outcome, message, 0)
}

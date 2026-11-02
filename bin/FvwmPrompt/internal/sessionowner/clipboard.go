// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"context"
	"errors"

	"github.com/fvwmorg/FvwmPrompt/internal/clipboard"
)

func runClipboardJob(request controlRequest, owner *clipboard.Client) serviceJob {
	job := serviceJob{request: request, outcome: "unavailable"}
	if request.message.Verb == "clipboard-status" {
		capabilities := clipboard.DisabledCapabilities()
		if owner != nil {
			capabilities = owner.Capabilities(request.ctx)
		}
		job.clipboardCapabilities = &capabilities
		job.outcome = capabilities.State
		return job
	}
	if owner == nil {
		job.err = errors.New("clipboard owner is disabled; explicit limits and qualified lock state remain required")
		return job
	}
	if request.message.Verb == "clipboard-list" {
		page, err := owner.Page(request.ctx, *request.message.ClipboardPage)
		job.err = err
		if err != nil {
			job.outcome = "failed"
			return job
		}
		job.clipboardPage = &page
		job.outcome = "ready"
		if page.Result != clipboard.OK {
			job.outcome = "rejected"
		}
		return job
	}
	reply, err := owner.Call(request.ctx, *request.message.Clipboard)
	job.err = err
	if err != nil {
		job.outcome = "failed"
		return job
	}
	job.clipboard = &reply
	job.outcome = "applied"
	if reply.Result != clipboard.OK {
		job.outcome = "rejected"
	}
	return job
}

func pauseClipboard(ctx context.Context, owner *clipboard.Client, epoch uint64) error {
	reply, err := owner.Call(ctx, clipboard.Request{Verb: clipboard.State, Ref: clipboard.Ref{Epoch: epoch}, Value: clipboard.Locked})
	if err != nil || reply.Result != clipboard.OK || reply.Mode != clipboard.Locked {
		owner.Close()
		return errors.New("clipboard pause unconfirmed; locker request refused")
	}
	// A running/exited locker is not qualified unlocked evidence. No lifecycle
	// callback resumes capture or history access; only a future qualified source
	// may drive that separate internal transition.
	return nil
}

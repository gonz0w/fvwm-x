//go:build go1.25

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filejobs

import (
	"os"
	"path/filepath"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

// This is bounded to admitted top-level targets. False SourceRemaining means
// the original inode was not observed, never that this job deleted it. Worker
// output loss stops immediately rather than fabricating reliable outcomes.
func (r *runner) notAttempted(root *os.Root, items []filemodel.Item, start int, state, detail string) ([]Outcome, error) {
	outcomes := make([]Outcome, 0, len(items)-start)
	if r.req.Operation == "mkdir" {
		if r.outputErr != nil {
			return nil, r.outputErr
		}
		r.item = 0
		r.path = nil
		r.destination = []byte(filepath.Join(string(r.req.Snapshot.Path), string(r.req.Name)))
		out := Outcome{State: state, Detail: detail + "; new folder was not created"}
		if err := r.event("item", state, out.Detail, &out); err != nil {
			return nil, err
		}
		return []Outcome{out}, nil
	}
	for i := start; i < len(items); i++ {
		if r.outputErr != nil {
			return outcomes, r.outputErr
		}
		item := items[i]
		r.item = i
		r.path = []byte(filepath.Join(string(r.req.Snapshot.Path), string(item.Name)))
		r.destination = nil
		out := Outcome{State: state, Detail: detail}
		if root != nil {
			info, err := root.Lstat(string(item.Name))
			out.SourceRemaining = err == nil && stamp(info).Device == item.Identity.Device && stamp(info).Inode == item.Identity.Inode
		}
		if !out.SourceRemaining {
			out.Detail += "; original source inode not observed (missing, replaced, or unavailable), no deletion attempted"
		}
		outcomes = append(outcomes, out)
		if err := r.event("item", state, out.Detail, &out); err != nil {
			return outcomes, err
		}
	}
	return outcomes, nil
}

func (r *runner) refused(root *os.Root, items []filemodel.Item, state, detail string) error {
	if _, err := r.notAttempted(root, items, 0, state, "Not attempted: "+detail); err != nil {
		return err
	}
	return r.event("finished", state, detail, nil)
}

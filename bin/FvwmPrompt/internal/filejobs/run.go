//go:build go1.25

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filejobs

import (
	"context"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

var errSkip = errors.New("skipped by user")

type runner struct {
	ctx                                    context.Context
	req                                    Request
	controls                               <-chan Control
	emit                                   func(Event) error
	sequence, decision, done, total, bytes uint64
	lastProgress                           time.Time
	item                                   int
	path, destination                      []byte
	cancelled                              bool
	known                                  bool
	outputErr                              error
	receipt                                *copyReceipt
}

func (r *runner) event(kind, state, detail string, outcome *Outcome) error {
	if r.outputErr != nil {
		return r.outputErr
	}
	r.sequence++
	if len(detail) > 1024 {
		detail = detail[:1024]
	}
	if outcome != nil && len(outcome.Detail) > 1024 {
		bounded := *outcome
		bounded.Detail = bounded.Detail[:1024]
		outcome = &bounded
	}
	r.outputErr = r.emit(Event{Schema: Schema, ID: r.req.ID, Generation: r.req.Generation,
		Sequence: r.sequence, Kind: kind, State: state, Item: r.item, Path: r.path,
		Destination: r.destination, Done: r.done, Total: r.total, TotalKnown: r.known,
		Bytes: r.bytes, Decision: r.decision, Detail: detail, Outcome: outcome})
	return r.outputErr
}

func (r *runner) poll() error {
	if r.cancelled {
		return context.Canceled
	}
	select {
	case <-r.ctx.Done():
		return r.ctx.Err()
	default:
	}
	for i := 0; i < 32; i++ {
		select {
		case c, ok := <-r.controls:
			if !ok {
				r.controls = nil
				return nil
			}
			if c.Schema == Schema && c.ID == r.req.ID && c.Kind == "cancel" {
				r.cancelled = true
				return context.Canceled
			}
		default:
			return nil
		}
	}
	return nil
}

func (r *runner) progress(force bool) error {
	if err := r.poll(); err != nil {
		return err
	}
	if !force && time.Since(r.lastProgress) < 100*time.Millisecond {
		return nil
	}
	r.lastProgress = time.Now()
	return r.event("progress", "running", "", nil)
}

// choose admits a new component only after an explicit correlated decision.
// Creation itself is exclusive, so a later competing creation cannot be lost.
func (r *runner) choose(root *os.Root, name string) (string, error) {
	for attempts := 0; attempts < MaxTargets; attempts++ {
		if len(filepath.Join(root.Name(), name)) > filemodel.MaxPath {
			return "", errors.New("destination path exceeds budget")
		}
		if err := r.poll(); err != nil {
			return "", err
		}
		_, err := root.Lstat(name)
		if errors.Is(err, os.ErrNotExist) {
			return name, nil
		}
		if err != nil {
			return "", err
		}
		r.decision++
		r.destination = []byte(filepath.Join(root.Name(), name))
		if err = r.event("decision", "waiting", "Destination exists: skip, choose a new name, or cancel", nil); err != nil {
			return "", err
		}
		timer := time.NewTimer(time.Minute)
		decided := false
		for !decided {
			select {
			case <-r.ctx.Done():
				timer.Stop()
				return "", r.ctx.Err()
			case <-timer.C:
				return "", errors.New("conflict decision expired")
			case c, ok := <-r.controls:
				if !ok {
					timer.Stop()
					return "", context.Canceled
				}
				if c.Schema != Schema || c.ID != r.req.ID {
					continue
				}
				if c.Kind == "cancel" {
					timer.Stop()
					r.cancelled = true
					return "", context.Canceled
				}
				if c.Kind != "decide" || c.Decision != r.decision {
					continue
				}
				switch c.Choice {
				case "skip":
					timer.Stop()
					return "", errSkip
				case "cancel":
					timer.Stop()
					r.cancelled = true
					return "", context.Canceled
				case "rename":
					if !validName(c.Name) {
						continue
					}
					name = string(c.Name)
					decided = true
				}
			}
		}
		timer.Stop()
	}
	return "", errors.New("too many conflicting names")
}

func stateFor(err error, changed bool) string {
	if changed {
		return "partial"
	}
	if errors.Is(err, errSkip) {
		return "skipped"
	}
	if errors.Is(err, context.Canceled) || errors.Is(err, context.DeadlineExceeded) {
		return "cancelled"
	}
	return "failed"
}

// Run processes one job with no queue. Plans are bounded before mutation, then
// each step revalidates observed identities. Open roots anchor directories, but
// neither they nor stat/unlink pairs freeze entries against same-user writers.
func Run(ctx context.Context, req Request, controls <-chan Control, emit func(Event) error) error {
	if err := ValidateRequest(req); err != nil {
		return err
	}
	if emit == nil {
		return errors.New("missing event consumer")
	}
	r := runner{ctx: ctx, req: req, controls: controls, emit: emit}
	if err := r.event("accepted", "accepted", "", nil); err != nil {
		return err
	}
	items, _ := selected(req)
	root, err := openObserved(req)
	if err != nil {
		state := "failed"
		if ctx.Err() != nil {
			state = "cancelled"
		}
		return r.refused(nil, items, state, err.Error())
	}
	defer root.Close()
	plans := make([][]treeNode, len(items))
	for i, item := range items {
		if err = r.poll(); err != nil {
			break
		}
		info, e := root.Lstat(string(item.Name))
		if e != nil || stamp(info) != item.Identity {
			err = errors.New("selected item changed; refresh before acting")
			break
		}
		if req.Operation == "link" {
			plans[i] = []treeNode{{path: string(item.Name), info: info, identity: item.Identity}}
			r.total++
		} else {
			plans[i], err = plan(root, string(item.Name), &r)
		}
		if err != nil {
			break
		}
	}
	if err != nil {
		return r.refused(root, items, stateFor(err, false), err.Error())
	}
	if req.Operation == "mkdir" {
		r.total = 1
	}
	r.known = true
	if err = r.poll(); err != nil {
		return r.refused(root, items, stateFor(err, false), err.Error())
	}
	var destination *os.Root
	if req.Operation == "copy" || req.Operation == "move" || req.Operation == "link" {
		destination, err = os.OpenRoot(string(req.Destination))
		if err != nil {
			return r.refused(root, items, "failed", err.Error())
		}
		defer destination.Close()
	} else {
		destination = root
	}
	outcomes := make([]Outcome, 0, len(items)+1)
	if req.Operation == "mkdir" {
		name, e := r.choose(root, string(req.Name))
		out := Outcome{}
		if e == nil {
			e = root.Mkdir(name, 0777)
		}
		if e == nil {
			r.done++
			out = Outcome{State: "completed", DestinationPresent: true, ContentComplete: true, MetadataComplete: true}
		} else {
			out.State = stateFor(e, false)
			out.Detail = e.Error()
		}
		r.destination = []byte(filepath.Join(root.Name(), name))
		outcomes = append(outcomes, out)
		if err = r.event("item", out.State, out.Detail, &out); err != nil {
			return err
		}
	} else {
		for i, item := range items {
			r.item = i
			r.path = []byte(filepath.Join(string(req.Snapshot.Path), string(item.Name)))
			r.destination = nil
			out := r.operate(root, destination, plans[i], item)
			outcomes = append(outcomes, out)
			if err = r.event("item", out.State, out.Detail, &out); err != nil {
				return err
			}
			if out.State == "cancelled" || r.cancelled || ctx.Err() != nil {
				remaining, e := r.notAttempted(root, items, i+1, "cancelled", "Not attempted because job cancelled")
				if e != nil {
					return e
				}
				outcomes = append(outcomes, remaining...)
				break
			}
		}
	}
	final := "completed"
	completed := 0
	failed := 0
	for _, o := range outcomes {
		if o.State == "completed" {
			completed++
		}
		if o.State == "partial" {
			final = "partial"
		}
		if o.State == "failed" || o.State == "cancelled" {
			failed++
		}
	}
	if len(outcomes) > 0 && completed == 0 && failed == 0 && final != "partial" {
		final = "skipped"
	}
	if final != "partial" && failed > 0 {
		final = "failed"
		if completed > 0 {
			final = "partial"
		}
	}
	if (r.cancelled || ctx.Err() != nil) && final != "partial" {
		final = "cancelled"
		if completed > 0 {
			final = "partial"
		}
	}
	return r.event("finished", final, fmt.Sprintf("%d completed, %d other outcomes; cancellation is not rollback", completed, len(outcomes)-completed), nil)
}

func (r *runner) operate(source, destination *os.Root, nodes []treeNode, item filemodel.Item) (out Outcome) {
	out = Outcome{SourceRemaining: true}
	r.receipt = nil
	defer func() {
		info, err := source.Lstat(nodes[0].path)
		out.SourceRemaining = err == nil && stamp(info).Device == nodes[0].identity.Device && stamp(info).Inode == nodes[0].identity.Inode
	}()
	if err := r.poll(); err != nil {
		out.State = "cancelled"
		out.Detail = err.Error()
		return out
	}
	var err error
	switch r.req.Operation {
	case "delete":
		removed := 0
		removed, err = r.remove(source, nodes)
		out.ContentComplete = err == nil
		if err == nil {
			err = retireTrashMetadata(source, item)
		}
		out.MetadataComplete = err == nil
		if err != nil {
			out.State = stateFor(err, removed > 0)
		}
	case "trash":
		return r.trash(source, nodes, item)
	case "restore":
		return r.restore(source, nodes, item)
	default:
		name := string(item.Name)
		if len(r.req.Name) > 0 {
			name = string(r.req.Name)
		}
		if r.req.Operation == "rename" && name == string(item.Name) {
			out.State = "completed"
			out.DestinationPresent = true
			out.ContentComplete = true
			out.MetadataComplete = true
			out.Detail = "name unchanged"
			return out
		}
		name, err = r.choose(destination, name)
		if err != nil {
			out.State = stateFor(err, false)
			out.Detail = err.Error()
			return out
		}
		r.destination = []byte(filepath.Join(destination.Name(), name))
		if r.req.Operation == "link" {
			err = checkNode(source, nodes[0], false)
			if err == nil {
				info, e := os.Stat(string(r.req.Snapshot.Path))
				if e != nil || !sameObject(stamp(info), r.req.Snapshot.Root) {
					err = errors.New("source folder path changed before link creation")
				}
			}
			if err == nil {
				err = destination.Symlink(string(r.path), name)
			}
			if err == nil {
				r.done += uint64(len(nodes))
				out.DestinationPresent = true
				out.ContentComplete = true
				out.MetadataComplete = true
				out.Detail = "symbolic link; target not followed"
			}
		} else {
			if r.req.Operation == "move" || r.req.Operation == "rename" {
				var used bool
				out, used, err = r.moveFile(source, destination, nodes[0], name)
				if used {
					if err != nil {
						out.State = stateFor(err, out.DestinationPresent)
						out.Detail = err.Error()
					} else {
						out.State = "completed"
					}
					return out
				}
			}
			out, err = r.copy(source, destination, nodes, name)
			if err == nil && (r.req.Operation == "move" || r.req.Operation == "rename") {
				_, err = r.remove(source, nodes)
				out.SourceRemaining = err != nil
				out.Detail = "exclusive copy/remove; non-atomic; no overwrite"
			}
		}
	}
	if err != nil {
		if out.State == "" {
			out.State = stateFor(err, out.DestinationPresent)
		}
		out.Detail = err.Error()
	} else {
		out.State = "completed"
	}
	return out
}

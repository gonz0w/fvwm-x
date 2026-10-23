// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"errors"
	"fmt"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

type targetFlags []string

func (t *targetFlags) String() string { return "" }
func (t *targetFlags) Set(value string) error {
	if len(*t) >= collection.MaxSelected || len(value) > filemodel.MaxTarget {
		return errors.New("target count or encoded length exceeds limit")
	}
	*t = append(*t, value)
	return nil
}

// Startup validation performs no filesystem calls. All target resolution is
// owned by the same bounded cancellable worker used for ordinary navigation.
func prepareOptions(o *options, dirSet bool, reveal, properties targetFlags) error {
	if o.watch != "" && o.watch != "off" && o.watch != "auto" {
		return errors.New("watch must be off or auto")
	}
	if o.view != "" && o.view != "list" && o.view != "grid" {
		return errors.New("view must be list or grid")
	}
	if (len(reveal) != 0 && len(properties) != 0) ||
		(dirSet && len(reveal)+len(properties) != 0) {
		return errors.New("use --dir, repeated --reveal, or repeated --properties targets")
	}
	if len(reveal)+len(properties) == 0 {
		path, err := filemodel.ParseTarget(o.dir)
		if err != nil {
			return err
		}
		o.dir = string(path)
		return nil
	}
	values := reveal
	if len(properties) != 0 {
		values = properties
	}
	if len(values) > collection.MaxSelected {
		return errors.New("at most 256 targets are supported")
	}
	targets := make([][]byte, 0, len(values))
	for _, value := range values {
		path, err := filemodel.ParseTarget(value)
		if err != nil {
			return err
		}
		targets = append(targets, path)
	}
	o.targets, o.properties = targets, len(properties) != 0
	return nil
}

func (o options) initialRequest() workRequest {
	if len(o.targets) != 0 {
		return workRequest{Operation: "locate", Targets: o.targets, Hidden: o.hidden}
	}
	return workRequest{Operation: "scan", Path: []byte(o.dir), Hidden: o.hidden}
}

// A live target handoff replaces selection and clears the admitted filter so
// every requested target can be revealed. Validate the whole candidate before
// publishing any state; paths, history and inspector follow that same commit.
func (b *browser) publishLocated(result workResult, navigation string) error {
	snapshot := result.Snapshot
	if snapshot == nil || snapshot.Schema != 1 || len(result.Selected) == 0 {
		return errors.New("invalid located snapshot")
	}
	entries, err := snapshot.OrderedEntries(b.ordering(), b.reverse)
	if err != nil {
		return err
	}
	candidate := b.model
	if err := candidate.SetFilter(""); err != nil {
		return err
	}
	if err := candidate.Replace(entries); err != nil {
		return err
	}
	if err := candidate.SelectIDs(candidate.Generation, result.Selected); err != nil {
		return err
	}
	candidate.Focus = result.Selected[0]
	inspected := []string(nil)
	if navigation == "properties" {
		if _, err := snapshot.ObservedSelection(result.Selected); err != nil {
			return err
		}
		inspected = append([]string(nil), result.Selected...)
	}
	old := ""
	if b.snapshot != nil {
		old = string(b.snapshot.Path)
	}
	if old != "" && old != string(snapshot.Path) {
		b.history = append(b.history, old)
		if len(b.history) > 32 {
			b.history = b.history[1:]
		}
	}
	b.model, b.snapshot, b.hidden = candidate, snapshot, snapshot.Hidden
	b.inspected, b.replaceSelection, b.revealFocus = inspected, true, true
	b.status = fmt.Sprintf("Revealed %d requested entries; no item was opened.", len(result.Selected))
	if navigation == "properties" {
		b.status = fmt.Sprintf("Showing observed properties for %d requested entries.", len(result.Selected))
	}
	if b.watch != nil {
		b.watch.desire(snapshot, true)
	}
	return nil
}

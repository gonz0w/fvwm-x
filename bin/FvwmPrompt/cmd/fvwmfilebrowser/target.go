// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"errors"

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
	if (len(reveal) != 0 && len(properties) != 0) ||
		(dirSet && len(reveal)+len(properties) != 0) || len(properties) > 1 {
		return errors.New("use --dir, repeated --reveal, or one --properties target")
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
		return errors.New("at most 256 reveal targets are supported")
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

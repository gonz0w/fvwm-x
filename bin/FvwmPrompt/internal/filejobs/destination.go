//go:build go1.25

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filejobs

import (
	"errors"
	"os"
	"path/filepath"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

// A receipt tracks created entries, not just pathnames. It owns no additional
// descriptors: the caller retains the opened destination root through removal.
// Checks refuse replacements across the whole copy window; the final individual
// pathname syscall still cannot freeze another same-user writer atomically.
type copyReceipt struct {
	root    *os.Root
	entries map[string]filemodel.Identity
	sources map[string]string
}

func newReceipt(root *os.Root) *copyReceipt {
	return &copyReceipt{root: root, entries: make(map[string]filemodel.Identity), sources: make(map[string]string)}
}
func sameEntry(a, b filemodel.Identity) bool {
	return a.Device == b.Device && a.Inode == b.Inode && a.Mode.Type() == b.Mode.Type()
}
func (c *copyReceipt) add(source, target string, info os.FileInfo) {
	c.entries[target] = stamp(info)
	c.sources[source] = target
}
func (c *copyReceipt) check(target string, full bool) error {
	want, ok := c.entries[target]
	if !ok {
		return errors.New("destination was not created by this job")
	}
	info, err := c.root.Lstat(target)
	if err != nil {
		return err
	}
	actual := stamp(info)
	if !sameEntry(actual, want) || full && actual != want {
		return errors.New("created destination changed; source retained")
	}
	return nil
}
func (c *copyReceipt) parents(target string) error {
	for parent := filepath.Dir(target); parent != "."; parent = filepath.Dir(parent) {
		if err := c.check(parent, false); err != nil {
			return err
		}
	}
	return nil
}
func (c *copyReceipt) verify() error {
	for target := range c.entries {
		if err := c.parents(target); err != nil {
			return err
		}
		if err := c.check(target, true); err != nil {
			return err
		}
	}
	return nil
}
func (c *copyReceipt) forSource(source string) error {
	target, ok := c.sources[source]
	if !ok {
		return errors.New("missing created destination receipt")
	}
	if err := c.parents(target); err != nil {
		return err
	}
	return c.check(target, true)
}

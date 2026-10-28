// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"errors"
	"os/exec"
)

const maxBrowserLaunches = 8

// Bound only Files-menu display. Paths, stored names and launch argv retain
// their original bytes; a rune cap is not a guarantee for every font/screen.
func filesMenuLabel(text string) string {
	runes := []rune(clean(text, len(text)))
	if len(runes) <= 48 {
		return string(runes)
	}
	return string(runes[:22]) + "..." + string(runes[len(runes)-23:])
}

type browserCompletion struct {
	command *exec.Cmd
	err     error
}

type browserHandoff struct {
	helper, menuToken string
	pending           map[*exec.Cmd]struct{}
	completed         chan browserCompletion
}

func newBrowserHandoff() browserHandoff {
	return browserHandoff{pending: make(map[*exec.Cmd]struct{}),
		completed: make(chan browserCompletion, maxBrowserLaunches)}
}

func (d *Controller) openFilesBrowser(argument string) error {
	if d.closed || !identity(argument, 32) || argument != d.browser.menuToken {
		return errors.New("Files menu is stale; reopen Files before opening its folder in the browser.")
	}
	if !validText(d.directory, 4096) || d.directory == "" {
		return errors.New("Files menu has no committed folder to open.")
	}
	if len(d.browser.pending) >= maxBrowserLaunches {
		return errors.New("Too many file-browser launches are pending; no request was sent.")
	}
	if d.browser.helper == "" {
		return errors.New("File browser is unavailable; install fvwm-file-browser or set FVWM_FILE_BROWSER.")
	}
	if _, err := exec.LookPath(d.browser.helper); err != nil {
		return errors.New("File browser is unavailable; check FVWM_FILE_BROWSER and the installed browser.")
	}
	// The callback contains only an opaque identity. Snapshot the committed
	// literal directory into argv, without shell or FVWM command expansion.
	command, err := d.startDetached([]string{d.browser.helper, "--reuse", "--", d.directory}, "")
	if err != nil {
		return errors.New("Could not start file browser; no folder request was sent.")
	}
	d.browser.pending[command] = struct{}{}
	completed, slots := d.browser.completed, d.spawnSlots
	// Start is admission, not completion. One waiter releases the global slot;
	// only the event loop consumes this exact result and releases browser
	// admission. At most eight sends fit even after Close stops polling. Neither
	// channel closes, and independent browsers have no controller deadline.
	go func() {
		err := command.Wait()
		<-slots
		completed <- browserCompletion{command: command, err: err}
	}()
	return nil
}

func (d *Controller) pollBrowserLaunches() {
	for budget := 0; budget < maxBrowserLaunches; budget++ {
		select {
		case result := <-d.browser.completed:
			if _, present := d.browser.pending[result.command]; !present {
				continue
			}
			delete(d.browser.pending, result.command)
			if result.err != nil && !d.closed {
				d.notice("File browser exited unsuccessfully.\nThe folder request may have an unknown outcome.\nInspect the existing browser before retrying.")
			}
		default:
			return
		}
	}
}

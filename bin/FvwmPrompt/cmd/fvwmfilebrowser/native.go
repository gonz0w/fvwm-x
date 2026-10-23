// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"encoding/binary"
	"errors"
	"fmt"
	"net"
	"strconv"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
)

func nativePacket(value string, generation uint32) ([]byte, error) {
	if generation == 0 || len(value) > 692 {
		return nil, errors.New("stale native collection generation")
	}
	data, err := decodeField(value)
	if err != nil || len(data) < 6 || binary.BigEndian.Uint32(data) != generation {
		return nil, errors.New("stale native collection generation")
	}
	return data[4:], nil
}

func (b *browser) currentPath() string {
	if b.snapshot == nil {
		return ""
	}
	return string(b.snapshot.Path)
}
func (b *browser) nativeTag(phase string) string {
	return fmt.Sprintf("%s_%x_%s", b.instance, b.renderSerial, phase)
}
func (b *browser) sendCommands(commands []string) error {
	deadline := time.NewTimer(time.Until(b.renderDeadline))
	defer deadline.Stop()
	for i, command := range commands {
		if err := b.session.Send(command); err != nil {
			return err
		}
		if b.acks != nil && (i%32 == 31 || i == len(commands)-1) {
			tag := fmt.Sprintf("FVWM_FILES_SYNC %s %d %d", b.instance, b.renderSerial, i)
			if err := b.session.Send("Echo " + tag); err != nil {
				return err
			}
			for {
				select {
				case value := <-b.acks:
					if value == tag {
						goto acknowledged
					}
				case <-b.session.Done():
					return net.ErrClosed
				case <-deadline.C:
					return errors.New("native collection configuration timed out")
				}
			}
		acknowledged:
		}
	}
	return nil
}
func (b *browser) waitNative(token string, published uint32, pending uint32) ([]byte, error) {
	deadline := time.NewTimer(time.Until(b.renderDeadline))
	defer deadline.Stop()
	for {
		select {
		case reply := <-b.native:
			fields := strings.Fields(reply)
			if len(fields) < 2 || fields[1] != token {
				continue
			}
			if len(fields) != 6 || fields[0] != "FVWM_FORM_LIST" || fields[2] != strconv.FormatUint(uint64(published), 10) || fields[3] != strconv.FormatUint(uint64(pending), 10) || fields[4] != "0" {
				return nil, errors.New("native collection publication refused")
			}
			return nativePacket(fields[5], published)
		case <-b.session.Done():
			return nil, net.ErrClosed
		case <-deadline.C:
			return nil, errors.New("native collection publication timed out")
		}
	}
}
func (b *browser) snapshotNative(phase string, published, pending uint32) ([]byte, error) {
	tag := b.nativeTag(phase)
	if err := b.session.Send("SendToModule " + b.alias + " ListStatus picks " + tag + " selection"); err != nil {
		return nil, err
	}
	return b.waitNative(tag, published, pending)
}
func entryIDs(entries []collection.Entry) []string {
	ids := make([]string, 0, len(entries))
	for _, entry := range entries {
		ids = append(ids, entry.ID)
	}
	return ids
}
func entriesEqual(left, right []collection.Entry) bool {
	if len(left) != len(right) {
		return false
	}
	for i := range left {
		if left[i] != right[i] {
			return false
		}
	}
	return true
}
func iconName(kind string) string {
	switch kind {
	case "file", "directory":
		return kind
	case "symlink":
		return "link"
	default:
		return "other"
	}
}
func (b *browser) update() error {
	if err := b.session.Send("SendToModule " + b.alias + " RenewTimeout 3000"); err != nil {
		return err
	}
	entries := b.model.VisibleEntries()
	if !entriesEqual(entries, b.viewEntries) || b.replaceSelection {
		next := b.nativeGeneration + 1
		commands := []string{fmt.Sprintf("SendToModule %s ListBegin picks %d %d", b.alias, b.nativeGeneration, next)}
		previous := make(map[string]int, len(b.viewEntries))
		for i, entry := range b.viewEntries {
			previous[entry.ID] = i + 1
		}
		for _, entry := range entries {
			source := previous[entry.ID]
			choice := "keep"
			if b.replaceSelection || source == 0 {
				choice = "off"
				if b.model.Selected[entry.ID] {
					choice = "on"
				}
			}
			commands = append(commands, fmt.Sprintf("SendToModule %s ListEntry picks %d %d \"%s\" %s %s", b.alias, next, source, displayText(entry.Label, 90), iconName(entry.Kind), choice))
		}
		var packet []byte
		var err error
		// Bound the target module's queue too: a core Echo cannot prove that
		// Form consumed the previous batch. One native reply retires each batch.
		for offset := 0; offset < len(commands); offset += 32 {
			end := offset + 32
			if end > len(commands) {
				end = len(commands)
			}
			if err = b.sendCommands(commands[offset:end]); err != nil {
				return err
			}
			packet, err = b.snapshotNative(fmt.Sprintf("u%d", end), b.nativeGeneration, next)
			if err != nil {
				return err
			}
		}
		if !b.replaceSelection {
			if err := b.model.SelectNativeView(b.model.Generation, entryIDs(b.viewEntries), packet); err != nil {
				b.status = err.Error()
				return b.render()
			}
		}
		if err := b.session.Send(fmt.Sprintf("SendToModule %s ListCommit picks %d", b.alias, next)); err != nil {
			return err
		}
		packet, err = b.snapshotNative("committed", next, 0)
		if err != nil {
			// A late edit can exhaust the native selection budget. Recover
			// from the admitted model instead of leaving split mappings.
			b.status = err.Error()
			return b.render()
		}
		if b.revealFocus {
			for index, entry := range entries {
				if entry.ID == b.model.Focus {
					if err := b.session.Send(fmt.Sprintf("SendToModule %s ListFocus picks %d %d", b.alias, next, index+1)); err != nil {
						return err
					}
					packet, err = b.snapshotNative("focused", next, 0)
					if err != nil {
						return err
					}
					break
				}
			}
		}
		b.nativeGeneration = next
		b.viewEntries = entries
		b.replaceSelection = false
		if err := b.model.SelectNative(b.model.Generation, packet); err != nil {
			b.status = err.Error()
			return b.render()
		}
	} else {
		packet, err := b.snapshotNative("current", b.nativeGeneration, 0)
		if err != nil {
			return err
		}
		if err := b.model.SelectNative(b.model.Generation, packet); err != nil {
			b.status = err.Error()
			return b.render()
		}
	}
	labels := []nativeLabel{{"path-label", b.currentPath()}, {"status", b.status}, {"watcher", b.watchText()}, {"projection", b.projectionText()}, {"counts", b.countText()}}
	labels = append(labels, b.inspectorLabels()...)
	commands := make([]string, 0, len(labels))
	for _, label := range labels {
		commands = append(commands, "SendToModule "+b.alias+" SetLabel "+label.name+" \""+displayText(label.value, 120)+"\"")
	}
	if err := b.sendCommands(commands); err != nil {
		return err
	}
	_, err := b.snapshotNative("painted", b.nativeGeneration, 0)
	return err
}
func (b *browser) show() error {
	if b.renderSerial == ^uint64(0) {
		return errors.New("render generation exhausted")
	}
	b.renderSerial++
	b.renderDeadline = time.Now().Add(workerLimit)
	var err error
	if b.alias == "" || b.native == nil || b.renderPath != b.currentPath() || b.renderView != b.viewing() || b.nativeGeneration >= 2147483647 {
		err = b.render()
	} else {
		err = b.update()
	}
	if err != nil {
		return err
	}
	b.revealFocus = false
	active := 0
	if b.active {
		active = 1
	}
	if err := b.session.Send(fmt.Sprintf("Echo FVWM_FILES_RENDER %s %d %d %d %d", b.instance, b.form, b.renderSerial, b.nativeGeneration, active)); err != nil {
		return err
	}
	return b.session.Send("Echo FVWM_FILES_WATCH " + b.instance + " " + b.watch.state())
}

type nativeLabel struct{ name, value string }

func (b *browser) projectionText() string {
	direction := "ascending"
	if b.reverse {
		direction = "descending"
	}
	return fmt.Sprintf("Filter: %s; sort: %s %s", b.model.Filter(), b.ordering(), direction)
}
func (b *browser) countText() string {
	return fmt.Sprintf("%d/%d visible; %d selected (%d filtered out, limit %d)", b.model.VisibleCount(), len(b.model.Entries), len(b.model.Selected), b.model.HiddenSelectedCount(), collection.MaxSelected)
}
func (b *browser) inspectorLabels() []nativeLabel {
	labels := []nativeLabel{{"inspector", "Properties (F2): select entries to inspect observed metadata."}, {"detail1", ""}, {"detail2", ""}, {"detail3", ""}}
	if b.snapshot != nil && len(b.inspected) != 0 {
		if details, err := b.snapshot.ObservedSelection(b.inspected); err == nil {
			labels[0].value = "Observed metadata; Refresh updates it. No link is followed."
			for i, text := range details {
				if i < 3 {
					labels[i+1].value = text
				}
			}
		}
	}
	return labels
}

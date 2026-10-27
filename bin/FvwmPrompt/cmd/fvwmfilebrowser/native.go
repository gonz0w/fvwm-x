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
	"github.com/fvwmorg/FvwmPrompt/internal/viewbinding"
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
	packet, _, err := b.waitNativeGeneration(token, published, pending, 0)
	return packet, err
}

// Only layout observations admit an unchanged generation as an atomic refusal.
// Other transactions still require their exact published/pending pair.
func (b *browser) waitNativeGeneration(token string, published, pending, unchanged uint32) ([]byte, uint32, error) {
	deadline := time.NewTimer(time.Until(b.renderDeadline))
	defer deadline.Stop()
	for {
		select {
		case reply := <-b.native:
			fields := strings.Fields(reply)
			if len(fields) < 2 || fields[1] != token {
				continue
			}
			if len(fields) != 6 || fields[0] != "FVWM_FORM_LIST" || fields[3] != strconv.FormatUint(uint64(pending), 10) || fields[4] != "0" {
				return nil, 0, errors.New("native collection publication refused")
			}
			observed := published
			if unchanged != 0 && fields[2] == strconv.FormatUint(uint64(unchanged), 10) {
				observed = unchanged
			} else if fields[2] != strconv.FormatUint(uint64(published), 10) {
				return nil, 0, errors.New("native collection publication refused")
			}
			packet, err := nativePacket(fields[5], observed)
			return packet, observed, err
		case <-b.session.Done():
			return nil, 0, net.ErrClosed
		case <-deadline.C:
			return nil, 0, errors.New("native collection publication timed out")
		}
	}
}
func (b *browser) snapshotNative(phase string, published, pending uint32) ([]byte, error) {
	tag, err := b.requestNativeSnapshot(phase)
	if err != nil {
		return nil, err
	}
	return b.waitNative(tag, published, pending)
}

func (b *browser) requestNativeSnapshot(phase string) (string, error) {
	tag := b.nativeTag(phase)
	if err := b.session.Send("SendToModule " + b.alias + " ListStatus picks " + tag + " selection"); err != nil {
		return "", err
	}
	return tag, nil
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

func (b *browser) updateLayout() (err error) {
	const refused = "View unchanged: enlarge the browser window and retry View (F7)."
	mode := b.requestedView()
	if mode == b.renderView {
		return nil
	}
	defer func() {
		if err != nil {
			b.view = b.renderView
		}
	}()
	if b.nativeGeneration >= 2147483647 {
		return errors.New("native collection generation exhausted")
	}
	columns := 1
	if mode == "grid" {
		columns = 8
	}
	next := b.nativeGeneration + 1
	if err = b.session.Send(fmt.Sprintf("SendToModule %s ListLayout picks %d %d 18 %d", b.alias, b.nativeGeneration, next, columns)); err != nil {
		return err
	}
	tag, err := b.requestNativeSnapshot("layout")
	if err != nil {
		return err
	}
	packet, observed, err := b.waitNativeGeneration(tag, next, 0, b.nativeGeneration)
	if err != nil {
		return err
	}
	if err = b.model.SelectNative(b.model.Generation, packet); err != nil {
		return err
	}
	if observed == b.nativeGeneration {
		b.view = b.renderView
		b.status = refused
		return nil
	}
	// This changes only viewport geometry: do not replay ListFocus, entries or
	// input controls. Native range/scroll anchors and unsent drafts stay owned
	// by the existing Form, including an intentionally offscreen focus.
	b.nativeGeneration, b.renderView = next, mode
	if b.status == refused {
		b.status = "View changed to " + mode + "."
	}
	return nil
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
	if err := b.updateLayout(); err != nil {
		return err
	}
	labels := []nativeLabel{{"path-label", b.currentPath()}, {"status", b.status}, {"watcher", b.watchText()}, {"projection", b.projectionText()}, {"counts", b.countText()}, {"progress", b.progressText()}, {"readiness", b.readinessText()}}
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
func (b *browser) show() (err error) {
	if err = b.ensureBindings(); err != nil {
		return err
	}
	if err = b.projectionBinding.Observe("model", viewbinding.Version{
		Producer: b.bindingProducer(), Revision: b.providerGeneration()}); err != nil {
		return err
	}
	// Explicit native resnapshot: selection/focus can change locally even when
	// collection contents compare equal. Keep the existing ACK transaction.
	if err = b.projectionBinding.Invalidate(); err != nil {
		return err
	}
	ticket, ok := b.projectionBinding.Begin()
	if !ok {
		return errors.New("browser projection is unavailable")
	}
	defer func() {
		if !b.projectionBinding.Complete(ticket, err == nil) && err == nil {
			err = errors.New("browser projection was invalidated")
		}
		if err != nil {
			b.closeBindings()
		}
	}()
	if b.renderSerial == ^uint64(0) {
		return errors.New("render generation exhausted")
	}
	b.renderSerial++
	b.renderDeadline = time.Now().Add(workerLimit)
	// Content and layout publications each consume a generation. Keep the
	// existing explicit rebuild boundary when either would exhaust it.
	exhausted := b.nativeGeneration >= 2147483647 ||
		b.nativeGeneration == 2147483646 && b.renderView != b.requestedView() &&
			(!entriesEqual(b.model.VisibleEntries(), b.viewEntries) || b.replaceSelection)
	if b.alias == "" || b.native == nil || b.renderPath != b.currentPath() || exhausted {
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
	return b.projectionTextMode(b.viewing())
}

func (b *browser) projectionTextMode(mode string) string {
	direction := "ascending"
	if b.reverse {
		direction = "descending"
	}
	return fmt.Sprintf("View: %s; filter: %s; sort: %s %s", mode, b.model.Filter(), b.ordering(), direction)
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

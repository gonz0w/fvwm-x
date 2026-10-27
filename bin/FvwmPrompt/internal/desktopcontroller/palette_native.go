// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/base64"
	"encoding/binary"
	"errors"
	"fmt"
	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"strconv"
	"strings"
	"time"
	"unicode"
)

type paletteNative struct {
	generation, next     uint32
	serial               uint64
	wait, phase          string
	deadline             time.Time
	commands             []string
	ids, nextIDs         []string
	entries, nextEntries []collection.Entry
	dirty                bool
	labels               [4]string
	renewed              time.Time
}

func paletteDisplay(value string, limit int) string {
	var out strings.Builder
	for _, c := range value {
		s := string(c)
		if !unicode.IsPrint(c) || strings.ContainsRune("$%\\\"&", c) {
			s = fmt.Sprintf("[U+%04X]", c)
		}
		if out.Len()+len(s) > limit {
			out.WriteString("...")
			break
		}
		out.WriteString(s)
	}
	return out.String()
}
func palettePacket(value string, generation uint32) ([]byte, error) {
	if !strings.HasPrefix(value, "x") || len(value) > 300 {
		return nil, errors.New("Invalid palette selection")
	}
	data, err := base64.RawURLEncoding.Strict().DecodeString(value[1:])
	if err != nil || len(data) < 6 || binary.BigEndian.Uint32(data) != generation {
		return nil, errors.New("Stale palette view")
	}
	return data[4:], nil
}
func (d *Controller) paletteEmit(command string) {
	if len(command) > 1000 {
		d.fatal = errors.New("Palette command limit exceeded")
		return
	}
	d.emit(command)
}
func (d *Controller) paletteTag(phase string) string {
	p := d.palette
	p.native.serial++
	return fmt.Sprintf("%s_%x_%s", p.token, p.native.serial, phase)
}
func (d *Controller) paletteWait(phase string, published, pending uint32) {
	p := d.palette
	if p == nil {
		return
	}
	tag := d.paletteTag(phase)
	p.native.wait = tag
	p.native.phase = fmt.Sprintf("%s:%d:%d", phase, published, pending)
	p.native.deadline = time.Now().Add(5 * time.Second)
	d.paletteEmit("SendToModule SlatePaletteForm ListStatus picks " + tag + " selection")
}
func (d *Controller) paletteForm() {
	p := d.palette
	p.native.generation = 1
	p.native.wait = d.paletteTag("start")
	p.native.phase = "start:1:0"
	p.native.deadline = time.Now().Add(5 * time.Second)
	callback := "Command SendToModule FvwmDesktop palette-control " + p.token + " "
	lines := []string{`Title "Action palette"`, "Colorset 0", "ItemColorset 12", "CloseCommand SendToModule FvwmDesktop palette-control " + p.token + " close", "Line left", `Text "Find windows, applications, desktops and registered actions"`, "Line left", `Text "Query"`, `InputHex palette_query 32 x`, `Button continue "Filter (F3)" F3`, callback + "filter x$(palette_query:Base64) x$(picks:Base64)", "Line left", `Label windows 64 "Windows: pending"`, "Line left", `Label desktops 64 "Desktops: pending"`, "Line left", `Label applications 64 "Applications: pending"`, "Line left", `Label actions 64 "Actions: pending"`, "Line left", "List picks 64 8", "ListViewport 18 1", "ListVersioned", "ListNotify " + p.native.wait, "Line left", `Button continue "Activate (Enter)" ^M`, callback + "activate x$(palette_query:Base64) x$(picks:Base64)", `Button continue "Refresh (F5)" F5`, callback + "refresh x$(palette_query:Base64) x$(picks:Base64)", `Button continue "Close (Esc)" ^[`, callback + "close", "Line left", `Text "Tab: query/list. Arrows: focus. Space: select. Esc: close."`, `Timeout 3000 "SendToModule FvwmDesktop palette-control ` + p.token + ` close" ""`}
	d.emit("KillModule FvwmForm SlatePaletteForm")
	d.emit("DestroyModuleConfig SlatePaletteForm:*")
	for _, line := range lines {
		d.paletteEmit("*SlatePaletteForm: " + line)
	}
	d.emit("Module FvwmForm SlatePaletteForm")
	p.native.dirty = true
}
func (d *Controller) palettePublish() {
	p := d.palette
	if p == nil {
		return
	}
	p.native.dirty = true
	if p.native.generation == 0 || p.native.wait != "" {
		return
	}
	d.paletteUpdate()
}
func paletteEntriesEqual(a, b []collection.Entry) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}
func (d *Controller) paletteUpdate() {
	p := d.palette
	if p == nil {
		return
	}
	n := &p.native
	entries := p.model.Collection.VisibleEntries()
	n.dirty = false
	for i, provider := range p.model.Providers {
		text := fmt.Sprintf("%s: %s (%d)", []string{"Windows", "Desktops", "Applications", "Actions"}[i], provider.State, len(provider.Rows))
		if provider.Truncated {
			text += "; bounded, more omitted"
		}
		if provider.Reason != "" {
			text += "; " + provider.Reason
		}
		if text != n.labels[i] {
			d.paletteEmit("SendToModule SlatePaletteForm SetLabel " + []string{"windows", "desktops", "applications", "actions"}[i] + " \"" + paletteDisplay(text, 350) + "\"")
			n.labels[i] = text
		}
	}
	if !p.deadline.Equal(n.renewed) {
		d.paletteEmit("SendToModule SlatePaletteForm RenewTimeout 3000")
		n.renewed = p.deadline
	}
	if paletteEntriesEqual(entries, n.entries) {
		return
	}
	if n.generation >= 2147483647 {
		d.closePalette()
		d.notice("Palette view generation exhausted; reopen it")
		return
	}
	n.next = n.generation + 1
	n.nextEntries = entries
	n.nextIDs = nil
	old := map[string]int{}
	for i, id := range n.ids {
		old[id] = i + 1
	}
	n.commands = []string{fmt.Sprintf("SendToModule SlatePaletteForm ListBegin picks %d %d", n.generation, n.next)}
	for _, e := range entries {
		n.nextIDs = append(n.nextIDs, e.ID)
		choice := "keep"
		if old[e.ID] == 0 {
			choice = "off"
			if p.model.Collection.Selected[e.ID] {
				choice = "on"
			}
		}
		n.commands = append(n.commands, fmt.Sprintf("SendToModule SlatePaletteForm ListEntry picks %d %d \"%s\" %s %s", n.next, old[e.ID], paletteDisplay(e.Label, 600), e.Kind, choice))
	}
	d.paletteBatch()
}
func (d *Controller) paletteBatch() {
	p := d.palette
	if p == nil {
		return
	}
	n := &p.native
	if len(n.commands) > 0 {
		end := min(24, len(n.commands))
		for _, c := range n.commands[:end] {
			d.paletteEmit(c)
		}
		n.commands = n.commands[end:]
		d.paletteWait("batch", n.generation, n.next)
		return
	}
	d.paletteEmit(fmt.Sprintf("SendToModule SlatePaletteForm ListCommit picks %d", n.next))
	d.paletteWait("commit", n.next, 0)
}
func (d *Controller) paletteNativeReply(message string) bool {
	p := d.palette
	if p == nil {
		return false
	}
	f := strings.Fields(message)
	if len(f) < 2 || f[0] != "FVWM_FORM_LIST" || f[1] != p.native.wait || p.native.wait == "" {
		return false
	}
	n := &p.native
	phase := strings.Split(n.phase, ":")
	if len(f) != 6 || f[2] != phase[1] || f[3] != phase[2] || f[4] != "0" {
		d.closePalette()
		d.notice("Palette native publication was refused; reopen it")
		return true
	}
	gen, _ := strconv.ParseUint(f[2], 10, 32)
	packet, err := palettePacket(f[5], uint32(gen))
	if err != nil {
		d.closePalette()
		return true
	}
	n.wait = ""
	if phase[0] == "commit" {
		n.generation = n.next
		n.ids = n.nextIDs
		n.entries = n.nextEntries
		n.nextIDs = nil
		n.nextEntries = nil
	}
	if err = p.model.Collection.SelectNativeView(p.model.Collection.Generation, n.ids, packet); err != nil {
		d.closePalette()
		d.notice("Palette selection was refused; reopen it")
		return true
	}
	if phase[0] == "batch" {
		d.paletteBatch()
	} else if n.dirty {
		d.paletteUpdate()
	}
	return true
}

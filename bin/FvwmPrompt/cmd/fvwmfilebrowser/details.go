// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"encoding/binary"
	"encoding/hex"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"time"
	"unicode"
	"unicode/utf8"
)

const (
	detailPartLimit     = 400
	detailSourceLimit   = 128 * 1024
	detailExpandedLimit = 512 * 1024
	detailChunkLimit    = 1024
	detailFieldLimit    = 13
)

type detailField struct {
	name  string
	parts []string
}

// The main loop exclusively owns snapshots and their lifetimes. The reader
// admits only bounded typed messages using immutable process identity.
type detailView struct {
	lifetime, page, lastControl uint64
	alias                       string
	fields                      []detailField
	section, part, chooser      int
	captured                    string
}

type detailCallback struct {
	parent, lifetime, page, ticket uint64
	action, packet                 string
}

func detailToken(value string) (string, int) {
	c, size := utf8.DecodeRuneInString(value)
	if c == utf8.RuneError && size == 1 {
		return fmt.Sprintf("[BYTE+%02X]", value[0]), 1
	}
	if !unicode.IsPrint(c) || c > 126 || strings.ContainsRune("$%\\\"&[]", c) {
		return fmt.Sprintf("[U+%04X]", c), size
	}
	return value[:size], size
}

// Admission precedes expansion/allocation. Parts end on encoding boundaries;
// joining them recovers the entire admitted, unambiguous display representation.
func admitDetailFields(values []nativeLabel) ([]detailField, error) {
	if len(values) == 0 || len(values) > detailFieldLimit {
		return nil, errors.New("Details refused: field count exceeds snapshot limit.")
	}
	source, expanded, chunks := 0, 0, 0
	for _, field := range values {
		if len(field.value) > detailSourceLimit-source {
			return nil, errors.New("Details refused: snapshot exceeds 128 KiB source limit.")
		}
		source += len(field.value)
		used := 0
		chunks++
		for value := field.value; len(value) != 0; {
			token, size := detailToken(value)
			if len(token) > detailExpandedLimit-expanded {
				return nil, errors.New("Details refused: escaped snapshot exceeds 512 KiB limit.")
			}
			expanded += len(token)
			if used+len(token) > detailPartLimit {
				chunks++
				used = 0
			}
			used += len(token)
			value = value[size:]
		}
		if chunks > detailChunkLimit {
			return nil, errors.New("Details refused: snapshot exceeds 1024 parts.")
		}
	}
	fields := make([]detailField, 0, len(values))
	for _, field := range values {
		parts := []string{}
		var part strings.Builder
		for value := field.value; len(value) != 0; {
			token, size := detailToken(value)
			if part.Len()+len(token) > detailPartLimit {
				parts = append(parts, part.String())
				part.Reset()
			}
			part.WriteString(token)
			value = value[size:]
		}
		parts = append(parts, part.String())
		fields = append(fields, detailField{field.name, parts})
	}
	return fields, nil
}

func (b *browser) detailValues() []nativeLabel {
	values := []nativeLabel{{"Path", b.currentPath()}, {"Status", b.feedbackStatus()},
		{"Watcher", b.watchText()}, {"Progress", b.progressText()},
		{"Readiness", b.readinessText()}, {"Counts", b.countText()},
		{"Projection", b.projectionText()}}
	for i, label := range b.inspectorLabels() {
		name := "Inspector"
		if i > 0 {
			name = fmt.Sprintf("Detail %d", i)
		}
		// The ordinary single-name label is shortened by the file adapter.
		// Take original filename bytes for this complete snapshot instead.
		if i == 1 && len(b.inspected) == 1 && b.snapshot != nil {
			for _, item := range b.snapshot.Items {
				if item.ID == b.inspected[0] {
					label.value = "Observed name: " + string(item.Name)
					break
				}
			}
		}
		values = append(values, nativeLabel{name, label.value})
	}
	if b.snapshot != nil {
		targets := []struct{ name, id string }{
			{"Focused name", b.model.Focus},
			{"Inspected name", singleInspected(b.inspected)},
		}
		for _, target := range targets {
			if target.id == "" {
				continue
			}
			for _, item := range b.snapshot.Items {
				if item.ID == target.id {
					values = append(values, nativeLabel{target.name, string(item.Name)})
					break
				}
			}
		}
	}
	return values
}

func singleInspected(ids []string) string {
	if len(ids) == 1 {
		return ids[0]
	}
	return ""
}

func canonicalDecimal(value string) (uint64, bool) {
	n, err := strconv.ParseUint(value, 10, 64)
	return n, err == nil && n != 0 && strconv.FormatUint(n, 10) == value
}
func invocationTicket(value string) (uint64, bool) {
	n, err := strconv.ParseUint(value, 16, 64)
	return n, err == nil && n != 0 && len(value) == 16 && fmt.Sprintf("%016x", n) == value
}

func parseDetailCallback(message, instance string) (detailCallback, bool) {
	if len(message) > 800 {
		return detailCallback{}, false
	}
	p := strings.Fields(message)
	if len(p) != 8 || p[0] != "FVWM_FILES_DETAILS_1" || p[1] != instance {
		return detailCallback{}, false
	}
	parent, ok := canonicalDecimal(p[2])
	if !ok {
		return detailCallback{}, false
	}
	life, ok := canonicalDecimal(p[3])
	if !ok {
		return detailCallback{}, false
	}
	page, ok := canonicalDecimal(p[4])
	if !ok {
		return detailCallback{}, false
	}
	callback := detailCallback{parent: parent, lifetime: life, page: page, action: p[5], packet: p[6]}
	if p[5] == "closed" && p[6] == "-" && p[7] == "-" {
		return callback, true
	}
	switch p[5] {
	case "read", "previous", "next", "close":
	default:
		return detailCallback{}, false
	}
	ticket, ok := invocationTicket(p[7])
	if !ok || len(p[6]) > 64 || !strings.HasPrefix(p[6], "x") {
		return detailCallback{}, false
	}
	callback.ticket = ticket
	return callback, true
}

func (b *browser) routeDetailAck(message string) {
	if strings.HasPrefix(message, "FVWM_FILES_DETAILS_SYNC "+b.instance+" ") &&
		b.detailAckReplies != nil {
		b.detailAckReplies.offer(message, message)
	}
}

func (b *browser) routeDetailCallback(message string) error {
	callback, ok := parseDetailCallback(message, b.instance)
	if !ok {
		callback, ok = parseDetailOpen(message, b.instance)
	}
	if ok {
		select {
		case b.detailEvents <- callback:
		default:
		}
	}
	return nil
}

// A chooser packet has generation 1, one focus index, and exactly enough
// selection bits for its immutable rows. Selection never chooses a field.
func detailSection(value string, rows int) (int, error) {
	if rows < 1 || rows > detailFieldLimit {
		return 0, errors.New("invalid detail rows")
	}
	data, err := nativePacket(value, 1)
	if err != nil || len(data) != 2+(rows+7)/8 {
		return 0, errors.New("invalid detail section packet")
	}
	focus := int(binary.BigEndian.Uint16(data))
	if focus >= rows {
		return 0, errors.New("invalid detail section focus")
	}
	if rows%8 != 0 && data[len(data)-1]>>uint(rows%8) != 0 {
		return 0, errors.New("invalid detail section padding")
	}
	return focus, nil
}

func (b *browser) sendDetailCommands(commands []string) error {
	if b.detailSendSerial == ^uint64(0) {
		return errors.New("details send generation exhausted")
	}
	b.detailSendSerial++
	return b.sendCommandBatch(commands, "FVWM_FILES_DETAILS_SYNC",
		b.detailSendSerial, nil, b.detailAckReplies, time.Now().Add(workerLimit))
}

func (b *browser) retireDetails() error {
	if b.details == nil {
		return nil
	}
	alias := b.details.alias
	b.details = nil
	if alias == "" {
		return nil
	}
	return b.sendDetailCommands([]string{
		"KillModule FvwmForm " + alias, "DestroyModuleConfig " + alias + ":*",
	})
}

func (b *browser) openDetails() error {
	if b.detailSerial == ^uint64(0) {
		return errors.New("details lifetime exhausted")
	}
	values := b.detailValues()
	fields, err := admitDetailFields(values)
	if err != nil {
		fields = []detailField{{"Snapshot refused", []string{err.Error()}}}
	}
	if err := b.retireDetails(); err != nil {
		return err
	}
	b.detailSerial++
	b.details = &detailView{lifetime: b.detailSerial, fields: fields,
		captured: fmt.Sprintf("Snapshot: form %d; model %d; native %d.",
			b.form, b.model.Generation, b.nativeGeneration),
	}
	return b.renderDetails()
}

func (b *browser) renderDetails() error {
	d := b.details
	if d.page == ^uint64(0) {
		return errors.New("details page exhausted")
	}
	oldAlias := d.alias
	d.page++
	d.lastControl = 0
	d.alias = fmt.Sprintf("FvwmFiles-%s-details-%d-%d", b.instance, d.lifetime, d.page)
	callback := fmt.Sprintf("Echo FVWM_FILES_DETAILS_1 %s %d %d %d ", b.instance, b.form, d.lifetime, d.page)
	commands := []string{}
	if oldAlias != "" {
		commands = append(commands, "KillModule FvwmForm "+oldAlias,
			"DestroyModuleConfig "+oldAlias+":*")
	}
	line := func(value string) { commands = append(commands, "*"+d.alias+": "+value) }
	line("UIScale monitor")
	line(`Title "File browser details"`)
	line("Line left")
	line(`Text "Read-only snapshot; reopen F1 for current values."`)
	line("Line left")
	line(`Text "ASCII escapes: [U+XXXX]; invalid bytes: [BYTE+XX]."`)
	line("Line left")
	line(`Text "` + d.captured + `"`)
	line("Line left")
	line("List sections 40 4")
	line("ListViewport 18 1")
	line("ListVersioned")
	for _, field := range d.fields {
		line(`ListItem off "` + field.name + `"`)
	}
	line(fmt.Sprintf("ListFocus %d", d.chooser+1))
	button := func(label, key, action string) {
		line(`Button continue "` + label + `" ` + key)
		line("Command " + callback + action + " x$(sections:Base64) $(FormJob:Token)")
	}
	line("Line left")
	button("Read (Enter)", "^M", "read")
	line("Line left")
	field := d.fields[d.section]
	line(fmt.Sprintf(`Text "Field: %s; Part %d/%d"`, field.name, d.part+1, len(field.parts)))
	line("Line left")
	line("InputHex value 40 x" + hex.EncodeToString([]byte(field.parts[d.part])))
	line("InputReadonly value")
	line("Line left")
	button("Previous part (F9)", "F9", "previous")
	button("Next part (F10)", "F10", "next")
	button("Close (Esc)", "^[", "close")
	line("Line left")
	line(`Text "Home/End and Left/Right scroll the value. Tab: fields."`)
	line("Line left")
	line(`Text "Frozen until reopened; lease and parent idle: 5 minutes."`)
	line("CloseCommand " + callback + "closed - -")
	// The native orphan limit is independent of callbacks/parent responsiveness.
	line(`Timeout 3000 "KillModule FvwmForm ` + d.alias + `" ""`)
	commands = append(commands, "Module FvwmForm "+d.alias)
	return b.sendDetailCommands(commands)
}

func (b *browser) detailAction(callback detailCallback) (bool, error) {
	d := b.details
	if d == nil || callback.parent != b.form ||
		callback.lifetime != d.lifetime || callback.page != d.page {
		return false, nil
	}
	if callback.action == "closed" {
		return true, b.retireDetails()
	}
	if callback.ticket == 0 || callback.ticket <= d.lastControl {
		return false, nil
	}
	section, err := detailSection(callback.packet, len(d.fields))
	if err != nil {
		return false, nil
	}
	switch callback.action {
	case "read", "previous", "next", "close":
	default:
		return false, nil
	}
	d.lastControl = callback.ticket
	if callback.action == "close" {
		return true, b.retireDetails()
	}
	if callback.action == "read" {
		d.section, d.part = section, 0
	} else {
		// The chooser can move independently; paging keeps the displayed field.
		if callback.action == "previous" && d.part > 0 {
			d.part--
		}
		if callback.action == "next" && d.part+1 < len(d.fields[d.section].parts) {
			d.part++
		}
	}
	return true, b.renderDetailsFocus(section)
}

// Keep chooser focus even if a user pages the displayed field after moving the
// chooser. This is separate from the field whose value is currently displayed.
func (b *browser) renderDetailsFocus(focus int) error {
	b.details.chooser = focus
	return b.renderDetails()
}

func parseDetailOpen(message, instance string) (detailCallback, bool) {
	if len(message) > 128 {
		return detailCallback{}, false
	}
	p := strings.Fields(message)
	if len(p) != 4 || p[0] != "FVWM_FILES_DETAILS_OPEN_1" || p[1] != instance {
		return detailCallback{}, false
	}
	parent, ok := canonicalDecimal(p[2])
	if !ok {
		return detailCallback{}, false
	}
	ticket, ok := invocationTicket(p[3])
	if !ok {
		return detailCallback{}, false
	}
	return detailCallback{parent: parent, action: "open", ticket: ticket}, true
}

func (b *browser) acceptDetailOpen(callback detailCallback) (bool, error) {
	if callback.parent != b.form || callback.ticket == 0 || callback.ticket <= b.lastControl {
		return false, nil
	}
	b.lastControl = callback.ticket
	return true, b.openDetails()
}

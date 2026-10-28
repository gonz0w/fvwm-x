// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/internal/pinboard"
)

type formView struct {
	workText                                string
	serial, revision, last, modelGeneration uint64
	alias, mode, epoch, id                  string
	offset                                  int
	pageIDs                                 []string
}

func safeText(value string, limit int) string {
	var out strings.Builder
	for len(value) > 0 {
		r, size := utf8.DecodeRuneInString(value)
		text := value[:size]
		if r == utf8.RuneError && size == 1 {
			text = fmt.Sprintf("[BYTE+%02X]", value[0])
		} else if r < 32 || r > 126 || strings.ContainsRune("$%\\\"&", r) {
			text = fmt.Sprintf("[U+%04X]", r)
		}
		if out.Len()+len(text) > limit {
			out.WriteString("...")
			break
		}
		out.WriteString(text)
		value = value[size:]
	}
	return out.String()
}

func (b *board) workText() string {
	if b.active != nil {
		return "Worker active: " + b.active.request.Operation
	}
	if b.cleanupBlocked {
		return "Worker idle; image cleanup unavailable"
	}
	if len(b.obsolete) > 0 {
		return "Worker idle; image retirement pending"
	}
	if b.icons != nil && b.ready {
		return "Worker idle; icon acquisition pending"
	}
	return "Worker idle; no pending work"
}
func (b *board) updateWorkLabel() {
	if b.form == nil {
		return
	}
	text := b.workText()
	if text == b.form.workText {
		return
	}
	b.form.workText = text
	_ = b.session.Send("SendToModule " + b.form.alias + ` SetLabel work "` + text + `"`)
}

func (b *board) feedback(message string) {
	b.status = message
	if b.form != nil {
		_ = b.session.Send("SendToModule " + b.form.alias + " SetLabel status \"" + safeText(message, 160) + "\"")
	}
}
func (b *board) closeForm() error {
	if b.form == nil {
		return nil
	}
	alias := b.form.alias
	b.form = nil
	if err := b.session.Send("KillModule FvwmForm " + alias); err != nil {
		return err
	}
	return b.session.Send("DestroyModuleConfig " + alias + ":*")
}

func (b *board) newForm(mode, id, title string) (*formView, []string, error) {
	if b.formSerial == ^uint64(0) {
		return nil, nil, errors.New("Form generation exhausted")
	}
	if err := b.closeForm(); err != nil {
		return nil, nil, err
	}
	b.formSerial++
	f := &formView{serial: b.formSerial, revision: b.state.Revision, alias: fmt.Sprintf("FvwmPins-%s-%x", b.protocol.Token, b.formSerial), mode: mode, epoch: b.epoch, id: id, modelGeneration: b.model.Generation}
	f.workText = b.workText()
	lines := []string{"UIScale monitor", `Title "` + title + `"`, `Line left`, `Label status 50 "` + safeText(b.status, 160) + `"`, `LabelViewport status`, `Line left`, `Label work 50 "` + f.workText + `"`, `LabelViewport work`, `Line left`}
	return f, lines, nil
}

func (b *board) formCallback(f *formView) string {
	return fmt.Sprintf("Echo FVWM_PINBOARD_FORM %s %d %d %s $(FormJob:Token) ", b.protocol.Token, f.serial, f.revision, f.epoch)
}
func (b *board) finishForm(f *formView, lines []string) error {
	lines = append(lines, "CloseCommand "+b.formCallback(f)+"dialog-close -")
	// The board lease and native lifetime are independent. An abandoned Form
	// has its own bounded timeout and cannot start another owner.
	lines = append(lines, `Timeout 3000 "KillModule FvwmForm `+f.alias+`" ""`)
	commands := make([]string, 0, len(lines)+1)
	for _, line := range lines {
		commands = append(commands, "*"+f.alias+": "+line)
	}
	commands = append(commands, "Module FvwmForm "+f.alias)
	if err := b.configure(commands); err != nil {
		return err
	}
	b.form = f
	return nil
}
func formButton(lines *[]string, callback, label, key, verb, data string) {
	*lines = append(*lines, `Button continue "`+label+`" `+key, "Command "+callback+verb+" "+data)
}

func (b *board) showManager(offset int) error {
	if offset < 0 || offset%collection.PageSize != 0 || offset >= len(b.state.Pins) {
		offset = 0
	}
	page, err := b.model.Page(offset)
	if err != nil {
		return err
	}
	f, lines, err := b.newForm("manager", "", "Pinboard manager")
	if err != nil {
		return err
	}
	f.offset = offset
	lines = append(lines, fmt.Sprintf(`Text "References %d-%d of %d; unpin never changes a target."`, minInt(offset+1, len(b.state.Pins)), offset+len(page), len(b.state.Pins)), `Line left`, `List picks 50 8`, `ListViewport 8 1`, `ListVersioned`)
	focus := 0
	for i, entry := range page {
		f.pageIDs = append(f.pageIDs, entry.ID)
		target := b.observed[entry.ID]
		kind := target.Kind
		if target.Status != "present" {
			kind = target.Status
		}
		if kind == "" {
			kind = "pending"
		}
		lines = append(lines, fmt.Sprintf(`ListItem off "%s [%s] %s"`, safeText(entry.Label, 100), kind, entry.ID[:8]))
		if entry.ID == b.model.Focus {
			focus = i + 1
		}
	}
	if focus > 0 {
		lines = append(lines, fmt.Sprintf("ListFocus %d", focus))
	}
	cb := b.formCallback(f)
	packet := "x$(picks:Base64)"
	lines = append(lines, "Line left")
	formButton(&lines, cb, "Open (F3)", "F3", "open", packet)
	formButton(&lines, cb, "Edit (F2)", "F2", "edit", packet)
	formButton(&lines, cb, "Move (F4)", "F4", "move-dialog", packet)
	formButton(&lines, cb, "Unpin (Alt-U)", "Alt-U", "unpin", packet)
	lines = append(lines, "Line left")
	formButton(&lines, cb, "New (Alt-N)", "Alt-N", "new", packet)
	formButton(&lines, cb, "Refresh (F5)", "F5", "refresh", packet)
	formButton(&lines, cb, "Reload (F6)", "F6", "reload", packet)
	lines = append(lines, "Line left")
	formButton(&lines, cb, "Previous (F9)", "F9", "previous", packet)
	formButton(&lines, cb, "Next (F10)", "F10", "next", packet)
	formButton(&lines, cb, "Cancel work (Alt-X)", "Alt-X", "cancel-work", packet)
	lines = append(lines, "Line left")
	formButton(&lines, cb, "Close dialog (Esc)", "^[", "dialog-close", "-")
	formButton(&lines, cb, "Close pinboard (Alt-Q)", "Alt-Q", "close", "-")
	lines = append(lines, `Line left`, `Text "Arrows/Space: focus/select. Tab: controls. All pins remain in this list."`)
	return b.finishForm(f, lines)
}
func minInt(a, b int) int {
	if a < b {
		return a
	}
	return b
}

func (b *board) showNew() error {
	f, lines, err := b.newForm("new", "", "New pin")
	if err != nil {
		return err
	}
	lines = append(lines, `Text "Path: one absolute local path or local file URI (384 input bytes)."`, `Line left`, `Input path 42 ""`, `Line left`, `Text "Longer paths: fvwm-pinboard --pin PATH (data only), then Reload."`, `Line left`)
	cb := b.formCallback(f)
	formButton(&lines, cb, "Save (Enter)", "^M", "create", "x$(path:Base64)")
	formButton(&lines, cb, "Cancel (Esc)", "^[", "dialog-close", "-")
	return b.finishForm(f, lines)
}
func (b *board) showEdit(id string, move bool) error {
	i := b.state.Index(id)
	if i < 0 {
		return errors.New("select a current pin")
	}
	p := b.state.Pins[i]
	mode, title := "edit", "Edit pin"
	if move {
		mode, title = "move", "Move pin"
	}
	f, lines, err := b.newForm(mode, id, title)
	if err != nil {
		return err
	}
	lines = append(lines, `Text "Target (display may be shortened): `+safeText(string(p.Target), 120)+`"`, `Line left`)
	cb := b.formCallback(f)
	if move {
		lines = append(lines, `Text "Preferred monitor (empty: automatic):"`, `Line left`, "InputHex monitor 36 x"+hex.EncodeToString(p.Monitor), `Line left`, `Text "Grid column:"`, `Input column 12 "`+strconv.Itoa(p.Column)+`"`, `Text "Row:"`, `Input row 12 "`+strconv.Itoa(p.Row)+`"`, `Line left`)
		formButton(&lines, cb, "Save (Enter)", "^M", "move", "x$(monitor:Base64) x$(column:Base64) x$(row:Base64)")
	} else {
		lines = append(lines, `Text "Label (256 UTF-8 bytes):"`, `Line left`, "InputHex label 42 x"+hex.EncodeToString([]byte(p.Label)), `Line left`)
		formButton(&lines, cb, "Save label (Alt-L)", "Alt-L", "label", "x$(label:Base64)")
		lines = append(lines, `Line left`, `Text "Named icon override (empty: type fallback):"`, `Line left`, "InputHex icon 42 x"+hex.EncodeToString([]byte(p.Icon)), `Line left`)
		formButton(&lines, cb, "Save icon (Alt-I)", "Alt-I", "icon", "x$(icon:Base64)")
	}
	formButton(&lines, cb, "Cancel (Esc)", "^[", "dialog-close", "-")
	return b.finishForm(f, lines)
}

func formData(value string, limit int) ([]byte, error) {
	if !strings.HasPrefix(value, "x") || len(value) > 1+(limit*4+2)/3 {
		return nil, errors.New("Input exceeds its limit; longer targets require fvwm-pinboard --pin PATH, then Reload")
	}
	data, err := base64.RawURLEncoding.Strict().DecodeString(value[1:])
	if err != nil || len(data) > limit {
		return nil, errors.New("Invalid or over-limit literal input; no change saved")
	}
	return data, nil
}
func (b *board) managerFocus(value string) error {
	packet, err := formData(value, 518)
	if err != nil {
		return err
	}
	if len(packet) < 6 || binary.BigEndian.Uint32(packet) != 1 {
		return errors.New("stale manager list")
	}
	return b.model.SelectNativeView(b.form.modelGeneration, b.form.pageIDs, packet[4:])
}

func (b *board) control(message string) (bool, error) {
	fields := strings.Fields(message)
	if len(fields) > 0 && fields[0] == "FVWM_PINBOARD_CONTROL" {
		return b.contextControl(fields)
	}
	if len(fields) < 8 || fields[0] != "FVWM_PINBOARD_FORM" || fields[1] != b.protocol.Token {
		return false, errors.New("malformed Form control")
	}
	f := b.form
	if f == nil {
		return false, nil
	}
	serial, e1 := pinboard.Decimal(fields[2], ^uint64(0))
	revision, e2 := pinboard.Decimal(fields[3], ^uint64(0))
	if e1 != nil || e2 != nil || serial != f.serial {
		return false, nil
	}
	if fields[6] == "dialog-close" {
		return false, b.closeForm()
	}
	if revision != b.state.Revision || revision != f.revision || fields[4] != f.epoch || f.epoch != b.epoch {
		return false, errors.New("This dialog is stale; reopen the manager. No change saved")
	}
	ticket, err := strconv.ParseUint(fields[5], 16, 64)
	if err != nil || ticket == 0 || !pinboard.Hex(fields[5], 16) || ticket <= f.last {
		return false, errors.New("stale Form invocation")
	}
	f.last = ticket
	action := fields[6]
	if action == "close" {
		return true, nil
	}
	if action == "cancel-work" {
		b.cancelWork()
		return false, nil
	}
	if f.mode == "manager" {
		if len(fields) != 8 {
			return false, errors.New("invalid manager fields")
		}
		if err := b.managerFocus(fields[7]); err != nil {
			return false, err
		}
		id := b.model.Focus
		switch action {
		case "previous":
			return false, b.showManager(maxInt(0, f.offset-collection.PageSize))
		case "next":
			offset := f.offset + collection.PageSize
			if offset >= len(b.state.Pins) {
				offset = f.offset
			}
			return false, b.showManager(offset)
		case "new":
			return false, b.showNew()
		case "open":
			return false, b.open(id)
		case "edit":
			return false, b.showEdit(id, false)
		case "move-dialog":
			return false, b.showEdit(id, true)
		case "unpin":
			return false, b.change("unpin", pinboard.Pin{ID: id})
		case "refresh":
			return false, b.dataWork(workRequest{Operation: "observe", State: b.state})
		case "reload":
			return false, b.dataWork(workRequest{Operation: "load", Path: b.options.state})
		}
	}
	if action == "create" && f.mode == "new" {
		if len(fields) != 8 {
			return false, errors.New("invalid new-pin input")
		}
		data, err := formData(fields[7], 384)
		if err != nil {
			return false, err
		}
		target, err := filemodel.ParseTarget(string(data))
		if err != nil {
			return false, err
		}
		id, err := pinboard.NewID()
		if err != nil {
			return false, err
		}
		return false, b.change("create", pinboard.Pin{ID: id, Target: target, Label: pinboard.DefaultLabel(target)})
	}
	i := b.state.Index(f.id)
	if i < 0 {
		return false, errors.New("pin no longer exists")
	}
	p := b.state.Pins[i]
	if f.mode == "edit" && (action == "label" || action == "icon") {
		if len(fields) != 8 {
			return false, errors.New("invalid metadata input")
		}
		data, err := formData(fields[7], 256)
		if err != nil {
			return false, err
		}
		if action == "label" {
			p.Label = string(data)
		} else {
			p.Icon = string(data)
		}
		return false, b.change("edit", p)
	}
	if f.mode == "move" && action == "move" {
		if len(fields) != 10 {
			return false, errors.New("invalid move input")
		}
		monitor, err := formData(fields[7], 256)
		if err != nil {
			return false, err
		}
		column, err := formData(fields[8], 10)
		if err != nil {
			return false, err
		}
		row, err := formData(fields[9], 10)
		if err != nil {
			return false, err
		}
		c, e1 := pinboard.Decimal(string(column), pinboard.MaxGeneration)
		r, e2 := pinboard.Decimal(string(row), pinboard.MaxGeneration)
		if e1 != nil || e2 != nil {
			return false, errors.New("grid coordinates must be nonnegative strict decimal integers")
		}
		p.Monitor, p.Column, p.Row = monitor, int(c), int(r)
		return false, b.change("move", p)
	}
	return false, errors.New("unsupported Form action")
}
func maxInt(a, b int) int {
	if a > b {
		return a
	}
	return b
}

func (b *board) showContext(id string) error {
	if id != "-" && b.state.Index(id) < 0 {
		return errors.New("stale context pin")
	}
	if b.contextSerial == ^uint64(0) {
		return errors.New("context generation exhausted")
	}
	b.contextSerial++
	commands := []string{}
	if b.contextAlias != "" {
		commands = append(commands, "DestroyMenu "+b.contextAlias)
	}
	b.contextAlias = fmt.Sprintf("FvwmPinsMenu-%s-%x", b.protocol.Token, b.contextSerial)
	b.contextID = id
	commands = append(commands, "DestroyMenu recreate "+b.contextAlias)
	cb := fmt.Sprintf("Echo FVWM_PINBOARD_CONTROL %s %d %d %s ", b.protocol.Token, b.contextSerial, b.view, b.epoch)
	add := func(label, verb string) {
		commands = append(commands, `AddToMenu `+b.contextAlias+` "`+label+`" `+cb+verb+" "+id)
	}
	if id != "-" {
		add("Open", "open")
		add("Edit label/icon", "edit")
		add("Move reference", "move-dialog")
		add("Unpin reference", "unpin")
	}
	add("New pin", "new")
	add("Manage all references", "manage")
	add("Refresh targets", "refresh")
	add("Reload saved data", "reload")
	add("Cancel work", "cancel-work")
	add("Close pinboard", "close")
	if err := b.configure(commands); err != nil {
		return err
	}
	return b.session.Send("Menu " + b.contextAlias)
}

func (b *board) contextControl(fields []string) (bool, error) {
	if len(fields) != 7 || fields[1] != b.protocol.Token {
		return false, errors.New("malformed context control")
	}
	serial, e1 := pinboard.Decimal(fields[2], ^uint64(0))
	view, e2 := pinboard.Decimal(fields[3], pinboard.MaxGeneration)
	if e1 != nil || e2 != nil || serial != b.contextSerial || uint32(view) != b.view || fields[4] != b.epoch || fields[6] != b.contextID {
		return false, errors.New("stale context action")
	}
	// A menu invocation is single-use even if an old callback is replayed.
	b.contextSerial++
	id := fields[6]
	switch fields[5] {
	case "close":
		return true, nil
	case "manage":
		return false, b.showManager(0)
	case "new":
		return false, b.showNew()
	case "edit":
		return false, b.showEdit(id, false)
	case "move-dialog":
		return false, b.showEdit(id, true)
	case "open":
		return false, b.open(id)
	case "unpin":
		return false, b.change("unpin", pinboard.Pin{ID: id})
	case "refresh":
		return false, b.dataWork(workRequest{Operation: "observe", State: b.state})
	case "reload":
		return false, b.dataWork(workRequest{Operation: "load", Path: b.options.state})
	case "cancel-work":
		b.cancelWork()
		return false, nil
	}
	return false, errors.New("unknown context action")
}

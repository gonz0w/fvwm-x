// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package pinboard

import (
	"encoding/hex"
	"errors"
	"fmt"
	"strconv"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/transport"
)

func Field(data []byte) string {
	if len(data) == 0 {
		return "-"
	}
	return hex.EncodeToString(data)
}

func Unfield(text string, max int) ([]byte, error) {
	if text == "-" {
		return nil, nil
	}
	if len(text) == 0 || len(text) > 2*max || len(text)%2 != 0 || !Hex(text, len(text)) {
		return nil, errors.New("invalid hexadecimal byte field")
	}
	return hex.DecodeString(text)
}

func Decimal(text string, max uint64) (uint64, error) {
	n, err := strconv.ParseUint(text, 10, 64)
	if err != nil || n > max || strconv.FormatUint(n, 10) != text {
		return 0, errors.New("invalid strict decimal")
	}
	return n, nil
}

type Protocol struct{ Alias, Token string }

func (p Protocol) Command(verb string, args ...string) (string, error) {
	switch verb {
	case "Owner", "Begin", "Layout", "Entry", "Label", "Picture", "Commit", "Abort", "Status", "Cells", "Renew", "Retire":
	default:
		return "", errors.New("unknown pinboard protocol verb")
	}
	if !Hex(p.Token, 32) || len(p.Alias) > 64 || p.Alias == "" {
		return "", errors.New("invalid pinboard owner")
	}
	for _, c := range p.Alias {
		if !(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '-') {
			return "", errors.New("invalid pinboard alias")
		}
	}
	command := "SendToModule " + p.Alias + " " + verb + " " + p.Token
	for _, arg := range args {
		if arg == "" || strings.ContainsAny(arg, " \t\n\r\x00") {
			return "", errors.New("invalid protocol token")
		}
		command += " " + arg
	}
	if len(command) > transport.MaxCommand {
		return "", errors.New("pinboard command exceeds transport limit")
	}
	return command, nil
}

// Scene returns split label/picture fields; no filesystem target is transmitted.
func (p Protocol) Scene(old, next uint32, s State, missing map[string]bool, pictures map[string]string) ([]string, error) {
	if next == 0 || next > MaxGeneration || next <= old {
		return nil, errors.New("invalid scene generation")
	}
	if err := s.Validate(); err != nil {
		return nil, err
	}
	var commands []string
	add := func(verb string, args ...string) error {
		command, err := p.Command(verb, args...)
		if err == nil {
			commands = append(commands, command)
		}
		return err
	}
	gen := strconv.FormatUint(uint64(next), 10)
	if err := add("Begin", strconv.FormatUint(uint64(old), 10), gen, strconv.Itoa(len(s.Pins))); err != nil {
		return nil, err
	}
	if err := add("Layout", gen, strconv.Itoa(s.Layout.CellWidth), strconv.Itoa(s.Layout.CellHeight), strconv.Itoa(s.Layout.IconSize)); err != nil {
		return nil, err
	}
	for _, pin := range s.Pins {
		absent := "0"
		if missing[pin.ID] {
			absent = "1"
		}
		if err := add("Entry", gen, pin.ID, Field(pin.Monitor), strconv.Itoa(pin.Column), strconv.Itoa(pin.Row), absent); err != nil {
			return nil, err
		}
		if err := add("Label", gen, pin.ID, Field([]byte(pin.Label))); err != nil {
			return nil, err
		}
		if picture := pictures[pin.ID]; picture != "" && len(picture) <= 384 {
			if err := add("Picture", gen, pin.ID, Field([]byte(picture))); err != nil {
				return nil, err
			}
		}
	}
	if err := add("Commit", gen); err != nil {
		return nil, err
	}
	return commands, nil
}

type Status struct {
	Tag                  string
	Current, Pending     uint32
	State, Window, Epoch string
}
type Event struct {
	View            uint32
	Epoch, Verb, ID string
	Monitor         []byte
	Column, Row     int
}
type Cell struct {
	Index         int    `json:"index"`
	ID            string `json:"id"`
	Monitor       []byte `json:"monitor_bytes"`
	Quarters      int    `json:"quarters"`
	Column, Row   int
	X, Y          int
	Width, Height int
	Visible       bool
}
type CellPage struct {
	Tag                  string
	View                 uint32
	Epoch                string
	Offset, Count, Total int
	Cells                []Cell
}

func ParseStatus(message, token string) (Status, error) {
	f := strings.Fields(message)
	var s Status
	if len(f) != 8 || f[0] != "FVWM_PINBOARD_STATUS" || f[1] != token || !Hex(token, 32) || len(f[2]) > 32 {
		return s, errors.New("invalid native status")
	}
	current, e1 := Decimal(f[3], MaxGeneration)
	pending, e2 := Decimal(f[4], MaxGeneration)
	if e1 != nil || e2 != nil || !Hex(f[7], 16) {
		return s, errors.New("invalid status generations")
	}
	switch f[5] {
	case "ready", "pending", "refused", "unavailable", "retired":
	default:
		return s, errors.New("invalid native state")
	}
	if _, err := Decimal(f[6], ^uint64(0)); err != nil {
		return s, err
	}
	// The window is an observed native identity; it is never a target path.
	return Status{f[2], uint32(current), uint32(pending), f[5], f[6], f[7]}, nil
}

func ParseEvent(message, token string) (Event, error) {
	f := strings.Fields(message)
	var event Event
	if len(f) != 9 || f[0] != "FVWM_PINBOARD_EVENT" || f[1] != token || !Hex(token, 32) || !Hex(f[3], 16) {
		return event, errors.New("invalid pinboard event")
	}
	view, err := Decimal(f[2], MaxGeneration)
	if err != nil || view == 0 {
		return event, errors.New("invalid event view")
	}
	switch f[4] {
	case "focus", "open", "context", "move", "manage", "refresh", "close", "layout":
	default:
		return event, errors.New("unknown pinboard event")
	}
	if f[5] != "-" && !Hex(f[5], 32) {
		return event, errors.New("invalid event pin")
	}
	monitor, err := Unfield(f[6], 256)
	if err != nil {
		return event, err
	}
	column, e1 := Decimal(f[7], MaxGeneration)
	row, e2 := Decimal(f[8], MaxGeneration)
	if e1 != nil || e2 != nil {
		return event, errors.New("invalid event placement")
	}
	return Event{uint32(view), f[3], f[4], f[5], monitor, int(column), int(row)}, nil
}

func ParseCell(message, token, tag string, view uint32, epoch string) (Cell, error) {
	f := strings.Fields(message)
	var cell Cell
	if len(f) != 16 || f[0] != "FVWM_PINBOARD_CELL" || f[1] != token || f[2] != tag || f[3] != strconv.FormatUint(uint64(view), 10) || f[4] != epoch || !Hex(f[6], 32) {
		return cell, errors.New("stale or invalid projected cell")
	}
	index, err := Decimal(f[5], MaxPins-1)
	if err != nil {
		return cell, err
	}
	monitor, err := Unfield(f[7], 256)
	if err != nil {
		return cell, err
	}
	numbers := make([]int, 8)
	for i := 0; i < 8; i++ {
		value, err := strconv.ParseInt(f[8+i], 10, 32)
		if err != nil || strconv.FormatInt(value, 10) != f[8+i] {
			return cell, errors.New("invalid cell geometry")
		}
		numbers[i] = int(value)
	}
	if numbers[0] < 4 || numbers[0] > 16 || numbers[1] < 0 || numbers[2] < 0 || numbers[5] < 0 || numbers[6] < 0 || numbers[7] < 0 || numbers[7] > 1 {
		return cell, errors.New("invalid cell projection")
	}
	return Cell{int(index), f[6], monitor, numbers[0], numbers[1], numbers[2], numbers[3], numbers[4], numbers[5], numbers[6], numbers[7] == 1}, nil
}

func ParseCellsEnd(message, token, tag string, view uint32, epoch string) (CellPage, error) {
	f := strings.Fields(message)
	var page CellPage
	if len(f) != 8 || f[0] != "FVWM_PINBOARD_CELLS_END" || f[1] != token || f[2] != tag || f[3] != fmt.Sprint(view) || f[4] != epoch {
		return page, errors.New("stale projected cells")
	}
	o, e1 := Decimal(f[5], MaxPins)
	c, e2 := Decimal(f[6], 8)
	total, e3 := Decimal(f[7], MaxPins)
	if e1 != nil || e2 != nil || e3 != nil || o+c > total {
		return page, errors.New("invalid cell page")
	}
	return CellPage{Tag: tag, View: view, Epoch: epoch, Offset: int(o), Count: int(c), Total: int(total)}, nil
}

// Validate pins one complete native projection page to the committed order.
// A partial, duplicate or misordered page never starts icon acquisition.
func (p CellPage) Validate(s State, offset int) error {
	if p.View == 0 || !Hex(p.Epoch, 16) || p.Epoch == "0000000000000000" ||
		p.Offset != offset || offset < 0 || p.Total != len(s.Pins) || p.Count != len(p.Cells) ||
		p.Count > 8 || offset+p.Count > p.Total || p.Count == 0 && offset < p.Total {
		return errors.New("incomplete or stale native cell page")
	}
	for i, cell := range p.Cells {
		if cell.Index != offset+i || cell.ID != s.Pins[offset+i].ID ||
			cell.Quarters < 4 || cell.Quarters > 16 || len(cell.Monitor) > 256 ||
			cell.Column < 0 || cell.Row < 0 || cell.Width < 0 || cell.Height < 0 ||
			cell.Visible && (cell.Width == 0 || cell.Height == 0) {
			return errors.New("native cell identity/geometry disagrees with committed pins")
		}
	}
	return nil
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
package desktopcontroller

import (
	"encoding/json"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/actionpalette"
	"github.com/fvwmorg/FvwmPrompt/internal/viewbinding"
)

type workspaceFacts struct {
	Desk          int    `json:"desk"`
	PageX         int    `json:"page_x"`
	PageY         int    `json:"page_y"`
	X             int    `json:"x"`
	Y             int    `json:"y"`
	Width         int    `json:"width"`
	Height        int    `json:"height"`
	MonitorOutput string `json:"monitor_output"`
	MonitorRole   string `json:"monitor_role"`
	StickyDesks   bool   `json:"sticky_desks"`
	StickyPages   bool   `json:"sticky_pages"`
	Iconified     bool   `json:"iconified"`
	Fullscreen    bool   `json:"fullscreen"`
	Focused       bool   `json:"focused"`
	PageMaxX      int    `json:"page_max_x"`
	PageMaxY      int    `json:"page_max_y"`
}

func paletteRowKeys(row map[string]any) bool {
	for _, key := range []string{"id", "index", "kind", "label", "detail", "icon", "enabled"} {
		if _, ok := row[key]; !ok {
			return false
		}
	}
	for key := range row {
		if !oneOf(key, "id", "index", "kind", "label", "detail", "icon", "enabled", "desk", "page_x", "page_y", "x", "y", "width", "height", "monitor_output", "monitor_role", "sticky_desks", "sticky_pages", "iconified", "fullscreen", "focused", "page_max_x", "page_max_y") {
			return false
		}
		if oneOf(key, "desk", "page_x", "page_y", "x", "y", "width", "height", "page_max_x", "page_max_y") {
			number, ok := row[key].(json.Number)
			if !ok {
				return false
			}
			value, err := number.Int64()
			if err != nil || value < -2147483648 || value > 2147483647 {
				return false
			}
		}
		if oneOf(key, "sticky_desks", "sticky_pages", "iconified", "fullscreen", "focused") {
			if _, ok := row[key].(bool); !ok {
				return false
			}
		}
		if oneOf(key, "monitor_output", "monitor_role") {
			if _, ok := row[key].(string); !ok {
				return false
			}
		}
	}
	return true
}
func overviewRowFacts(row map[string]any) bool {
	keys := []string{"desk", "page_max_x", "page_max_y"}
	if row["kind"] == string(actionpalette.Window) {
		keys = []string{"desk", "page_x", "page_y", "x", "y", "width", "height", "monitor_output", "monitor_role", "sticky_desks", "sticky_pages", "iconified", "fullscreen", "focused"}
	}
	for _, key := range keys {
		if _, ok := row[key]; !ok {
			return false
		}
	}
	return true
}
func (f workspaceFacts) valid(kind actionpalette.Kind) bool {
	if kind == actionpalette.Desktop {
		return f.PageMaxX >= 0 && f.PageMaxY >= 0 && f.PageMaxX <= 2147483647 && f.PageMaxY <= 2147483647
	}
	if f.Width < 1 || f.Height < 1 || f.Width > 2147483647 || f.Height > 2147483647 || f.X < -2147483648 || f.X > 2147483647 || f.Y < -2147483648 || f.Y > 2147483647 || f.PageX < -2147483648 || f.PageX > 2147483647 || f.PageY < -2147483648 || f.PageY > 2147483647 {
		return false
	}
	for _, identity := range []string{f.MonitorOutput, f.MonitorRole} {
		n, err := strconv.ParseUint(identity, 10, 64)
		if err != nil || strconv.FormatUint(n, 10) != identity {
			return false
		}
	}
	return true
}
func (f workspaceFacts) detail(kind actionpalette.Kind) string {
	if kind == actionpalette.Desktop {
		return fmt.Sprintf("desk%d; pages0..%d,0..%d", f.Desk, f.PageMaxX, f.PageMaxY)
	}
	flags := []string{}
	for _, flag := range []struct {
		yes  bool
		name string
	}{{f.StickyDesks, "sticky-desks"}, {f.StickyPages, "sticky-pages"}, {f.Iconified, "iconified"}, {f.Fullscreen, "fullscreen"}, {f.Focused, "focused"}} {
		if flag.yes {
			flags = append(flags, flag.name)
		}
	}
	text := fmt.Sprintf("desk%d page%d,%d %dx%d%+d%+d output%s role%s %s", f.Desk, f.PageX, f.PageY, f.Width, f.Height, f.X, f.Y, f.MonitorOutput, f.MonitorRole, strings.Join(flags, ","))
	return text[:min(160, len(text))]
}
func (d *Controller) workspaceMasks() {
	extended := coreReply | configSourceMessage
	regular := regularDesktopMask
	if d.palette != nil {
		extended |= nativeEcho
	}
	if d.palette != nil || len(d.scratchpads.definitions) > 0 {
		regular |= paletteEventMask
	}
	d.emit(fmt.Sprintf("SET_MASK %d", extended))
	d.emit(fmt.Sprintf("SET_MASK %d", regular))
}
func (d *Controller) overviewAction(verb string, row actionpalette.Row, arguments []string) error {
	p := d.palette
	if p == nil || !p.overview {
		return errors.New("Overview is not open")
	}
	if p.eventDirty || p.pending != nil || p.binding.State() != viewbinding.Current || row.Version != p.coreGeneration {
		return errors.New("Workspace selection is stale; Refresh first")
	}
	if verb == "mark" {
		if row.Kind != actionpalette.Window {
			return errors.New("Select a window to mark")
		}
		marked := row
		p.marked = &marked
		d.paletteEmit("SendToModule SlatePaletteForm SetLabel marked " + quote("Marked: "+paletteDisplay(row.Label, 120)))
		return d.fatal
	}
	source := row
	suffix := "drag"
	if verb == "move" {
		if row.Kind != actionpalette.Desktop || p.marked == nil || p.marked.Version != p.coreGeneration || len(arguments) != 2 {
			return errors.New("Mark a current window, then select a workspace and page")
		}
		x, ex := strconv.Atoi(arguments[0])
		y, ey := strconv.Atoi(arguments[1])
		bounds := p.facts[row.ID]
		if ex != nil || ey != nil || strconv.Itoa(x) != arguments[0] || strconv.Itoa(y) != arguments[1] || x < 0 || y < 0 || x > bounds.PageMaxX || y > bounds.PageMaxY {
			return errors.New("Destination page is outside the observed workspace")
		}
		source = *p.marked
		suffix = fmt.Sprintf("move %d %d %d", row.Index, x, y)
	} else if verb != "drag" || row.Kind != actionpalette.Window {
		return errors.New("Select a window for pointer Move")
	}
	if p.serial == ^uint64(0) {
		return errors.New("Overview request identity exhausted; reopen it")
	}
	p.serial++
	serial := fmt.Sprintf("%016x", p.serial)
	p.pending = &paletteQuery{serial: serial, phase: "activate", target: source.ID, action: verb, deadline: time.Now().Add(5 * time.Second)}
	d.paletteDisarmFocus()
	d.paletteEmit(fmt.Sprintf("Send_Palette %s %s %s %d %s", p.cookie, serial, p.coreGeneration, source.Index, suffix))
	return d.fatal
}

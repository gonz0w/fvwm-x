// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"reflect"
	"strconv"
	"strings"
	"unicode"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopinspect"
)

// Only the event loop owns these records. A running job retains its original
// view identity even when another view replaces it. Refresh coalesces to one
// pending request without cancelling or reusing the worker's ownership record.
type inspection struct {
	token, frame, scope, filter, status string
	page                                int
	result                              *desktopinspect.Result
	pending                             bool
}
type inspectionWork struct {
	view     string
	request  desktopinspect.Request
	themeKey []any
}

var inspectionScopes = []string{"all", "settings", "theme", "bindings", "layout", "sources", "commands"}

func inspectionScope(scope string) bool {
	for _, known := range inspectionScopes {
		if scope == known {
			return true
		}
	}
	return false
}
func inspectionFilter(filter string) bool {
	if len(filter) > 128 || !utf8.ValidString(filter) {
		return false
	}
	for _, c := range filter {
		if unicode.IsControl(c) {
			return false
		}
	}
	return true
}
func (d *Controller) inspectionRequest(scope, filter string) (desktopinspect.Request, error) {
	settings, err := d.query("all", nil, nil)
	if err != nil {
		return desktopinspect.Request{}, err
	}
	data, err := json.Marshal(settings)
	if err != nil {
		return desktopinspect.Request{}, err
	}
	source, err := d.literalSourceRequest()
	if err != nil {
		return desktopinspect.Request{}, err
	}
	colors := make(map[string]string, len(d.lastThemeColors))
	for name, color := range d.lastThemeColors {
		colors[name] = color
	}
	request := desktopinspect.Request{Scope: scope, Filter: filter, Provider: d.instance,
		SettingsGeneration: d.generation, Settings: data, Appearance: d.lastThemeAppearance,
		Colors: colors, Source: source}
	encoded, err := json.Marshal(request)
	if err != nil {
		return desktopinspect.Request{}, err
	}
	// Apply the same strict bounds before admitting work as the worker itself.
	return desktopinspect.DecodeRequest(encoded)
}
func (d *Controller) startInspection(scope string) error {
	if scope == "" {
		scope = "all"
	}
	if !inspectionScope(scope) {
		return errors.New("inspect expects all, settings, theme, bindings, layout, sources or commands")
	}
	if _, busy := d.jobs["inspection"]; !busy && len(d.jobs) >= 4 {
		return errors.New("Desktop service is busy; try the inspector again")
	}
	if !d.inspectionFocusConfigured {
		// Only this viewer's Form alias adopts the WM's ordinary focus
		// grab/release policy. Set it once so paging retains focus without
		// repeatedly overriding any later user-defined window policy.
		d.emit("Style SlateInspectorForm FPGrabFocus, FPReleaseFocus")
		d.inspectionFocusConfigured = true
	}
	d.inspection = &inspection{token: newID(16), scope: scope}
	return d.refreshInspection()
}
func (d *Controller) closeInspection() {
	d.inspection = nil
	if d.cancelQueued("inspection") {
		d.inspectionJob = nil
	}
	d.emit("KillModule FvwmForm SlateInspectorForm")
}
func (d *Controller) staleInspection(reason string) {
	if view := d.inspection; view != nil {
		view.status = reason
		d.showInspection()
	}
}
func (d *Controller) refreshInspection() error {
	view := d.inspection
	if view == nil {
		return nil
	}
	view.status = "Collecting; domains are separate point-in-time observations."
	if _, busy := d.jobs["inspection"]; busy {
		view.pending = true
		return d.showInspection()
	}
	request, err := d.inspectionRequest(view.scope, view.filter)
	if err != nil {
		view.status = "Refresh failed; previous snapshot retained."
		d.showInspection()
		return err
	}
	if !d.workerJob("inspection", "inspect", values{"inspect": request}) {
		view.status = "Service busy; previous snapshot retained. Press F5 to retry."
		return d.showInspection()
	}
	view.pending = false
	d.inspectionJob = &inspectionWork{view: view.token, request: request,
		themeKey: append([]any(nil), d.lastTheme...)}
	return d.showInspection()
}
func (d *Controller) finishInspection(completed job) {
	work := d.inspectionJob
	d.inspectionJob = nil
	view := d.inspection
	if work == nil || view == nil {
		return
	}
	if work.view == view.token {
		var result desktopinspect.Result
		err := completed.err
		if err == nil {
			data, marshalErr := json.Marshal(completed.result)
			err = marshalErr
			if err == nil {
				result, err = desktopinspect.DecodeResult(data)
			}
		}
		if err == nil && (result.Provider != work.request.Provider ||
			result.SettingsGeneration != work.request.SettingsGeneration ||
			result.Scope != work.request.Scope || result.Filter != work.request.Filter) {
			err = errors.New("Inspector result identity does not match its request")
		}
		if err != nil {
			// Worker/parser errors may contain source paths or provider text. The
			// native failure message is deliberately fixed and keeps the old data.
			view.status = "Refresh failed; previous snapshot is stale. Press F5 to retry."
		} else {
			view.result = &result
			view.page = min(view.page, inspectionPages(&result)-1)
			view.status = "Separate point-in-time observations; no atomic desktop snapshot."
			if work.request.Provider != d.instance || work.request.SettingsGeneration != d.generation || !reflect.DeepEqual(work.themeKey, d.lastTheme) {
				view.status = "Stale: settings or theme policy changed during collection. Press F5."
			}
		}
	}
	if view.pending {
		_ = d.refreshInspection()
	} else if work.view == view.token {
		_ = d.showInspection()
	}
}

// Two rows per page leave room for bounded 200-rune values at enlarged fonts.
// Section boundaries always start a new page.
const inspectionRowsPerPage = 2

type inspectionPage struct {
	section desktopinspect.Section
	rows    []desktopinspect.Row
}

func inspectionPageList(result *desktopinspect.Result) []inspectionPage {
	pages := []inspectionPage{}
	if result != nil {
		for _, section := range result.Sections {
			for i := 0; i < max(1, len(section.Rows)); i += inspectionRowsPerPage {
				pages = append(pages, inspectionPage{section, section.Rows[min(i, len(section.Rows)):min(i+inspectionRowsPerPage, len(section.Rows))]})
			}
		}
	}
	return pages
}
func inspectionPages(result *desktopinspect.Result) int {
	return max(1, len(inspectionPageList(result)))
}
func (d *Controller) inspectionControl(argument string) error {
	words := splitFields(argument)
	if len(words) < 2 {
		return errors.New("Invalid inspector callback")
	}
	view := d.inspection
	if words[1] == "close" {
		// A user may close Form A while a completed job replaces it with B.
		// Closing belongs to the view lifetime, while other controls belong to
		// one rendered frame. A replaced view still cannot close its successor.
		if view == nil || words[0] != view.token {
			return nil
		}
		if len(words) != 2 {
			return errors.New("Invalid inspector close")
		}
		d.closeInspection()
		return d.fatal
	}
	if view == nil || words[0] != view.frame {
		return nil // Retired page/result controls cannot act on a newer frame.
	}
	switch words[1] {
	case "refresh":
		if len(words) != 2 {
			return errors.New("Invalid inspector refresh")
		}
		return d.refreshInspection()
	case "page":
		if len(words) != 3 {
			return errors.New("Invalid inspector page")
		}
		page, err := strconv.Atoi(words[2])
		if err != nil || page < 0 || page >= inspectionPages(view.result) {
			return errors.New("Inspector page is out of range")
		}
		view.page = page
		return d.showInspection()
	case "filter":
		if len(words) != 4 || !inspectionScope(words[2]) || len(words[3]) > 172 || !strings.HasPrefix(words[3], "x") {
			return errors.New("Invalid inspector filter")
		}
		decoded, err := base64.RawURLEncoding.Strict().DecodeString(words[3][1:])
		if err != nil || !inspectionFilter(string(decoded)) {
			return errors.New("Inspector filter must contain at most 128 UTF-8 bytes")
		}
		view.scope, view.filter, view.page = words[2], string(decoded), 0
		return d.refreshInspection()
	}
	return errors.New("Unknown inspector control")
}
func inspectionText(lines []string, text string) []string {
	return diagnosticText(lines, text)
}

func (d *Controller) showInspection() error {
	view := d.inspection
	if view == nil {
		return nil
	}
	view.frame = newID(16)
	callback := "Command SendToModule FvwmDesktop inspect-control " + view.frame + " "
	lines := []string{`Title "Configuration inspector"`, "Colorset 2", "ItemColorset 12",
		"CloseCommand SendToModule FvwmDesktop inspect-control " + view.token + " close",
		"Line left", `Text "Read-only inventory; source locations are literal candidates."`,
	}
	lines = inspectionText(lines, view.status)
	lines = append(lines, "Selection inspect_scope single")
	for i, scope := range inspectionScopes {
		if i%4 == 0 {
			lines = append(lines, "Line left")
		}
		selected := "off"
		if view.scope == scope {
			selected = "on"
		}
		lines = append(lines, "Choice inspect_"+scope+" "+scope+" "+selected+" "+quote(title(scope)))
	}
	lines = append(lines, "Line left", `Text "Filter"`, "InputHex inspect_filter 24 x"+hex.EncodeToString([]byte(view.filter)),
		`Button continue "Apply" ^M`, callback+"filter $(inspect_scope) x$(inspect_filter:Base64)")
	pages := inspectionPageList(view.result)
	if len(pages) == 0 {
		lines = append(lines, "Line left", `Text "No snapshot collected yet."`)
	} else {
		page := pages[view.page]
		lines = inspectionText(lines, "Snapshot scope: "+view.result.Scope+"; filter: "+view.result.Filter)
		lines = append(lines, "Line left", "Text "+quote(fmt.Sprintf("%s [%s] - page %d/%d", page.section.Title, page.section.State, view.page+1, len(pages))))
		if page.section.Generation != "" {
			lines = inspectionText(lines, "Generation: "+page.section.Generation)
		}
		if page.section.Reason != "" {
			lines = inspectionText(lines, page.section.Reason)
		}
		for _, row := range page.rows {
			lines = inspectionText(lines, row.Label)
			lines = inspectionText(lines, row.Value)
		}
		if len(page.rows) == 0 {
			lines = append(lines, "Line left", `Text "No matching rows in this section."`)
		}
		if page.section.Truncated {
			lines = append(lines, "Line left", `Text "Bounded inventory; additional rows were omitted."`)
		}
	}
	lines = append(lines, "Line expand", `Button quit "Close" ^[`, "Command SendToModule FvwmDesktop inspect-control "+view.token+" close")
	for _, step := range []struct {
		label, key string
		delta      int
	}{{"Previous (F1)", "F1", -1}, {"Next (F2)", "F2", 1}} {
		page := view.page + step.delta
		if page >= 0 && page < len(pages) {
			lines = append(lines, "Button continue "+quote(step.label)+" "+step.key, callback+"page "+strconv.Itoa(page))
		}
	}
	lines = append(lines, `Button continue "Refresh (F5)" F5`, callback+"refresh")
	d.form("SlateInspectorForm", lines, true)
	return d.fatal
}

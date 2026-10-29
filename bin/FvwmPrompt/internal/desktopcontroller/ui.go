// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/hex"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"strconv"
	"strings"
	"time"
	"unicode"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopcheck"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/desktopworker"
)

func clean(text string, limit int) string {
	out := []rune{}
	for _, c := range text {
		if unicode.IsPrint(c) && !strings.ContainsRune("\\\"$%&", c) {
			out = append(out, c)
		}
	}
	out = []rune(strings.TrimSpace(string(out)))
	return string(out[:min(limit, len(out))])
}
func quote(text string) string { return `"` + clean(text, 1024) + `"` }
func fold(text string) string  { return desktopworker.Casefold(text) }
func title(text string) string {
	if text == "" {
		return text
	}
	return strings.ToUpper(text[:1]) + text[1:]
}
func splitFields(text string) []string { return strings.Fields(text) }
func found(name string) string         { path, _ := exec.LookPath(name); return path }
func (d *Controller) menu(menu desktopsettings.Menu) {
	d.emit("DestroyMenu recreate " + menu.Name)
	d.emit("AddToMenu " + menu.Name + " " + quote(menu.Title) + " Title")
	for _, row := range menu.Rows {
		if row[1] == "Greyed" {
			d.emit("AddToMenu " + menu.Name + " Greyed " + quote(row[0]))
		} else {
			d.emit("AddToMenu " + menu.Name + " " + quote(row[0]) + " " + row[1])
		}
	}
}
func (d *Controller) form(alias string, lines []string, retire bool) {
	if retire {
		d.emit("KillModule FvwmForm " + alias)
	}
	d.emit("DestroyModuleConfig " + alias + ":*")
	for _, line := range lines {
		d.emit("*" + alias + ": " + line)
	}
	d.emit("Module FvwmForm " + alias)
}
func (d *Controller) button(name, text string) {
	d.emit("SendToModule SlateBar ChangeButton " + name + " Title " + quote(text))
}

// Literal diagnostics preserve punctuation and escape controls visibly. Each
// row is bounded independently of the source's path and logical line length.
func diagnosticText(lines []string, text string) []string {
	var escaped strings.Builder
	for _, r := range text {
		if unicode.IsControl(r) {
			quoted := strconv.QuoteRune(r)
			escaped.WriteString(quoted[1 : len(quoted)-1])
		} else {
			escaped.WriteRune(r)
		}
	}
	runes := []rune(escaped.String())
	for i := 0; i < max(1, len(runes)); i += 72 {
		lines = append(lines, "Line left", "TextHex x"+hex.EncodeToString([]byte(string(runes[min(i, len(runes)):min(i+72, len(runes))]))))
	}
	return lines
}
func (d *Controller) notice(text string) {
	if len(text) > 8192 {
		text = "Diagnostic text exceeds the display limit; context unavailable"
	}
	lines := []string{`Title "FVWM-X"`}
	for _, row := range strings.Split(text, "\n") {
		lines = diagnosticText(lines, row)
	}
	lines = append(lines, "Line right", `Button quit "Dismiss" ^[`, "Command Nop", `Timeout 80 Nop ""`)
	d.form("SlateNoticeForm", lines, true)
}
func (d *Controller) configurationError(err error, source *configSource) {
	text := err.Error() + "\n" + source.text()
	fmt.Fprintln(os.Stderr, "FvwmDesktop: "+text)
	d.notice(text)
}

// Build the whole projection before publication or retained-wallpaper mutation.
// The first five key positions are also used by opacity preview confirmation.
func (d *Controller) themePlan(settings values, minute int) ([]any, desktopsettings.ThemeProjection, values, error) {
	presentation, err := desktopsettings.ResolvePresentation(settings)
	if err != nil {
		return nil, desktopsettings.ThemeProjection{}, nil, err
	}
	wallpaper, err := desktopsettings.ResolveWallpaper(settings, minute)
	if err != nil {
		return nil, desktopsettings.ThemeProjection{}, nil, err
	}
	mode, err := desktopsettings.Appearance(settings, minute)
	if err != nil {
		return nil, desktopsettings.ThemeProjection{}, nil, err
	}
	palette := d.palettes[stringAt(settings, "palette")][mode]
	projection, err := desktopsettings.ProjectTheme(palette, mode, presentation.Contrast)
	if err != nil {
		return nil, desktopsettings.ThemeProjection{}, nil, err
	}
	if _, err := desktopsettings.ProjectTheme(d.palettes[stringAt(wallpaper, "palette")][stringAt(wallpaper, "appearance")], stringAt(wallpaper, "appearance"), "normal"); err != nil {
		return nil, desktopsettings.ThemeProjection{}, nil, err
	}
	key := []any{settings["palette"], mode, settings["transparency"], settings["motion"], settings["launcher_opacity"], wallpaper["palette"], wallpaper["appearance"], presentation.Contrast}
	return key, projection, wallpaper, nil
}
func (d *Controller) theme(force bool) error {
	now := time.Now()
	key, projection, selected, err := d.themePlan(d.effective, now.Hour()*60+now.Minute())
	if err != nil {
		return err
	}
	if !force && reflect.DeepEqual(key, d.lastTheme) {
		return nil
	}
	if d.preview.control != nil && d.preview.control.contrast != nil {
		d.preview.finish("Theme changed; high contrast preview recovery requested.", true, false)
	}
	presentation, err := desktopsettings.ResolvePresentation(d.effective)
	if err != nil {
		return err
	}
	palette := d.palettes[key[0].(string)][key[1].(string)]
	for _, role := range projection.Roles {
		d.emit(fmt.Sprintf("Colorset %d Plain, fg %s, bg %s, hi %s, sh %s", role.Colorset, role.Foreground, role.Background, role.Highlight, role.Shadow))
	}
	d.effective["wallpaper"] = selected
	wallpaper := [2]string{selected["palette"].(string), selected["appearance"].(string)}
	if wallpaper != d.lastWallpaper {
		background := d.palettes[wallpaper[0]][wallpaper[1]]["background"]
		d.emit("Colorset 20 Plain, bg " + background)
		if d.png {
			d.emit("Colorset 20 AspectPixmap " + quote(filepath.Join(d.root, "wallpapers", wallpaper[0]+"-"+wallpaper[1]+".png")))
		}
		d.lastWallpaper = wallpaper
	}
	if presentation.Transparency {
		d.emit("Colorset 10 RootTransparent buffer, Tint " + palette["surface"] + " 88")
		d.emit("MenuStyle * Translucent 94")
	} else {
		d.emit("MenuStyle * !Translucent")
	}
	animation, steps := "!Animation", "0"
	if key[3] == true {
		animation, steps = "Animation", "6"
	}
	d.emit("MenuStyle * " + animation)
	d.emit("CopyMenuStyle * SlateLauncherStyle")
	if presentation.Transparency {
		d.emit("MenuStyle SlateLauncherStyle Translucent " + strconv.Itoa(presentation.LauncherOpacity))
	}
	d.emit(`MenuStyle SlateLauncherStyle ItemFormat "%24p%l%80p%>%24p"`)
	d.emit("Style * WindowShadeSteps " + steps)
	d.button("appearance", title(key[0].(string)))
	if !reflect.DeepEqual(d.lastTheme, key) {
		d.staleInspection("Theme policy changed; refresh to collect the current policy.")
	}
	d.lastTheme = key
	d.lastThemeAppearance = key[1].(string)
	d.lastThemeColors = make(map[string]string, len(palette))
	for name, color := range palette {
		d.lastThemeColors[name] = color
	}
	if err := d.settingsMenu("all"); err != nil {
		return err
	}
	d.highlightDesk()
	return d.fatal
}
func (d *Controller) highlightDesk() {
	for i := 0; i < 4; i++ {
		colorset := "10"
		if i == d.desk {
			colorset = "11"
		}
		d.emit(fmt.Sprintf("SendToModule SlateBar ChangeButton desk%d Colorset %s", i, colorset))
	}
}
func (d *Controller) applicationMenu(query *string) {
	if d.closed {
		return
	}
	if query != nil {
		runes := []rune(*query)
		text := string(runes[:min(80, len(runes))])
		d.appQuery = &text
	}
	menus := d.applicationMenus(nil)
	if d.appQuery != nil {
		menus = append(menus, d.applicationMenus(d.appQuery)...)
	}
	changed := !reflect.DeepEqual(d.appBinding.menus, menus)
	d.publishMenus(d.appBinding, menus)
	if changed {
		d.emit("ChangeMenuStyle SlateLauncherStyle SlateLauncherMenu")
	}
	if query != nil {
		d.emit("Menu SlateSearchResults Monitor c c")
	}
}

// Both the launcher and the most recent submitted search retain bounded native
// projections. A worker loss updates their Greyed rows without reopening a menu.
func (d *Controller) applicationMenus(query *string) []desktopsettings.Menu {
	menus := []desktopsettings.Menu{}
	available := d.appBinding.known && !d.appBinding.lost
	indexes := []int{}
	for i, app := range d.apps {
		if query == nil || strings.Contains(fold(app.Name), fold(*query)) {
			indexes = append(indexes, i)
		}
	}
	itemsFor := func(indexes []int) []desktopsettings.Row {
		items := []desktopsettings.Row{}
		for _, i := range indexes {
			action := "Greyed"
			if available {
				action = "SendToModule FvwmDesktop launch-bound " +
					d.instance + " " + d.appGeneration + " " +
					strconv.Itoa(i)
			}
			items = append(items, desktopsettings.Row{d.apps[i].Name, action})
		}
		return items
	}
	rows := []desktopsettings.Row{}
	if len(indexes) <= 20 {
		rows = itemsFor(indexes)
	} else {
		for page := 0; page < len(indexes); page += 20 {
			group := indexes[page:min(page+20, len(indexes))]
			name := "SlateApps"
			if query != nil {
				name = "SlateResults"
			}
			name += strconv.Itoa(page / 20)
			menus = append(menus, desktopsettings.Menu{Name: name, Title: "Applications", Rows: itemsFor(group)})
			rows = append(rows, desktopsettings.Row{d.apps[group[0]].Name + " ...", "Popup " + name})
		}
	}
	if !available {
		label := "Application discovery pending"
		if d.appBinding.lost {
			label = "Application discovery unavailable; discover again"
		}
		rows = append([]desktopsettings.Row{{label, "Greyed"}}, rows...)
	}
	if query != nil {
		if len(rows) == 0 {
			rows = []desktopsettings.Row{{"No matching applications", "Greyed"}}
		}
		return append(menus, desktopsettings.Menu{Name: "SlateSearchResults", Title: "Search: " + *query, Rows: rows})
	}
	rows = append(rows, desktopsettings.Row{"", "Nop"}, desktopsettings.Row{"Action palette...", "SendToModule FvwmDesktop palette"}, desktopsettings.Row{"Terminal", "SendToModule FvwmDesktop terminal"},
		desktopsettings.Row{"Search applications...", "Module FvwmForm SlateSearchForm"}, desktopsettings.Row{"Run command...", "Module FvwmForm SlateRunForm"},
		desktopsettings.Row{"Launch activity...", "SendToModule FvwmDesktop launch-activity"},
		desktopsettings.Row{"Files", "SendToModule FvwmDesktop files"}, desktopsettings.Row{"Workspaces", "Module FvwmPager 0 3"},
		desktopsettings.Row{"Appearance", "Popup SlateAppearance"}, desktopsettings.Row{"System", "Popup SlateSystem"}, desktopsettings.Row{"Session", "Popup SlateSession"})
	return append(menus, desktopsettings.Menu{Name: "SlateLauncherMenu", Title: "Applications", Rows: rows})
}
func (d *Controller) systemMenu() {
	if d.closed {
		return
	}
	menus := []desktopsettings.Menu{}
	available := d.workerAvailable()
	battery := "not reported"
	if d.status.Battery != nil {
		battery = strconv.Itoa(*d.status.Battery) + "%"
	}
	network := d.status.Network
	if !available {
		network = "unavailable (last: " + network + ")"
	}
	menus = append(menus, desktopsettings.Menu{Name: "SlateSystem", Title: "System", Rows: []desktopsettings.Row{
		{"Network: " + network, "SendToModule FvwmDesktop network"}, {"Sound", "Popup SlateSound"}, {"Battery: " + battery, "Nop"},
		{"Calendar", "SendToModule FvwmDesktop calendar"}, {"Display information", "SendToModule FvwmDesktop displays"},
		{"Screenshot", "SendToModule FvwmDesktop screenshot"}, {"System monitor", "SendToModule FvwmDesktop monitor"}}})
	backend := "amixer"
	if d.system == "OpenBSD" {
		backend = "sndioctl"
	}
	if d.system == "FreeBSD" {
		backend = "mixer"
	}
	rows := []desktopsettings.Row{{"No sound control backend is available", "Greyed"}}
	if available && found(backend) != "" {
		rows = []desktopsettings.Row{{"Volume up", "SendToModule FvwmDesktop volume up"}, {"Volume down", "SendToModule FvwmDesktop volume down"}, {"Mute / unmute", "SendToModule FvwmDesktop volume mute"}}
	}
	if !available {
		rows = []desktopsettings.Row{{"Sound control unavailable; waiting for desktop worker", "Greyed"}}
	}
	menus = append(menus, desktopsettings.Menu{Name: "SlateSound", Title: "Sound", Rows: rows})
	lock := "Greyed"
	if found("xlock") != "" || found("slock") != "" {
		lock = "SendToModule FvwmDesktop lock"
	}
	poweroff, reboot := "Greyed", "Greyed"
	if available {
		poweroff = "SendToModule FvwmDesktop confirm poweroff"
		reboot = "SendToModule FvwmDesktop confirm reboot"
	}
	menus = append(menus, desktopsettings.Menu{Name: "SlateSession", Title: "Session", Rows: []desktopsettings.Row{{"Lock", lock}, {"Log out...", "SendToModule FvwmDesktop confirm logout"},
		{"Restart FVWM", "Restart"}, {"Power off...", poweroff}, {"Reboot...", reboot}}})
	d.publishMenus(d.systemBinding, menus)
}

// Page by rendered rows, not diagnostics: one complete include chain can be
// taller than a window. Continuations retain every source byte and row order.
const reportRowsPerPage = 12

func reportPages(result desktopcheck.Result) [][]string {
	var lines []string
	if len(result.Diagnostics) == 0 {
		lines = diagnosticText(lines, "No errors in the supported literal settings subset.")
	}
	for _, item := range result.Diagnostics {
		span := strconv.Itoa(item.Line)
		if item.EndLine != item.Line {
			span += "-" + strconv.Itoa(item.EndLine)
		}
		lines = diagnosticText(lines, item.Path+":"+span)
		lines = diagnosticText(lines, item.Severity+": "+item.Message)
		for _, parent := range item.IncludedFrom {
			span := strconv.Itoa(parent.Line)
			if parent.EndLine != parent.Line {
				span += "-" + strconv.Itoa(parent.EndLine)
			}
			lines = diagnosticText(lines, "Read from "+parent.Path+":"+span)
		}
	}
	pages := [][]string{}
	for len(lines) > 0 {
		n := min(len(lines), reportRowsPerPage*2)
		pages = append(pages, lines[:n])
		lines = lines[n:]
	}
	return pages
}

func (d *Controller) showReport(token string, page int) error {
	if d.report == nil || d.report.token != token {
		return errors.New("Configuration report is stale; run the check again")
	}
	result := d.report.result
	content := reportPages(result)
	pages := len(content)
	if page < 0 || page >= pages {
		return errors.New("Invalid configuration report page")
	}
	lines := []string{`Title "Configuration check"`, "Colorset 2", "ItemColorset 12", "Line left", `Text "Desktop settings subset - configuration was not executed"`,
		"Line left", "Text " + quote(fmt.Sprintf("%d errors, %d unchecked; page %d/%d", result.Errors, result.Unchecked, page+1, pages))}
	lines = append(lines, content[page]...)
	lines = append(lines, "Line left", `Text "Other FVWM syntax and effective runtime state are not validated."`, "Line expand", `Button quit "Close" ^[`, "Command Nop")
	for _, direction := range []struct {
		label, key string
		page       int
	}{{"Previous (F1)", "F1", page - 1}, {"Next (F2)", "F2", page + 1}} {
		if direction.page >= 0 && direction.page < pages {
			lines = append(lines, "Button continue "+quote(direction.label)+" "+direction.key, "Command SendToModule FvwmDesktop check-page "+token+" "+strconv.Itoa(direction.page))
		}
	}
	d.form("SlateConfigCheckForm", lines, true)
	return d.fatal
}

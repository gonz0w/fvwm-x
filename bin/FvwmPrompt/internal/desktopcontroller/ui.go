// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"errors"
	"fmt"
	"os/exec"
	"path/filepath"
	"reflect"
	"strconv"
	"strings"
	"time"
	"unicode"

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
func (d *Controller) notice(text string) {
	d.form("SlateNoticeForm", []string{`Title "FVWM-X"`, "Line left", "Text " + quote(text), "Line right", `Button quit "Dismiss" ^[`, "Command Nop", `Timeout 80 Nop ""`}, true)
}
func (d *Controller) theme(force bool) error {
	now := time.Now()
	mode, err := desktopsettings.Appearance(d.effective, now.Hour()*60+now.Minute())
	if err != nil {
		return err
	}
	key := []any{d.effective["palette"], mode, d.effective["transparency"], d.effective["motion"], d.effective["launcher_opacity"]}
	if !force && reflect.DeepEqual(key, d.lastTheme) {
		return nil
	}
	palette := d.palettes[key[0].(string)][mode]
	if len(palette) == 0 {
		return errors.New("Missing desktop palette")
	}
	roles := []struct {
		number int
		fg, bg string
	}{{0, "text", "surface"}, {1, "muted", "surface"}, {2, "text", "raised"}, {3, "border", "border"},
		{4, "accent", "accent"}, {5, "text", "surface"}, {6, "on_accent", "accent"}, {7, "muted", "surface"}, {8, "accent", "surface"},
		{10, "text", "surface"}, {11, "on_accent", "accent"}, {12, "text", "raised"}, {13, "on_accent", "accent"}, {14, "muted", "surface"}}
	for _, r := range roles {
		d.emit(fmt.Sprintf("Colorset %d Plain, fg %s, bg %s, hi %s, sh %s", r.number, palette[r.fg], palette[r.bg], palette[r.bg], palette[r.bg]))
	}
	if d.effective["keep_wallpaper"] != true {
		d.effective["wallpaper"] = values{"palette": key[0], "appearance": mode}
	}
	selected := d.effective["wallpaper"].(map[string]any)
	wallpaper := [2]string{selected["palette"].(string), selected["appearance"].(string)}
	if wallpaper != d.lastWallpaper {
		background := d.palettes[wallpaper[0]][wallpaper[1]]["background"]
		d.emit("Colorset 20 Plain, bg " + background)
		if d.png {
			d.emit("Colorset 20 AspectPixmap " + quote(filepath.Join(d.root, "wallpapers", wallpaper[0]+"-"+wallpaper[1]+".png")))
		}
		d.lastWallpaper = wallpaper
	}
	if key[2] == true {
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
	if key[2] == true {
		d.emit("MenuStyle SlateLauncherStyle Translucent " + fmt.Sprint(key[4]))
	}
	d.emit(`MenuStyle SlateLauncherStyle ItemFormat "%24p%l%80p%>%24p"`)
	d.emit("Style * WindowShadeSteps " + steps)
	d.button("appearance", title(key[0].(string)))
	d.lastTheme = key
	d.menu(desktopsettings.AppearanceMenu(d.effective))
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
	indexes := []int{}
	for i, app := range d.apps {
		if query == nil || strings.Contains(fold(app.Name), fold(*query)) {
			indexes = append(indexes, i)
		}
	}
	rows := []desktopsettings.Row{}
	itemsFor := func(indexes []int) []desktopsettings.Row {
		items := []desktopsettings.Row{}
		for _, i := range indexes {
			items = append(items, desktopsettings.Row{d.apps[i].Name, fmt.Sprintf("SendToModule FvwmDesktop launch %d", i)})
		}
		return items
	}
	for page := 0; page < len(indexes); page += 20 {
		group := indexes[page:min(page+20, len(indexes))]
		name := "SlateApps"
		if query != nil {
			name = "SlateResults"
		}
		name += strconv.Itoa(page / 20)
		d.menu(desktopsettings.Menu{Name: name, Title: "Applications", Rows: itemsFor(group)})
		rows = append(rows, desktopsettings.Row{d.apps[group[0]].Name + " ...", "Popup " + name})
	}
	if query != nil {
		items := rows
		if len(indexes) <= 20 {
			items = itemsFor(indexes)
		}
		if len(items) == 0 {
			items = []desktopsettings.Row{{"No matching applications", "Greyed"}}
		}
		d.menu(desktopsettings.Menu{Name: "SlateSearchResults", Title: "Search: " + *query, Rows: items})
		d.emit("Menu SlateSearchResults Monitor c c")
		return
	}
	if len(indexes) <= 20 {
		rows = itemsFor(indexes)
	}
	rows = append(rows, desktopsettings.Row{"", "Nop"}, desktopsettings.Row{"Terminal", "SendToModule FvwmDesktop terminal"},
		desktopsettings.Row{"Search applications...", "Module FvwmForm SlateSearchForm"}, desktopsettings.Row{"Run command...", "Module FvwmForm SlateRunForm"},
		desktopsettings.Row{"Files", "SendToModule FvwmDesktop files"}, desktopsettings.Row{"Workspaces", "Module FvwmPager 0 3"},
		desktopsettings.Row{"Appearance", "Popup SlateAppearance"}, desktopsettings.Row{"System", "Popup SlateSystem"}, desktopsettings.Row{"Session", "Popup SlateSession"})
	d.menu(desktopsettings.Menu{Name: "SlateLauncherMenu", Title: "Applications", Rows: rows})
	d.emit("ChangeMenuStyle SlateLauncherStyle SlateLauncherMenu")
}
func (d *Controller) systemMenu() {
	battery := "not reported"
	if d.status.Battery != nil {
		battery = strconv.Itoa(*d.status.Battery) + "%"
	}
	d.menu(desktopsettings.Menu{Name: "SlateSystem", Title: "System", Rows: []desktopsettings.Row{
		{"Network: " + d.status.Network, "SendToModule FvwmDesktop network"}, {"Sound", "Popup SlateSound"}, {"Battery: " + battery, "Nop"},
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
	if found(backend) != "" {
		rows = []desktopsettings.Row{{"Volume up", "SendToModule FvwmDesktop volume up"}, {"Volume down", "SendToModule FvwmDesktop volume down"}, {"Mute / unmute", "SendToModule FvwmDesktop volume mute"}}
	}
	d.menu(desktopsettings.Menu{Name: "SlateSound", Title: "Sound", Rows: rows})
	lock := "Greyed"
	if found("xlock") != "" || found("slock") != "" {
		lock = "SendToModule FvwmDesktop lock"
	}
	d.menu(desktopsettings.Menu{Name: "SlateSession", Title: "Session", Rows: []desktopsettings.Row{{"Lock", lock}, {"Log out...", "SendToModule FvwmDesktop confirm logout"},
		{"Restart FVWM", "Restart"}, {"Power off...", "SendToModule FvwmDesktop confirm poweroff"}, {"Reboot...", "SendToModule FvwmDesktop confirm reboot"}}})
}
func (d *Controller) showReport(token string, page int) error {
	if d.report == nil || d.report.token != token {
		return errors.New("Configuration report is stale; run the check again")
	}
	result := d.report.result
	pages := max(1, (len(result.Diagnostics)+3)/4)
	if page < 0 || page >= pages {
		return errors.New("Invalid configuration report page")
	}
	lines := []string{`Title "Configuration check"`, "Colorset 2", "ItemColorset 12", "Line left", `Text "Desktop settings subset - configuration was not executed"`,
		"Line left", "Text " + quote(fmt.Sprintf("%d errors, %d unchecked; page %d/%d", result.Errors, result.Unchecked, page+1, pages))}
	if len(result.Diagnostics) == 0 {
		lines = append(lines, "Line left", `Text "No errors in the supported literal settings subset."`)
	}
	for _, item := range result.Diagnostics[page*4 : min(page*4+4, len(result.Diagnostics))] {
		path := []rune(item.Path)
		if len(path) > 80 {
			path = append([]rune("..."), path[len(path)-77:]...)
		}
		span := strconv.Itoa(item.Line)
		if item.EndLine != item.Line {
			span += "-" + strconv.Itoa(item.EndLine)
		}
		lines = append(lines, "Line left", "Text "+quote(string(path)+":"+span), "Line left", "Text "+quote(item.Severity+": "+item.Message))
		if len(item.IncludedFrom) > 0 {
			parent := item.IncludedFrom[len(item.IncludedFrom)-1]
			lines = append(lines, "Line left", "Text "+quote("Read from "+filepath.Base(parent.Path)+":"+strconv.Itoa(parent.Line)))
		}
	}
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

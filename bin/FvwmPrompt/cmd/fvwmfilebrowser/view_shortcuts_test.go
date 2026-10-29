// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"fmt"
	"strings"
	"testing"
)

func TestBrowserButtonsHaveUniqueDocumentedShortcuts(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.renderPath = "/old-folder"
	for i := 1; i <= maxActions; i++ {
		b.actions = append(b.actions, configuredAction{Label: fmt.Sprintf("Action %d", i), Program: "/configured"})
	}
	want := map[string]struct{ label, action, argument string }{
		"Alt-F": {"Filter (Alt-F)", "filter", "x$(filter:Base64)"},
		"Alt-C": {"Clear (Alt-C)", "unfilter", "-"},
		"Alt-S": {"Sort (Alt-S)", "sort", "-"},
		"Alt-R": {"Reverse (Alt-R)", "reverse", "-"},
		"Alt-A": {"Select visible (Alt-A)", "select-visible", "-"},
		"Alt-D": {"Clear selection (Alt-D)", "clear-selection", "-"},
		"Alt-H": {"Home (Alt-H)", "home", "-"},
		"Alt-U": {"Up (Alt-U)", "up", "-"},
		"Alt-B": {"Back (Alt-B)", "back", "-"},
		"Alt-G": {"Go (Alt-G)", "path", "x$(path:Base64)"},
		"Alt-O": {"Open file with (Alt-O)", "file-roles", "-"},
		"Alt-X": {"Cancel (Alt-X)", "cancel", "-"},
		"F1":    {"Details (F1)", "details", "-"},
		"F2":    {"Properties (F2)", "properties", "-"},
		"F3":    {"Selected (F3)", "selected", "-"},
		"F4":    {"Terminal (F4)", "terminal", "-"},
		"F5":    {"Refresh (F5)", "refresh", "-"},
		"F6":    {"Hidden (F6)", "hidden", "-"},
		"F7":    {"View (F7)", "view", "-"},
		"F8":    {"Watch (F8)", "watch", "-"},
		"F9":    {"Jobs (F9)", "jobs", "-"},
		"F12":   {"Open folder with (F12)", "directory-roles", "-"},
		"^M":    {"Open (Enter)", "focused", "-"},
		"^[":    {"Close (Esc)", "close", "-"},
	}
	for i := 1; i <= maxActions; i++ {
		key := fmt.Sprintf("Alt-%d", i)
		want[key] = struct{ label, action, argument string }{fmt.Sprintf("Action %d (%s)", i, key), "custom", fmt.Sprint(i - 1)}
	}
	seen := make(map[string]bool)
	notify, pending := "", ""
	helped := false
	_, err := layoutScript(t, b, peer, b.show, func(command string) string {
		if pending != "" {
			if !strings.HasSuffix(command, pending) {
				t.Fatal("shortcut did not preserve its typed/versioned callback", command, pending)
			}
			pending = ""
		}
		if _, text, found := strings.Cut(command, `: Button continue "`); found {
			label, key, valid := strings.Cut(text, `" `)
			entry, known := want[key]
			if !valid || !known || seen[key] || label != entry.label {
				t.Fatal("missing/duplicate/undocumented shortcut", command)
			}
			seen[key] = true
			pending = fmt.Sprintf("Command Echo FVWM_FILES_3 %s %d %s %s x$(picks:Base64) $(FormJob:Token)", b.instance, b.form, entry.action, entry.argument)
			if key == "F1" {
				pending = fmt.Sprintf("Command Echo FVWM_FILES_DETAILS_OPEN_1 %s %d $(FormJob:Token)", b.instance, b.form)
			}
		}
		if strings.Contains(command, `: Text "Alt-letter/digit: labeled actions; Ctrl-U: clear input."`) {
			helped = true
		}
		if strings.Contains(command, ": ListNotify ") {
			notify = strings.Fields(command)[2]
		}
		if strings.HasPrefix(command, "Module FvwmForm ") {
			if notify == "" {
				t.Fatal("missing native startup observation")
			}
			return layoutReply("SendToModule unused ListStatus picks "+notify+" selection", 1, 0, []byte{0, 1, 1})
		}
		return ""
	})
	if err != nil || len(seen) != len(want) || pending != "" || !helped {
		t.Fatal("not every browser action is keyboard-accessible", err, seen)
	}
}

func TestBrowserLabelViewportsAndSummary(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.renderPath = "/old-folder"
	labels := make(map[string]bool)
	pending, notify := "", ""
	_, err := layoutScript(t, b, peer, b.show, func(command string) string {
		if pending != "" {
			if !strings.HasSuffix(command, ": LabelViewport "+pending) {
				t.Fatalf("label %s lacks its viewport: %s", pending, command)
			}
			labels[pending] = true
			pending = ""
		} else if _, value, ok := strings.Cut(command, ": Label "); ok {
			pending = strings.Fields(value)[0]
		}
		if strings.Contains(command, ": ListNotify ") {
			notify = strings.Fields(command)[2]
		}
		if strings.HasPrefix(command, "Module FvwmForm ") {
			return layoutReply("SendToModule unused ListStatus picks "+notify+" selection", 1, 0, []byte{0, 1, 1})
		}
		return ""
	})
	if err != nil || pending != "" || len(labels) != 11 {
		t.Fatal(err, pending, labels)
	}
	for _, name := range []string{"path-label", "status", "watcher", "projection", "progress", "readiness", "counts", "inspector", "detail1", "detail2", "detail3"} {
		if !labels[name] {
			t.Fatal("missing viewport", name)
		}
	}
	if !strings.HasPrefix(b.countText(), "Selected ") || !strings.Contains(b.countText(), "; hidden ") || !strings.Contains(b.countText(), "; limit ") {
		t.Fatal(b.countText())
	}
	projection := b.projectionText()
	if strings.Index(projection, "; sort:") > strings.Index(projection, "; filter:") {
		t.Fatal(projection)
	}
}

func TestLiveSummaryUsesSameFormatting(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	commands, err := layoutScript(t, b, peer, b.show, func(command string) string {
		if strings.Contains(command, " ListStatus ") {
			return layoutReply(command, 7, 0, []byte{0, 1, 1})
		}
		return ""
	})
	if err != nil {
		t.Fatal(err)
	}
	for name, value := range map[string]string{"counts": b.countText(), "projection": b.projectionText(), "readiness": b.readinessText(), "status": b.status} {
		want := "SendToModule " + b.alias + " SetLabel " + name + " \"" + displayText(value, 120) + "\""
		found := false
		for _, command := range commands {
			found = found || command == want
		}
		if !found {
			t.Fatal("missing live summary", want, commands)
		}
	}
}

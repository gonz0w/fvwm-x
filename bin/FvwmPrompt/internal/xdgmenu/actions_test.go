// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"strings"
	"testing"
)

func TestCurrentDesktopsSharesColonListAndEmptyFallback(t *testing.T) {
	for value, want := range map[string][]string{"": {"FVWM"}, "Other:Third": {"Other", "Third"}, "Other:FVWM": {"Other", "FVWM"}, ":": {"", ""}} {
		if got := CurrentDesktops(value); !reflect.DeepEqual(got, want) {
			t.Fatal(value, got, want)
		}
	}
}

func TestDeclaredActionPlanBoundsMetadataAndLeavesLaunchToCaller(t *testing.T) {
	entry := parsedDesktop(t, "Name=Parent\nActions=One;One;Missing;BadName;BadIcon;Empty;Two;Eight;Outside;\n"+
		"[Desktop Action One]\nName=First\nName[fr]=Première\nIcon=first\nIcon[fr]=première\nExec=action %i %c %k\n"+
		"[Desktop Action BadName]\nName=bad\\q\nExec=action\n"+
		"[Desktop Action BadIcon]\nName=Bad icon\nIcon=bad\\q\nExec=action\n"+
		"[Desktop Action Empty]\nName=\nExec=action\n"+
		"[Desktop Action Two]\nName=Second\nIcon[fr]=without-base-is-not-an-icon\nExec=action %Z\n"+
		"[Desktop Action Eight]\nName=Eighth\nExec=action\n"+
		"[Desktop Action Outside]\nName=Outside bound\nExec=action\n")
	got := DeclaredActions(entry, []string{"fr"}, "Parent français", "parent-fr")
	want := []DeclaredAction{
		{"Parent français — Première", "première", "action %i %c %k"},
		{"Parent français — Second", "parent-fr", "action %Z"},
		{"Parent français — Eighth", "parent-fr", "action"},
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatal(got, want)
	}
	// The plan intentionally retains raw Exec: consumers expand one at a time
	// with their own argv limits, then skip the malformed %Z action.
	if entry.Groups["Desktop Action One"]["Icon"] != "first" {
		t.Fatal("input metadata mutated")
	}
	entry.Groups["Desktop Entry"]["Actions"] = `One\q;`
	if got := DeclaredActions(entry, nil, "Parent", ""); got != nil {
		t.Fatal("malformed list supplied actions", got)
	}
}

func TestLocalizedIconOptionalBaseAndMalformedFallback(t *testing.T) {
	entry := parsedDesktop(t, "Name=Parent\nIcon=base\\sicon\nIcon[fr]=icône\\slocale\n")
	for locale, want := range map[string]string{"fr_FR.UTF-8": "icône locale", "de_DE.UTF-8": "base icon"} {
		if got, err := LocalizedIcon(entry, "Desktop Entry", []string{locale}); err != nil || got != want {
			t.Fatal(got, err, want)
		}
	}
	delete(entry.Groups["Desktop Entry"], "Icon")
	if got, err := LocalizedIcon(entry, "Desktop Entry", []string{"fr"}); err != nil || got != "" {
		t.Fatal("localized-only optional icon acquired a base", got, err)
	}
	entry.Groups["Desktop Entry"]["Icon"] = `bad\q`
	if _, err := LocalizedIcon(entry, "Desktop Entry", nil); err == nil {
		t.Fatal("malformed icon silently accepted")
	}
}

func TestMenuActionBudgetOverflowRefusesWholePresentation(t *testing.T) {
	for _, budget := range []struct {
		name         string
		items, bytes int
	}{{"item", 9, MaxSourceBytes}, {"bytes", MaxItems, 32}} {
		t.Run(budget.name, func(t *testing.T) {
			for _, withAction := range []bool{false, true} {
				body := "Name=A\nExec=app\n"
				if withAction {
					body += "Actions=One;\n[Desktop Action One]\nName=" + strings.Repeat("B", 80) + "\nExec=action\n"
				}
				entry := parsedDesktop(t, body)
				root := mergeFixture(t, `<AppDir>/apps</AppDir><Include><All/></Include>`, nil)
				compiled, err := BuildMenuPlans(root, map[AppLocation]map[string]PoolEntry{{Path: "/apps"}: {"a.desktop": {Path: "/apps/a.desktop", Entry: entry}}})
				if err != nil {
					t.Fatal(err)
				}
				allocated, err := AllocateMenus(compiled.Plan)
				if err != nil {
					t.Fatal(err)
				}
				state := presentationState{remaining: budget.items, bytes: budget.bytes}
				got, err := state.menu(compiled, allocated, 0)
				if withAction {
					if err == nil || got != nil {
						t.Fatal("action escaped global budget or published partial result", got, err)
					}
				} else if err != nil || got == nil || len(got.Items) != 1 {
					t.Fatal("primary control no longer fits same budget", got, err)
				}
			}
		})
	}
}

func TestMenuExecutableCallbackRetainsTryExecOnlyPolicy(t *testing.T) {
	for _, withCallback := range []bool{false, true} {
		calls := 0
		var executable func(string) bool
		if withCallback {
			executable = func(name string) bool {
				calls++
				if name != "missing-try-exec" {
					t.Fatal("presentation resolved argv executable instead of TryExec", name)
				}
				return false
			}
		}
		entry := parsedDesktop(t, "Name=Parent\nExec=missing-parent\nActions=One;\n[Desktop Action One]\nName=Action\nExec=missing-action\n")
		hidden := parsedDesktop(t, "Name=Unavailable\nTryExec=missing-try-exec\nExec=another-missing-parent\n")
		got := presentationFixture(t, `<Include><All/></Include>`, map[string]*Desktop{"parent.desktop": entry, "try.desktop": hidden}, PresentationOptions{Executable: executable})
		if !reflect.DeepEqual(presentedLabels(got), []string{"Parent", "Parent — Action"}) ||
			!reflect.DeepEqual(got.Items[0].Argv, []string{"missing-parent"}) ||
			!reflect.DeepEqual(got.Items[1].Argv, []string{"missing-action"}) {
			t.Fatal("standalone argv resolution moved before launch", got)
		}
		expectedCalls := 0
		if withCallback {
			expectedCalls = 1
		}
		if calls != expectedCalls {
			t.Fatal("TryExec lookup policy changed", calls, expectedCalls)
		}
	}
}

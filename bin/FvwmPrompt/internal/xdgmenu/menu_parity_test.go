// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"testing"
)

func parityDesktop(t *testing.T) *Desktop {
	return parsedDesktop(t, "Name=Parent\nName[fr]=Parent français\nIcon=parent-icon\nIcon[fr]=parent-fr\nExec=app %i %c %k\nTerminal=true\nPath=/working path\nActions=Zulu;Alpha;Bad;Missing;Zulu;\n"+
		"[Desktop Action Zulu]\nName=Zulu\nName[fr]=Dernière\nIcon=action-icon\nIcon[fr]=action-fr\nExec=action %i %c %k\n"+
		"[Desktop Action Alpha]\nName=Alpha\nName[fr]=Première\nExec=action %i %c %k\n"+
		"[Desktop Action Bad]\nName=Invalid\nExec=action \"%c\"\n"+
		"[Desktop Action Unlisted]\nName=Never included\nExec=action\n")
}

// These regressions use only pre-parity presentation APIs for old-code RED.
func TestMenuParityDeclaredActionsKeepParentContextAndLocalizedIcons(t *testing.T) {
	got := presentationFixture(t, `<Include><All/></Include>`, map[string]*Desktop{"parent.desktop": parityDesktop(t)}, PresentationOptions{Locales: []string{"fr"}})
	want := []string{"Parent français", "Parent français — Dernière", "Parent français — Première"}
	if !reflect.DeepEqual(presentedLabels(got), want) {
		t.Fatal(presentedLabels(got), want)
	}
	for index, item := range got.Items {
		executable, icon := "action", "parent-fr"
		if index == 0 {
			executable = "app"
		}
		if index == 1 {
			icon = "action-fr"
		}
		if item.Entry == nil || item.Entry.ID != "parent.desktop" || !item.Terminal || item.WorkingDir != "/working path" || item.Icon != icon ||
			!reflect.DeepEqual(item.Argv, []string{executable, "--icon", icon, "Parent français", "/apps/parent.desktop"}) {
			t.Fatal("action lost parent source/context/icon", item)
		}
	}
}

func TestMenuParityClustersReserveAndMergeOnlyParentID(t *testing.T) {
	entries := map[string]*Desktop{"parent.desktop": parityDesktop(t), "other.desktop": displayDesktop(t, "Name=Other\n")}
	for _, test := range []struct {
		layout string
		want   []string
	}{
		{`<Layout><Filename>parent.desktop</Filename><Merge type="files"/></Layout>`, []string{"Parent", "Parent — Zulu", "Parent — Alpha", "Other"}},
		{`<Layout><Merge type="files"/><Filename>parent.desktop</Filename></Layout>`, []string{"Other", "Parent", "Parent — Zulu", "Parent — Alpha"}},
		{`<Layout><Filename>parent.desktop</Filename><Filename>parent.desktop</Filename><Merge type="all"/></Layout>`, []string{"Parent", "Parent — Zulu", "Parent — Alpha", "Other"}},
		{`<Layout><Merge type="files"/></Layout>`, []string{"Other", "Parent", "Parent — Zulu", "Parent — Alpha"}},
	} {
		got := presentationFixture(t, `<Include><All/></Include>`+test.layout, entries, PresentationOptions{})
		if !reflect.DeepEqual(presentedLabels(got), test.want) {
			t.Fatal(presentedLabels(got), test.want)
		}
	}
}

func TestMenuParityInlineLimitCountsVisibleActionRows(t *testing.T) {
	entries := map[string]*Desktop{"parent.desktop": parityDesktop(t)}
	for _, test := range []struct {
		limit string
		want  []string
		kind  ItemKind
	}{
		{"2", []string{"Tools"}, SubmenuItem},
		{"3", []string{"Parent", "Parent — Zulu", "Parent — Alpha"}, ApplicationItem},
	} {
		got := presentationFixture(t, `<Menu><Name>Tools</Name><Include><All/></Include></Menu><Layout><Menuname inline="true" inline_alias="true" inline_header="false" inline_limit="`+test.limit+`">Tools</Menuname></Layout>`, entries, PresentationOptions{})
		if !reflect.DeepEqual(presentedLabels(got), test.want) || got.Items[0].Kind != test.kind {
			t.Fatal(got, test.want)
		}
	}
}

func TestMenuParityDirectoryIconUsesSameLocaleFallback(t *testing.T) {
	directory, err := ParseDesktop([]byte("[Desktop Entry]\nType=Directory\nName=Folder\nName[fr]=Dossier\nIcon=folder-default\nIcon[fr]=folder-fr\n"))
	if err != nil {
		t.Fatal(err)
	}
	for _, locale := range []string{"fr_FR.UTF-8", "de_DE.UTF-8"} {
		got := presentationFixture(t, `<DirectoryDir>/metadata</DirectoryDir><Directory>root.directory</Directory><Include><All/></Include>`, map[string]*Desktop{"app.desktop": displayDesktop(t, "Name=App\n")}, PresentationOptions{Locales: []string{locale}, Directories: map[string]*Desktop{"/metadata/root.directory": directory}})
		want := "folder-default"
		if locale == "fr_FR.UTF-8" {
			want = "folder-fr"
		}
		if got.Icon != want {
			t.Fatal(got.Icon, want)
		}
	}
}

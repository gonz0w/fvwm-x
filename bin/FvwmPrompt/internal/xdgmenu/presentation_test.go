// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"strings"
	"testing"
)

func presentationFixture(t *testing.T, xml string, entries map[string]*Desktop, options PresentationOptions) *PresentedMenu {
	t.Helper()
	pool := make(map[string]PoolEntry)
	for id, d := range entries {
		pool[id] = PoolEntry{Path: "/apps/" + id, Entry: d}
	}
	root := mergeFixture(t, `<AppDir>/apps</AppDir>`+xml, nil)
	compiled, err := BuildMenuPlans(root, map[AppLocation]map[string]PoolEntry{{Path: "/apps"}: pool})
	if err != nil {
		t.Fatal(err)
	}
	allocated, err := AllocateMenus(compiled.Plan)
	if err != nil {
		t.Fatal(err)
	}
	got, err := PresentMenus(compiled, allocated, options)
	if err != nil {
		t.Fatal(err)
	}
	return got
}
func presentedLabels(menu *PresentedMenu) []string {
	var labels []string
	for _, item := range menu.Items {
		labels = append(labels, item.Label)
	}
	return labels
}
func displayDesktop(t *testing.T, text string) *Desktop {
	t.Helper()
	return parsedDesktop(t, "Exec=fixture-app\n"+text)
}

func TestMenuPresentationVisibilityAndArguments(t *testing.T) {
	entries := map[string]*Desktop{
		"a.desktop":         displayDesktop(t, "Name=Alpha & 100%\n"),
		"hidden.desktop":    displayDesktop(t, "Name=Hidden\nHidden=true\n"),
		"nodisplay.desktop": displayDesktop(t, "Name=NoDisplay\nNoDisplay=true\n"),
		"only.desktop":      displayDesktop(t, "Name=Only\nOnlyShowIn=FVWM;\n"),
		"not.desktop":       displayDesktop(t, "Name=Not\nNotShowIn=FVWM;\n"),
		"try.desktop":       displayDesktop(t, "Name=Unavailable\nTryExec=missing\n"),
		"locale.desktop":    displayDesktop(t, "Name=Zulu\nName[fr]=Bonjour\n"),
	}
	t.Run("visibility-after-allocation", func(t *testing.T) {
		got := presentationFixture(t, `<Include><All/></Include>`, entries, PresentationOptions{Desktops: []string{"FVWM"}})
		if !reflect.DeepEqual(presentedLabels(got), []string{"Alpha & 100%", "Only", "Zulu"}) {
			t.Fatal(got)
		}
	})
	t.Run("localized-order", func(t *testing.T) {
		got := presentationFixture(t, `<Include><All/></Include>`, entries, PresentationOptions{Desktops: []string{"FVWM"}, Locales: []string{"fr_FR.UTF-8"}})
		if !reflect.DeepEqual(presentedLabels(got), []string{"Alpha & 100%", "Bonjour", "Only"}) {
			t.Fatal(got)
		}
	})
	t.Run("tryexec-policy-without-launch", func(t *testing.T) {
		calls := 0
		got := presentationFixture(t, `<Include><All/></Include>`, entries, PresentationOptions{Desktops: []string{"FVWM"}, Executable: func(name string) bool { calls++; return name == "missing" }})
		if calls != 1 || len(got.Items) != 4 {
			t.Fatal(got, calls)
		}
	})
	t.Run("literal-codes-terminal-path", func(t *testing.T) {
		d := parsedDesktop(t, `Name=Title $value %c
Icon=app-icon
Exec=fixture-app "two words" "" %% %i %c %k %F
Terminal=true
Path=/working dir
`)
		got := presentationFixture(t, `<Include><All/></Include>`, map[string]*Desktop{"a.desktop": d}, PresentationOptions{})
		item := got.Items[0]
		if !reflect.DeepEqual(item.Argv, []string{"fixture-app", "two words", "", "%", "--icon", "app-icon", "Title $value %c", "/apps/a.desktop"}) || !item.Terminal || item.WorkingDir != "/working dir" {
			t.Fatal(item)
		}
		item.Argv[0] = "changed"
		if !strings.HasPrefix(d.Groups["Desktop Entry"]["Exec"], "fixture-app") {
			t.Fatal("mutated Desktop")
		}
	})
}

func TestMenuLayoutOrdering(t *testing.T) {
	entries := map[string]*Desktop{"a.desktop": displayDesktop(t, "Name=Alpha\n"), "b.desktop": displayDesktop(t, "Name=Beta\n"), "z.desktop": displayDesktop(t, "Name=Zulu\n")}
	cases := []struct {
		name, layout string
		want         []string
	}{
		{"default", "", []string{"Alpha", "Beta", "Zulu"}},
		{"explicit-then-merge", `<Layout><Filename>z.desktop</Filename><Merge type="files"/></Layout>`, []string{"Zulu", "Alpha", "Beta"}},
		{"reserve-later-explicit", `<Layout><Merge type="files"/><Filename>a.desktop</Filename></Layout>`, []string{"Beta", "Zulu", "Alpha"}},
		{"separator-normalization", `<Layout><Separator/><Filename>z.desktop</Filename><Separator/><Separator/><Filename>a.desktop</Filename><Separator/></Layout>`, []string{"Zulu", "", "Alpha"}},
		{"missing-and-duplicates", `<Layout><Filename>missing.desktop</Filename><Filename>z.desktop</Filename><Filename>z.desktop</Filename></Layout>`, []string{"Zulu"}},
		{"empty-layout-default", `<Layout/>`, []string{"Alpha", "Beta", "Zulu"}},
		{"last-layout", `<Layout><Filename>a.desktop</Filename></Layout><Layout><Filename>z.desktop</Filename></Layout>`, []string{"Zulu"}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			got := presentationFixture(t, `<Include><All/></Include>`+c.layout, entries, PresentationOptions{})
			if !reflect.DeepEqual(presentedLabels(got), c.want) {
				t.Fatal(presentedLabels(got), c.want)
			}
		})
	}
	t.Run("mixed-visual-caption-sort", func(t *testing.T) {
		got := presentationFixture(t, `<Include><Filename>b.desktop</Filename></Include><Menu><Name>Alpha</Name><Include><Filename>a.desktop</Filename></Include></Menu><Menu><Name>Zulu</Name><Include><Filename>z.desktop</Filename></Include></Menu><Layout><Merge type="all"/></Layout>`, entries, PresentationOptions{})
		if !reflect.DeepEqual(presentedLabels(got), []string{"Alpha", "Beta", "Zulu"}) || got.Items[0].Kind != SubmenuItem || got.Items[1].Kind != ApplicationItem {
			t.Fatal(got)
		}
	})
}

func TestMenuDirectoryMetadata(t *testing.T) {
	entry := displayDesktop(t, "Name=App\n")
	parse := func(text string) *Desktop {
		d, err := ParseDesktop([]byte("[Desktop Entry]\nType=Directory\n" + text))
		if err != nil {
			t.Fatal(err)
		}
		return d
	}
	options := PresentationOptions{Locales: []string{"fr"}, Directories: map[string]*Desktop{
		"/parent/a.directory": parse("Name=Parent\n"),
		"/child/a.directory":  parse("Name=Child\nName[fr]=Enfant\nIcon=child-icon\n"),
	}}
	t.Run("last-directory-and-dir-precedence", func(t *testing.T) {
		got := presentationFixture(t, `<DirectoryDir>/parent</DirectoryDir><DirectoryDir>/child</DirectoryDir><Directory>a.directory</Directory><Directory>missing.directory</Directory><Include><All/></Include>`, map[string]*Desktop{"a.desktop": entry}, options)
		if got.Label != "Enfant" || got.Icon != "child-icon" {
			t.Fatal(got)
		}
	})
	t.Run("hidden-directory", func(t *testing.T) {
		options.Directories["/child/hidden.directory"] = parse("Name=Hidden\nHidden=true\n")
		got := presentationFixture(t, `<DirectoryDir>/child</DirectoryDir><Directory>hidden.directory</Directory><Include><All/></Include>`, map[string]*Desktop{"a.desktop": entry}, options)
		if got != nil {
			t.Fatal(got)
		}
	})
	t.Run("fallback-to-name", func(t *testing.T) {
		got := presentationFixture(t, `<DirectoryDir>/absent</DirectoryDir><Directory>a.directory</Directory><Include><All/></Include>`, map[string]*Desktop{"a.desktop": entry}, options)
		if got.Label != "Root" {
			t.Fatal(got)
		}
	})
}

func TestMenuLayoutInlining(t *testing.T) {
	entry := displayDesktop(t, "Name=App\n")
	cases := []struct {
		name, attr string
		want       []string
		kind       ItemKind
	}{
		{"ordinary", "", []string{"Tools"}, SubmenuItem},
		{"inline-no-header", `inline="true" inline_header="false"`, []string{"App"}, ApplicationItem},
		{"inline-header", `inline="true"`, []string{"Tools", "App"}, HeaderItem},
		{"inline-alias", `inline="true" inline_alias="true"`, []string{"Tools"}, ApplicationItem},
		{"inline-unlimited", `inline="true" inline_limit="0" inline_header="false"`, []string{"App"}, ApplicationItem},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			got := presentationFixture(t, `<Menu><Name>Tools</Name><Include><All/></Include></Menu><Layout><Menuname `+c.attr+`>Tools</Menuname></Layout>`, map[string]*Desktop{"a.desktop": entry}, PresentationOptions{})
			if !reflect.DeepEqual(presentedLabels(got), c.want) || got.Items[0].Kind != c.kind {
				t.Fatal(got)
			}
		})
	}
	t.Run("inline-limit-submenu", func(t *testing.T) {
		got := presentationFixture(t, `<Menu><Name>Tools</Name><Include><All/></Include></Menu><Layout><Menuname inline="true" inline_limit="1">Tools</Menuname></Layout>`, map[string]*Desktop{"a.desktop": entry, "b.desktop": displayDesktop(t, "Name=Other\n")}, PresentationOptions{})
		if len(got.Items) != 1 || got.Items[0].Kind != SubmenuItem {
			t.Fatal(got)
		}
	})
	for _, show := range []string{"false", "true"} {
		t.Run("empty-"+show, func(t *testing.T) {
			got := presentationFixture(t, `<Menu><Name>Empty</Name></Menu><Layout><Menuname show_empty="`+show+`">Empty</Menuname></Layout>`, nil, PresentationOptions{})
			if (len(got.Items) != 0) != (show == "true") {
				t.Fatal(got)
			}
		})
	}
	t.Run("inherited-default-layout", func(t *testing.T) {
		got := presentationFixture(t, `<DefaultLayout inline="true" inline_header="false"><Merge type="files"/><Merge type="menus"/></DefaultLayout><Menu><Name>Tools</Name><Include><All/></Include></Menu>`, map[string]*Desktop{"a.desktop": entry}, PresentationOptions{})
		if len(got.Items) != 1 || got.Items[0].Kind != ApplicationItem {
			t.Fatal(got)
		}
	})
}

func TestMenuPresentationFailures(t *testing.T) {
	entry := displayDesktop(t, "Name=App\n")
	for _, layout := range []string{`<Layout><Merge type="unknown"/></Layout>`, `<DefaultLayout inline="yes"/>`, `<Layout><Menuname inline_limit="-1">Missing</Menuname></Layout>`, `<Directory>../escape.directory</Directory>`} {
		t.Run(layout, func(t *testing.T) {
			root := mergeFixture(t, `<Include><All/></Include>`+layout, nil)
			compiled, err := BuildMenuPlans(root, nil)
			if err != nil {
				t.Fatal(err)
			}
			allocated, err := AllocateMenus(compiled.Plan)
			if err != nil {
				t.Fatal(err)
			}
			got, err := PresentMenus(compiled, allocated, PresentationOptions{})
			if got != nil || err == nil {
				t.Fatal(got, err)
			}
		})
	}
	t.Run("tree-mismatch", func(t *testing.T) {
		got, err := PresentMenus(&CompiledMenu{Plan: &MenuPlan{Name: "A"}, Definition: &Node{Tag: "Menu"}}, &AllocatedMenu{Name: "B"}, PresentationOptions{})
		if got != nil || err == nil {
			t.Fatal(got, err)
		}
	})
	t.Run("bad-exec-diagnostic", func(t *testing.T) {
		entry.Groups["Desktop Entry"]["Exec"] = "app %q"
		root := mergeFixture(t, `<AppDir>/apps</AppDir><Include><All/></Include>`, nil)
		compiled, _ := BuildMenuPlans(root, map[AppLocation]map[string]PoolEntry{{Path: "/apps"}: {"a.desktop": {"/apps/a.desktop", entry}}})
		allocated, _ := AllocateMenus(compiled.Plan)
		got, err := PresentMenus(compiled, allocated, PresentationOptions{})
		if got != nil || err == nil || !strings.Contains(err.Error(), "/apps/a.desktop") {
			t.Fatal(got, err)
		}
	})
}

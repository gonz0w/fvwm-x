// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"bytes"
	"errors"
	"strings"
	"testing"
)

func rendererOptions() RenderOptions {
	return RenderOptions{Title: "XDGMenu", Titles: true, IncludeItems: "both", Regenerate: "PipeRead `fvwm-menu-desktop`", Configure: "PipeRead `fvwm-menu-desktop --configure`", Action: func(item PresentedItem) (string, error) { return "Exec exec fixture-app", nil }}
}
func rendererMenu() *PresentedMenu {
	child := &PresentedMenu{Name: "Tools", Label: "Tools & More", Items: []PresentedItem{{Kind: ApplicationItem, Label: "Café"}}}
	return &PresentedMenu{Name: "Applications", Items: []PresentedItem{{Kind: SubmenuItem, Label: child.Label, Menu: child}, {Kind: ApplicationItem, Label: "Alpha & 100%"}}}
}
func TestNativeMenuRendering(t *testing.T) {
	t.Run("ordinary-names-and-controls", func(t *testing.T) {
		var out bytes.Buffer
		err := RenderMenus(&out, []MenuDocument{{Menu: rendererMenu()}}, rendererOptions())
		if err != nil {
			t.Fatal(err)
		}
		text := out.String()
		for _, line := range []string{`DestroyMenu "XDGMenu"`, `AddToMenu "XDGMenu" "XDGMenu" Title`, `+ "Tools && More" Popup "Tools & More"`, `DestroyMenu "Tools & More"`, `+ "Alpha && 100%%" Exec exec fixture-app`, `+ "$[gt.Regenerate]" PipeRead ` + "`fvwm-menu-desktop`"} {
			if !strings.Contains(text, line+"\n") {
				t.Fatal(text, line)
			}
		}
		if strings.Count(text, "Configure") != 1 {
			t.Fatal(text)
		}
	})
	t.Run("dynamic-and-insert", func(t *testing.T) {
		for _, insert := range []string{"", "Existing"} {
			var out bytes.Buffer
			o := rendererOptions()
			o.Dynamic = true
			o.InsertInto = insert
			o.IncludeItems = "none"
			if err := RenderMenus(&out, []MenuDocument{{Menu: rendererMenu()}}, o); err != nil {
				t.Fatal(err)
			}
			if insert == "" && !strings.HasPrefix(out.String(), `DestroyMenu recreate "XDGMenu"`) {
				t.Fatal(out.String())
			}
			if insert != "" && !strings.HasPrefix(out.String(), "AddToMenu \"Existing\"\n") {
				t.Fatal(out.String())
			}
		}
	})
	t.Run("multiple-root-names", func(t *testing.T) {
		var out bytes.Buffer
		err := RenderMenus(&out, []MenuDocument{{Menu: rendererMenu(), Label: "Fixture Applications"}, {Menu: &PresentedMenu{Name: "Empty"}, Label: "Empty"}}, rendererOptions())
		if err != nil {
			t.Fatal(err)
		}
		if !strings.Contains(out.String(), `DestroyMenu "FvwmFixture Applications"`) || !strings.Contains(out.String(), `+ "Fixture Applications" Popup "FvwmFixture Applications"`) || strings.Contains(out.String(), "FvwmEmpty") {
			t.Fatal(out.String())
		}
	})
	t.Run("caption-literal-expansion", func(t *testing.T) {
		menu := &PresentedMenu{Name: "Applications", Items: []PresentedItem{{Kind: ApplicationItem, Label: "quote\" $[value]\\ & % * ^ @\nnext"}}}
		var out bytes.Buffer
		if err := RenderMenus(&out, []MenuDocument{{Menu: menu}}, rendererOptions()); err != nil {
			t.Fatal(err)
		}
		if !strings.Contains(out.String(), `quote\" $$[value]\\ && %% ** ^^ @@ next`) {
			t.Fatal(out.String())
		}
	})
	t.Run("icon-resolution", func(t *testing.T) {
		var out bytes.Buffer
		o := rendererOptions()
		o.Icons = true
		o.Icon = func(name string, directory bool) (string, error) { return "/icons/app.svg:24x24", nil }
		if err := RenderMenus(&out, []MenuDocument{{Menu: rendererMenu()}}, o); err != nil || !strings.Contains(out.String(), "%/icons/app.svg:24x24%") {
			t.Fatal(out.String(), err)
		}
	})
}

type renderFailedWriter struct{}

func (renderFailedWriter) Write([]byte) (int, error) { return 0, errors.New("writer failed") }
func TestNativeMenuRenderingFailure(t *testing.T) {
	if err := RenderMenus(renderFailedWriter{}, []MenuDocument{{Menu: rendererMenu()}}, rendererOptions()); err == nil {
		t.Fatal("lost writer error")
	}
	for _, action := range []string{"Exec first\nExec second", "bad\x00action"} {
		t.Run(action, func(t *testing.T) {
			var out bytes.Buffer
			o := rendererOptions()
			o.Action = func(PresentedItem) (string, error) { return action, nil }
			if err := RenderMenus(&out, []MenuDocument{{Menu: rendererMenu()}}, o); err == nil {
				t.Fatal("accepted multiline action")
			}
		})
	}
	t.Run("icon-delimiter", func(t *testing.T) {
		var out bytes.Buffer
		o := rendererOptions()
		o.Icons = true
		o.Icon = func(string, bool) (string, error) { return "/bad%icon", nil }
		if err := RenderMenus(&out, []MenuDocument{{Menu: rendererMenu()}}, o); err == nil {
			t.Fatal("accepted icon delimiter")
		}
	})
	t.Run("cyclic-render-tree", func(t *testing.T) {
		menu := &PresentedMenu{Name: "Loop", Label: "Loop"}
		menu.Items = []PresentedItem{{Kind: SubmenuItem, Label: "Loop", Menu: menu}}
		var out bytes.Buffer
		if err := RenderMenus(&out, []MenuDocument{{Menu: menu}}, rendererOptions()); err == nil {
			t.Fatal("accepted cyclic menu")
		}
	})
	t.Run("output-budget", func(t *testing.T) {
		s := renderState{out: &bytes.Buffer{}, bytes: 2, items: 10}
		if err := s.line("too long\n"); err == nil {
			t.Fatal("exceeded output limit")
		}
	})
}

func TestNativeMenuNameCollisions(t *testing.T) {
	child := func(label string) PresentedItem {
		return PresentedItem{Kind: SubmenuItem, Label: label, Menu: &PresentedMenu{Label: label, Items: []PresentedItem{{Kind: ApplicationItem, Label: "App"}}}}
	}
	for _, documents := range [][]MenuDocument{
		{{Menu: &PresentedMenu{Items: []PresentedItem{child("Same"), child("Same"), child("XDGMenu")}}}},
		{{Label: "Same", Menu: &PresentedMenu{Items: []PresentedItem{child("Tools")}}}, {Label: "Same", Menu: &PresentedMenu{Items: []PresentedItem{child("Tools")}}}},
	} {
		var out bytes.Buffer
		if err := RenderMenus(&out, documents, rendererOptions()); err != nil {
			t.Fatal(err)
		}
		definitions := make(map[string]bool)
		var references []string
		for _, line := range strings.Split(out.String(), "\n") {
			if strings.HasPrefix(line, "DestroyMenu ") {
				name := strings.TrimPrefix(line, "DestroyMenu ")
				if definitions[name] {
					t.Fatal("overwritten menu", name, out.String())
				}
				definitions[name] = true
			}
			if _, name, ok := strings.Cut(line, " Popup "); ok {
				references = append(references, name)
			}
		}
		for _, name := range references {
			if !definitions[name] {
				t.Fatal("dangling menu", name, out.String())
			}
		}
		if len(definitions) != len(references)+1 {
			t.Fatal("menu references collapsed", out.String())
		}
	}
}

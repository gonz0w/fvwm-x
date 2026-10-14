// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"strings"
	"testing"
)

func TestMenuXMLPreservesOrderAndMetadata(t *testing.T) {
	data := `<?xml version="1.0"?><!DOCTYPE Menu PUBLIC "-//freedesktop//DTD Menu 1.0//EN" "https://example.invalid/menu.dtd">
<Menu><Name>Applications</Name><MergeFile type="parent">parent.menu</MergeFile>
<Layout><Filename>one.desktop</Filename><Separator/><Menuname inline="true">Tools</Menuname></Layout>
<Extension hint="value">one<!-- split -->two</Extension></Menu>`
	root, err := ParseMenuXML([]byte(data))
	if err != nil || root.Tag != "Menu" || len(root.Children) != 4 {
		t.Fatal(root, err)
	}
	if root.Children[1].Tag != "MergeFile" || root.Children[1].Attr["type"] != "parent" ||
		root.Children[2].Children[1].Tag != "Separator" || root.Children[3].Text != "onetwo" {
		t.Fatal(root)
	}
}

func TestMenuXMLRefusalsAndBounds(t *testing.T) {
	cases := map[string]string{
		"empty":               "",
		"wrong-root":          "<Applications/>",
		"two-roots":           "<Menu/><Menu/>",
		"incomplete":          "<Menu><Name>one</Menu>",
		"text-outside":        "text<Menu/>",
		"unknown-entity":      "<!DOCTYPE Menu [<!ENTITY label 'Fixture'>]><Menu><Name>&label;</Name></Menu>",
		"namespace":           `<Menu xmlns="urn:fixture"/>`,
		"duplicate-attribute": `<Menu><MergeFile type="path" type="parent"/></Menu>`,
		"long-value":          "<Menu><Name>" + strings.Repeat("x", MaxValueBytes+1) + "</Name></Menu>",
		"long-attribute":      `<Menu hint="` + strings.Repeat("x", MaxValueBytes+1) + `"/>`,
		"long-source":         strings.Repeat(" ", MaxSourceBytes+1),
		"deep":                strings.Repeat("<Menu>", MaxDepth+1) + strings.Repeat("</Menu>", MaxDepth+1),
		"many-nodes":          "<Menu>" + strings.Repeat("<All/>", MaxItems) + "</Menu>",
	}
	for name, data := range cases {
		t.Run(name, func(t *testing.T) {
			if root, err := ParseMenuXML([]byte(data)); err == nil || root != nil {
				t.Fatal("accepted malformed/excessive XML", err)
			}
		})
	}
	for name, data := range map[string]string{
		"depth-boundary": strings.Repeat("<Menu>", MaxDepth) + strings.Repeat("</Menu>", MaxDepth),
		"value-boundary": "<Menu><Name>" + strings.Repeat("x", MaxValueBytes) + "</Name></Menu>",
	} {
		t.Run(name, func(t *testing.T) {
			if _, err := ParseMenuXML([]byte(data)); err != nil {
				t.Fatal(err)
			}
		})
	}
}

func TestMenuRulePolicy(t *testing.T) {
	cases := []struct {
		name, body string
		matched    bool
	}{
		{"filename", "<Filename>one.desktop</Filename>", true},
		{"other-file", "<Filename>two.desktop</Filename>", false},
		{"category", "<Category>Utility</Category>", true},
		{"other-category", "<Category>Game</Category>", false},
		{"and", "<And><Category>Utility</Category><Filename>one.desktop</Filename></And>", true},
		{"and-false", "<And><Category>Game</Category><Filename>one.desktop</Filename></And>", false},
		{"or", "<Or><Category>Game</Category><Filename>one.desktop</Filename></Or>", true},
		{"not", "<Not><Filename>two.desktop</Filename><Category>Game</Category></Not>", true},
		{"not-false", "<Not><Filename>two.desktop</Filename><Category>Utility</Category></Not>", false},
		{"all", "<All/>", true},
		{"empty-or", "<Or/>", false},
		{"empty-and", "<And/>", true},
		{"empty-not", "<Not/>", true},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			root, err := ParseMenuXML([]byte("<Menu><Include>" + c.body + "</Include></Menu>"))
			if err != nil {
				t.Fatal(err)
			}
			matched, err := MatchRule(root.Children[0], "one.desktop", []string{"Utility", "Text"})
			if err != nil || matched != c.matched {
				t.Fatal(matched, err)
			}
		})
	}
	for _, rule := range []*Node{nil, {Tag: "Unknown"}, {Tag: "Filename"}, {Tag: "Category"}, {Tag: "All", Children: []*Node{{Tag: "All"}}}} {
		if matched, err := MatchRule(rule, "one.desktop", nil); err == nil || matched {
			t.Fatal("accepted malformed rule", matched, err)
		}
	}
	cycle := &Node{Tag: "Not"}
	cycle.Children = []*Node{cycle}
	if _, err := MatchRule(cycle, "one.desktop", nil); err == nil {
		t.Fatal("cyclic rule was not refused")
	}
	for name, categories := range map[string][]string{
		"input-count": make([]string, MaxItems+1),
		"work-count":  make([]string, MaxItems),
	} {
		t.Run(name, func(t *testing.T) {
			if _, err := MatchRule(&Node{Tag: "Category", Text: "Utility"}, "one.desktop", categories); err == nil {
				t.Fatal("unbounded category work accepted")
			}
		})
	}
	id := "\u00a0one.desktop"
	if matched, err := MatchRule(&Node{Tag: "Filename", Text: id}, id, nil); err != nil || !matched {
		t.Fatal("non-ASCII identity whitespace changed", matched, err)
	}
}

func BenchmarkDesktopParse(b *testing.B) {
	data := []byte("[Desktop Entry]\nType=Application\nName=Fixture\nExec=fixture %U\nCategories=Utility;\n")
	b.ReportAllocs()
	for i := 0; i < b.N; i++ {
		if _, err := ParseDesktop(data); err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkExecExpand(b *testing.B) {
	context := ExecContext{Name: "Café Menu", Icon: "fixture", Location: "/fixture/app.desktop"}
	b.ReportAllocs()
	for i := 0; i < b.N; i++ {
		if _, err := ExpandExec("fixture %i %c %k %U", context); err != nil {
			b.Fatal(err)
		}
	}
}

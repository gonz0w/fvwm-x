// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"fmt"
	"reflect"
	"strings"
	"testing"
)

func TestNormalizeMIMEType(t *testing.T) {
	for _, value := range []string{"text/plain", "Application/Vnd.Foo+XML", "text/x-c++src", "inode/directory"} {
		got, err := NormalizeMIMEType(value)
		if err != nil || got != strings.ToLower(value) {
			t.Fatal(value, got, err)
		}
	}
	for _, value := range []string{"", "text", "/plain", "text/", "text/plain/extra", "text/*", "*/plain", "text/plain; charset=utf-8", " text/plain", "text/plain\n", "text/\x00plain", "text/pläin", strings.Repeat("x", 256) + "/plain"} {
		if _, err := NormalizeMIMEType(value); err == nil {
			t.Errorf("accepted invalid MIME type %q", value)
		}
	}
}

func TestMIMETypeGraphPrecedenceAndSpecificity(t *testing.T) {
	graph, err := NewMIMETypeGraph([]MIMETypeSource{
		{Aliases: []byte("application/x-old application/x-leaf\n"), Subclasses: []byte("application/x-leaf application/x-z\napplication/x-leaf application/x-a\napplication/x-a application/x-root\napplication/x-z application/x-root\napplication/x-leaf application/x-root\n")},
		{Aliases: []byte("application/x-old application/x-wrong\napplication/x-chain application/x-old\n")},
	})
	if err != nil {
		t.Fatal(err)
	}
	got, err := graph.Types("APPLICATION/X-CHAIN")
	want := []string{"application/x-leaf", "application/x-a", "application/x-z", "application/x-root", "application/octet-stream"}
	if err != nil || !reflect.DeepEqual(got, want) {
		t.Fatal("alias precedence, subtype order or lexical tie lost", got, err)
	}
	got, err = graph.Types("image/unknown")
	if err != nil || !reflect.DeepEqual(got, []string{"image/unknown", "application/octet-stream"}) {
		t.Fatal("standard binary fallback lost", got, err)
	}
	got, err = graph.Types("text/x-custom")
	if err != nil || !reflect.DeepEqual(got, []string{"text/x-custom", "text/plain", "application/octet-stream"}) {
		t.Fatal("standard text fallback lost", got, err)
	}
	got, err = graph.Types("inode/directory")
	if err != nil || !reflect.DeepEqual(got, []string{"inode/directory"}) {
		t.Fatal("inode type acquired a binary fallback", got, err)
	}
}

func TestMIMETypeGraphRejectsInvalidCyclesAndFloods(t *testing.T) {
	cases := []MIMETypeSource{
		{Aliases: []byte("not-a-type text/plain\n")},
		{Aliases: []byte("text/x-a text/x-b extra\n")},
		{Aliases: []byte("text/x-a text/x-b\ntext/x-b text/x-a\n")},
		{Subclasses: []byte("text/x-a text/x-b\ntext/x-b text/x-a\n")},
		{Aliases: []byte("text/x-old text/plain\n"), Subclasses: []byte("text/x-old text/plain\n")},
		{Aliases: []byte(strings.Repeat("#", MaxMIMETypeSourceBytes+1))},
		{Subclasses: []byte("text/plain text/\x00bad\n")},
	}
	var aliases, edges, deep strings.Builder
	for i := 0; i <= MaxMIMETypeAliases; i++ {
		fmt.Fprintf(&aliases, "text/x-old%d text/plain\n", i)
	}
	for i := 0; i <= MaxMIMETypeEdges; i++ {
		fmt.Fprintf(&edges, "text/x-child%d text/plain\n", i)
	}
	for i := 0; i < MaxMIMETypeDepth; i++ {
		fmt.Fprintf(&deep, "text/x-%d text/x-%d\n", i, i+1)
	}
	cases = append(cases, MIMETypeSource{Aliases: []byte(aliases.String())}, MIMETypeSource{Subclasses: []byte(edges.String())}, MIMETypeSource{Subclasses: []byte(deep.String())})
	for i, source := range cases {
		if _, err := NewMIMETypeGraph([]MIMETypeSource{source}); err == nil {
			t.Errorf("invalid graph %d accepted", i)
		}
	}
	var wide strings.Builder
	for i := 0; i < MaxMIMETypeReachable; i++ {
		fmt.Fprintf(&wide, "text/x-root text/x-parent%d\n", i)
	}
	graph, err := NewMIMETypeGraph([]MIMETypeSource{{Subclasses: []byte(wide.String())}})
	if err != nil {
		t.Fatal(err)
	}
	if _, err := graph.Types("text/x-root"); err == nil {
		t.Fatal("reachable type flood accepted")
	}
}

func TestApplicationMIMEAppsParserNormalizesAliasesAndScopesTypes(t *testing.T) {
	graph, err := NewMIMETypeGraph([]MIMETypeSource{{Aliases: []byte("text/x-old text/plain\n")}})
	if err != nil {
		t.Fatal(err)
	}
	got, err := ParseApplicationMIMEApps([]byte("[Default Applications]\nTEXT/X-OLD=a.desktop;\nimage/png=b.desktop;\n[Added Associations]\ntext/plain=c.desktop;\n[Removed Associations]\nimage/png=c.desktop;\n"), graph)
	if err != nil || !reflect.DeepEqual(got["text/plain"], DirectoryMIMEApps{Defaults: []string{"a.desktop"}, Added: []string{"c.desktop"}}) || !reflect.DeepEqual(got["image/png"].Removed, []string{"c.desktop"}) {
		t.Fatal(got, err)
	}
	for _, raw := range []string{
		"[Default Applications]\ntext/x-old=a.desktop;\ntext/plain=b.desktop;\n",
		"[Added Associations]\ntext/plain=a.desktop;\n[Removed Associations]\ntext/x-old=a.desktop;\n",
		"[Added Associations]\ntext/*=a.desktop;\n",
	} {
		if _, err := ParseApplicationMIMEApps([]byte(raw), graph); err == nil {
			t.Fatal("malformed or conflicting canonical settings accepted", raw)
		}
	}
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package actionpalette

import (
	"fmt"
	"strings"
	"testing"
)

func providers() [4]Provider {
	var p [4]Provider
	for i := range p {
		p[i] = Provider{Version: "v1", State: "observed", Coverage: "test"}
	}
	return p
}
func row(kind Kind, id string) Row {
	return Row{ID: id, Label: id, Kind: kind, Version: "v1", Icon: "other", Enabled: true}
}
func TestBoundsAndAtomicReplacement(t *testing.T) {
	var m Model
	p := providers()
	kinds := []Kind{Window, Desktop, Application, Registered}
	limits := []int{512, 128, 500, 16}
	for i, n := range limits {
		for j := 0; j < n; j++ {
			p[i].Rows = append(p[i].Rows, row(kinds[i], fmt.Sprint(j)))
		}
	}
	if err := m.Replace(p); err != nil {
		t.Fatal(err)
	}
	if len(m.Collection.Entries) != MaxRows {
		t.Fatal("combined bound")
	}
	generation := m.Collection.Generation
	p[2].Rows = append(p[2].Rows, row(Application, "overflow"))
	if m.Replace(p) == nil || m.Collection.Generation != generation {
		t.Fatal("overlimit replacement mutated model")
	}
}
func TestIdentityLossFilterAndStaleAction(t *testing.T) {
	var m Model
	p := providers()
	p[0].Rows = []Row{row(Window, "opaque-one"), row(Window, "opaque-two")}
	p[0].Rows[0].Label = "Café $ literal"
	if err := m.Replace(p); err != nil {
		t.Fatal(err)
	}
	id := ID(Window, "opaque-one")
	g := m.Collection.Generation
	m.Collection.Focus = id
	_ = m.Collection.SelectIDs(g, []string{id})
	if err := m.Filter("CAFÉ $"); err != nil || m.Collection.VisibleCount() != 1 {
		t.Fatal("literal case insensitive filter", err)
	}
	if _, err := m.Resolve(g, id); err == nil {
		t.Fatal("stale projection acted")
	}
	if _, err := m.Resolve(m.Collection.Generation, id); err != nil {
		t.Fatal(err)
	}
	p[0].State = "unavailable"
	p[0].Reason = "Core lost"
	if err := m.Replace(p); err != nil {
		t.Fatal(err)
	}
	if _, err := m.Resolve(m.Collection.Generation, id); err == nil {
		t.Fatal("lost provider acted")
	}
	if m.Collection.Focus != id || !m.Collection.Selected[id] {
		t.Fatal("surviving selection lost")
	}
	p[0].Rows = nil
	_ = m.Replace(p)
	if m.Collection.Selected[id] {
		t.Fatal("removed selection retained")
	}
	if m.Filter(strings.Repeat("é", 65)) == nil {
		t.Fatal("UTF8 byte limit")
	}
}
func TestRegisteredCapabilitiesAreFinite(t *testing.T) {
	r := Registry()
	if len(r) > MaxActions || len(r) != 8 {
		t.Fatal(r)
	}
	if Allowed(Intent("quit"), map[string]bool{"desktop": true}) {
		t.Fatal("unregistered action")
	}
	if Allowed(Terminal, map[string]bool{"desktop": true}) {
		t.Fatal("missing capability")
	}
	if !Allowed(Terminal, map[string]bool{"terminal": true}) {
		t.Fatal("terminal unavailable")
	}
}

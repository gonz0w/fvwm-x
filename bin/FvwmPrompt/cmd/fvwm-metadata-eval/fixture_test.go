//go:build fvwm_metadata_eval

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"encoding/json"
	"fmt"
	"path/filepath"
	"reflect"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
)

func TestFrozenPublicOracle(t *testing.T) {
	e, err := corpus(12)
	if err != nil {
		t.Fatal(err)
	}
	// These IDs are authored independently from backend query output.
	cases := []struct {
		filter string
		ids    []string
	}{
		{"", []string{"meta-0001", "meta-0005", "meta-0009", "meta-0000", "meta-0002", "meta-0010", "meta-0011", "meta-0008", "meta-0007", "meta-0006", "meta-0003", "meta-0004"}},
		{"ALPHA", []string{"meta-0000"}},
		{"σigma", []string{"meta-0003"}},
		{"[100%]_", []string{"meta-0010"}},
		{`\xff`, []string{"meta-0005", "meta-0004"}},
		{"absent", []string{}},
	}
	for _, tc := range cases {
		got, err := baseline(e, query{Order: "name", Filter: tc.filter})
		if err != nil || got.Generation != 41 || !reflect.DeepEqual(got.IDs, tc.ids) {
			t.Fatalf("filter %q: %+v, %v; want %v", tc.filter, got, err, tc.ids)
		}
	}
	labels := e.Snapshot.Entries()
	if labels[4].Label != labels[5].Label || labels[4].ID == labels[5].ID {
		t.Fatal("corpus lost distinct identities with equal display labels")
	}
}

func TestSnapshotAndBounds(t *testing.T) {
	for _, count := range []int{0, 12, 256, 4096} {
		e, err := corpus(count)
		if err != nil {
			t.Fatal(err)
		}
		root := t.TempDir()
		if err := saveSnapshot(root, e); err != nil {
			t.Fatal(err)
		}
		got, err := loadSnapshot(filepath.Join(root, "snapshot.json"))
		if err != nil || !reflect.DeepEqual(e, got) {
			t.Fatalf("snapshot size %d: %v", count, err)
		}
	}
	if _, err := corpus(4097); err == nil {
		t.Fatal("unbounded corpus admitted")
	}
	oversize, _ := corpus(4096)
	extra := oversize.Snapshot.Items[0]
	extra.ID = "extra-distinct-id"
	oversize.Snapshot.Items = append(oversize.Snapshot.Items, extra)
	if err := validateEnvelope(oversize); err == nil {
		t.Fatal("4097 distinct records admitted")
	}
	e, _ := corpus(12)
	e.Snapshot.Items = append(e.Snapshot.Items, e.Snapshot.Items[0])
	if err := validateEnvelope(e); err == nil {
		t.Fatal("duplicate domain identity admitted")
	}
	for _, data := range [][]byte{nil, []byte(`{`), []byte(`{"probe_version":2}`), []byte(`{} {}`)} {
		if _, err := decodeEnvelope(data); err == nil {
			t.Fatal("malformed cache admitted")
		}
	}
}

func TestReconcileGenerationOracle(t *testing.T) {
	e, _ := corpus(256)
	var m collection.Model
	if err := m.Replace(e.Snapshot.Entries()); err != nil {
		t.Fatal(err)
	}
	generation := m.Generation
	if changes, err := m.Reconcile(generation, e.Snapshot.Entries()); err != nil || changes != (collection.Changes{}) || m.Generation != generation {
		t.Fatalf("no-op changed generation: %+v %v", changes, err)
	}
	e.Snapshot.Items[0].Identity.Size++
	if changes, err := m.Reconcile(generation, e.Snapshot.Entries()); err != nil || changes.Updated != 1 || m.Generation != generation+1 {
		t.Fatalf("metadata update: %+v %v", changes, err)
	}
	if _, err := m.Reconcile(generation, e.Snapshot.Entries()); err == nil {
		t.Fatal("stale generation admitted")
	}
	e, _ = corpus(4096)
	if changes, err := m.Reconcile(m.Generation, e.Snapshot.Entries()); err != nil || !changes.Replaced {
		t.Fatalf("large replacement: %+v %v", changes, err)
	}
	if _, err := json.Marshal(e); err != nil {
		t.Fatal(err)
	}
}

func TestFrozenSortAndUpdateOrders(t *testing.T) {
	e, err := corpus(12)
	if err != nil {
		t.Fatal(err)
	}
	cases := []struct {
		key     string
		reverse bool
		order   []int
	}{
		{"name", false, []int{1, 5, 9, 0, 2, 10, 11, 8, 7, 6, 3, 4}},
		{"name", true, []int{1, 4, 3, 6, 7, 8, 11, 10, 2, 0, 9, 5}},
		{"kind", false, []int{1, 5, 0, 2, 10, 11, 7, 6, 3, 4, 9, 8}},
		{"kind", true, []int{1, 8, 9, 4, 3, 6, 7, 11, 10, 2, 0, 5}},
		{"size", false, []int{1, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
		{"size", true, []int{1, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 0}},
		{"modified", false, []int{1, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 0}},
		{"modified", true, []int{1, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
	}
	for _, tc := range cases {
		ids := []string{}
		for _, i := range tc.order {
			ids = append(ids, fmt.Sprintf("meta-%04d", i))
		}
		got, err := baseline(e, query{Order: tc.key, Reverse: tc.reverse})
		if err != nil || !reflect.DeepEqual(got.IDs, ids) {
			t.Fatalf("%s reverse=%t: %+v %v; want %v", tc.key, tc.reverse, got, err, ids)
		}
	}
	memory, err := newMemory(e)
	if err != nil {
		t.Fatal(err)
	}
	if _, err = memory.query(query{Order: "name"}); err != nil {
		t.Fatal(err)
	}
	generation := memory.model.Generation
	if next, changed, err := memory.staged(e); err != nil || changed || next != memory || next.model.Generation != generation {
		t.Fatal("no-op changed view", err)
	}
	nextEnvelope := changed(e, false)
	next, changedFlag, err := memory.staged(nextEnvelope)
	if err != nil || !changedFlag || memory.source.Generation != 41 || next.source.Generation != 42 {
		t.Fatal("staged update was not atomic", err)
	}
	got, err := next.query(query{Order: "name"})
	want := []string{"meta-0001", "meta-0005", "meta-0009", "meta-0000", "meta-0002", "meta-0010", "replacement-0011", "meta-0008", "meta-0007", "meta-0006", "meta-0003", "meta-0004"}
	if err != nil || got.Generation != 42 || !reflect.DeepEqual(got.IDs, want) {
		t.Fatal("updated oracle", got, err)
	}
	if _, _, err := next.staged(e); err == nil {
		t.Fatal("stale generation admitted")
	}
	wrong := changed(nextEnvelope, true)
	wrong.Generation = nextEnvelope.Generation
	if _, _, err := next.staged(wrong); err == nil {
		t.Fatal("changed bytes under unchanged generation admitted")
	}
}

func TestFrozenASCIITokenOracle(t *testing.T) {
	e, err := corpus(12)
	if err != nil {
		t.Fatal(err)
	}
	for _, tc := range []struct {
		prefix string
		ids    []string
	}{
		{"alp", []string{"meta-0000"}}, {"ig", []string{"meta-0003"}},
		{"pipe", []string{"meta-0009"}}, {"item", []string{"meta-0010"}},
		{"sum", []string{"meta-0007"}}, {"absent", []string{}},
	} {
		if got := tokenScan(e.Snapshot.Entries(), tc.prefix); !reflect.DeepEqual(got, tc.ids) {
			t.Fatal(tc.prefix, got, tc.ids)
		}
	}
}

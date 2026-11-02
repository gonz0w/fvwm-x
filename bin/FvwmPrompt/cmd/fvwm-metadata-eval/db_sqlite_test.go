//go:build fvwm_metadata_eval && fvwm_sqlite_eval && cgo

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"path/filepath"
	"reflect"
	"testing"
)

func TestSQLiteFullMetadataAndLiteralParity(t *testing.T) {
	e, err := corpus(12)
	if err != nil {
		t.Fatal(err)
	}
	e.Generation = ^uint64(0) - 1
	e.Snapshot.Items[0].Identity.Device = ^uint64(0)
	e.Snapshot.Items[0].Identity.Inode = ^uint64(0) - 2
	e.Snapshot.Items[0].Identity.Size = -1 << 63
	e.Snapshot.Items[0].Identity.MTime = 1<<63 - 1
	path := filepath.Join(t.TempDir(), "metadata.db")
	d, err := openSQLite(path, false, true)
	if err != nil {
		t.Fatal(err)
	}
	defer func() {
		if err := d.close(); err != nil {
			t.Error(err)
		}
	}()
	if err := d.load(e); err != nil {
		t.Fatal(err)
	}
	if err := d.load(e); err != nil {
		t.Fatal("no-op", err)
	}
	for _, q := range workloadQueries() {
		want, err := baseline(e, q)
		if err != nil {
			t.Fatal(err)
		}
		got, err := d.query(q)
		if err != nil || !reflect.DeepEqual(got, want) {
			t.Fatal("literal parity", q, got, want, err)
		}
	}
	if err := d.close(); err != nil {
		t.Fatal(err)
	}
	d, err = openSQLite(path, false, false)
	if err != nil {
		t.Fatal(err)
	}
	if err := d.reopen(); err != nil {
		t.Fatal(err)
	}
	got, err := d.readback()
	if err != nil || !reflect.DeepEqual(got, e) {
		t.Fatal("complete metadata after reopen", err)
	}
	if err := d.load(changed(e, true)); err != nil {
		t.Fatal("maximum generation", err)
	}
	if err := d.load(e); err == nil {
		t.Fatal("stale generation accepted")
	}
}

func TestSQLiteFailureControls(t *testing.T) {
	rows, err := databaseControls(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	if len(rows) != 9 {
		t.Fatalf("incomplete fixed controls: %+v", rows)
	}
	for _, row := range rows {
		if row.Status != "passed" {
			t.Fatal(row)
		}
	}
}

func TestSQLiteASCIITokenWorkload(t *testing.T) {
	e, err := corpus(12)
	if err != nil {
		t.Fatal(err)
	}
	d, err := openSQLite(filepath.Join(t.TempDir(), "metadata.db"), false, true)
	if err != nil {
		t.Fatal(err)
	}
	defer func() {
		if err := d.close(); err != nil {
			t.Error(err)
		}
	}()
	if err := d.load(e); err != nil {
		t.Fatal(err)
	}
	available, err := d.ftsBuild(e)
	if err != nil {
		t.Fatal(err)
	}
	if !available {
		t.Skip("runtime FTS5 unavailable; not token-query qualification")
	}
	for _, prefix := range []string{"alp", "ig", "pipe", "item", "sum", "absent"} {
		want := tokenScan(e.Snapshot.Entries(), prefix)
		got, err := d.ftsQuery(prefix)
		if err != nil || !reflect.DeepEqual(got, want) {
			t.Fatal(prefix, got, want, err)
		}
	}
}

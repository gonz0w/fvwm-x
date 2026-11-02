//go:build fvwm_metadata_eval && fvwm_sqlite_eval && cgo

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strconv"
	"strings"
)

func sqliteCode(err error, expected int) bool {
	var failure *sqliteFailure
	return errors.As(err, &failure) && failure.Code == expected
}

func databaseControls(root string) (rows []control, err error) {
	e, err := corpus(12)
	if err != nil {
		return nil, err
	}
	path := filepath.Join(root, "failure.db")
	d, err := openSQLite(path, false, true)
	if err != nil {
		return nil, err
	}
	defer func() { err = errors.Join(err, d.close()) }()
	if err := d.load(e); err != nil {
		return nil, err
	}
	q := query{Order: "name"}
	want, _ := baseline(e, q)
	unchanged := func() error {
		got, err := d.query(q)
		if err != nil || !reflect.DeepEqual(got, want) {
			return fmt.Errorf("failure changed publication: %+v: %v", got, err)
		}
		stored, readErr := d.readback()
		if readErr != nil || !reflect.DeepEqual(stored, e) {
			return fmt.Errorf("failure changed complete metadata: %v", readErr)
		}
		return nil
	}
	next := e
	next.Generation++
	other, err := openSQLite(path, false, false)
	if err != nil {
		return nil, err
	}
	if err := other.exec("BEGIN IMMEDIATE"); err != nil {
		other.close()
		return nil, err
	}
	busy := d.load(next)
	cleanup := errors.Join(other.exec("ROLLBACK"), other.close())
	if !sqliteCode(busy, 5) || cleanup != nil {
		return nil, fmt.Errorf("busy control: %v, cleanup: %v", busy, cleanup)
	}
	if err := unchanged(); err != nil {
		return nil, err
	}
	rows = append(rows, control{"busy", "passed", "two connections; immediate refusal; generation retained"})

	ro, err := openSQLite(path, true, false)
	if err != nil {
		return nil, err
	}
	readonly := ro.load(next)
	if closeErr := ro.close(); !sqliteCode(readonly, 8) || closeErr != nil {
		return nil, fmt.Errorf("read-only control: %v, close: %v", readonly, closeErr)
	}
	if err := unchanged(); err != nil {
		return nil, err
	}
	rows = append(rows, control{"read-only", "passed", "explicit read-only connection; no chmod-based inference"})

	if err := d.exec("CREATE TABLE probe_full (value BLOB)"); err != nil {
		return nil, err
	}
	ceiling, err := d.scalar("PRAGMA max_page_count")
	if err != nil || ceiling <= 0 {
		return nil, errors.New("private page ceiling missing")
	}
	pages, err := d.scalar("PRAGMA page_count")
	if err != nil || pages <= 0 || pages > ceiling {
		return nil, errors.New("invalid full-control page count")
	}
	actual, err := d.scalar("PRAGMA max_page_count=" + strconv.FormatInt(pages, 10))
	if err != nil || actual != pages {
		return nil, errors.New("full-control page ceiling not established")
	}
	full := d.exec("INSERT INTO probe_full VALUES(zeroblob(1048576))")
	if !sqliteCode(full, 13) {
		return nil, fmt.Errorf("expected bounded SQLITE_FULL: %v", full)
	}
	if err := unchanged(); err != nil {
		return nil, err
	}
	if actual, err := d.scalar("PRAGMA max_page_count=" + strconv.FormatInt(ceiling, 10)); err != nil || actual != ceiling {
		return nil, errors.New("could not restore private page ceiling")
	}
	rows = append(rows, control{"full", "passed", fmt.Sprintf("actual page ceiling %d; no filesystem exhaustion", pages)})
	// Exercise the actual replacement path after DELETE and partial INSERT,
	// not just an unrelated engine-full statement. The old metadata and
	// generation must remain readable after the failed transaction.
	large, buildErr := corpus(4096)
	if buildErr != nil {
		return nil, buildErr
	}
	large.Generation = next.Generation
	if actual, err := d.scalar("PRAGMA max_page_count=" + strconv.FormatInt(pages, 10)); err != nil || actual != pages {
		return nil, errors.New("replacement page ceiling not established")
	}
	failure := d.load(large)
	var progress *loadFailure
	if !sqliteCode(failure, 13) || !errors.As(failure, &progress) || !progress.Deleted || progress.Inserted == 0 || progress.Inserted >= 4096 {
		return nil, fmt.Errorf("expected partial replacement SQLITE_FULL: %v", failure)
	}
	if err := unchanged(); err != nil {
		return nil, err
	}
	restored, readErr := d.readback()
	if readErr != nil || !reflect.DeepEqual(restored, e) {
		return nil, errors.New("full replacement changed previous metadata")
	}
	if actual, err := d.scalar("PRAGMA max_page_count=" + strconv.FormatInt(ceiling, 10)); err != nil || actual != ceiling {
		return nil, errors.New("replacement ceiling restore failed")
	}
	if err := d.load(large); err != nil {
		return nil, err
	}
	if err := d.load(envelope{Version: e.Version, Generation: large.Generation + 1, Snapshot: e.Snapshot}); err != nil {
		return nil, err
	}
	e.Generation = large.Generation + 1
	next = e
	next.Generation++
	want, _ = baseline(e, q)
	if err := unchanged(); err != nil {
		return nil, err
	}
	rows = append(rows, control{"full-replacement", "passed", fmt.Sprintf("DELETE and %d inserts precede full refusal; prior metadata survives; restored ceiling permits recovery", progress.Inserted)})

	other, err = openSQLite(path, false, false)
	if err != nil {
		return nil, err
	}
	if err := other.exec("BEGIN IMMEDIATE"); err != nil {
		other.close()
		return nil, err
	}
	if err := other.exec("UPDATE probe_meta SET value=? WHERE key='generation'", u64(next.Generation)); err != nil {
		other.close()
		return nil, err
	}
	// Closing the connection tests rollback of an uncommitted transaction;
	// it is not a process crash or power-loss recovery experiment.
	if err := other.close(); err != nil {
		return nil, err
	}
	if err := unchanged(); err != nil {
		return nil, err
	}
	rows = append(rows, control{"uncommitted-close", "passed", "uncommitted generation rolled back on connection close"})
	if err := d.load(next); err != nil {
		return nil, err
	}
	if err := d.close(); err != nil {
		return nil, err
	}
	d, err = openSQLite(path, false, false)
	if err != nil {
		return nil, err
	}
	d.generation, err = d.readGeneration()
	if err != nil || d.generation != next.Generation {
		return nil, errors.New("committed reopen lost generation")
	}
	e = next
	want, _ = baseline(e, q)
	if err := unchanged(); err != nil {
		return nil, err
	}
	rows = append(rows, control{"committed-reopen", "passed", "exact committed domain generation and ordered IDs"})
	for _, tc := range []struct {
		name string
		data []byte
	}{{"corrupt", []byte("not a database")}, {"truncated", []byte("SQLite format 3\x00")}} {
		badPath := filepath.Join(root, tc.name+".db")
		if err := os.WriteFile(badPath, tc.data, 0600); err != nil {
			return nil, err
		}
		bad, openErr := openSQLite(badPath, false, false)
		if openErr == nil {
			_, openErr = bad.readGeneration()
			if err := bad.close(); err != nil {
				return nil, err
			}
		}
		if openErr == nil {
			return nil, fmt.Errorf("%s database admitted", tc.name)
		}
		if err := os.Remove(badPath); err != nil {
			return nil, err
		}
		clean, err := openSQLite(badPath, false, true)
		if err != nil {
			return nil, err
		}
		err = clean.load(e)
		if err == nil {
			stored, readErr := clean.readback()
			if readErr != nil || !reflect.DeepEqual(stored, e) {
				err = errors.New("corrupt cache rebuild differs")
			}
		}
		err = errors.Join(err, clean.close())
		if err != nil {
			return nil, err
		}
		rows = append(rows, control{tc.name, "passed", "explicit refusal and clean owned rebuild; original database unchanged"})
	}
	wrong, err := openSQLite(filepath.Join(root, "wrong-schema.db"), false, true)
	if err != nil {
		return nil, err
	}
	_, schemaErr := wrong.readGeneration()
	closeErr := wrong.close()
	if schemaErr == nil || closeErr != nil {
		return nil, errors.New("wrong database schema admitted or not closed")
	}
	if err := unchanged(); err != nil {
		return nil, err
	}
	rows = append(rows, control{"wrong-schema", "passed", "probe metadata absent; original generation unchanged"})
	return rows, nil
}

func asciiTokens(label string) string {
	return strings.Join(strings.FieldsFunc(strings.ToLower(label), func(r rune) bool {
		return r < 'a' || r > 'z'
	}), " ")
}

func (d *sqliteBackend) ftsBuild(e envelope) (available bool, err error) {
	if err := d.exec("CREATE VIRTUAL TABLE probe_fts USING fts5(domain_id UNINDEXED, tokens, tokenize='ascii')"); err != nil {
		var failure *sqliteFailure
		if errors.As(err, &failure) && failure.Code == 1 && strings.Contains(failure.Text, "no such module: fts5") {
			return false, nil
		}
		return false, err
	}
	if err = d.exec("BEGIN IMMEDIATE"); err != nil {
		return false, err
	}
	defer func() {
		if err != nil {
			err = errors.Join(err, d.rollback())
		}
	}()
	s, err := d.prepare("INSERT INTO probe_fts VALUES(?,?)")
	if err != nil {
		return false, err
	}
	for _, entry := range e.Snapshot.Entries() {
		err = s.bind(entry.ID, asciiTokens(entry.Label))
		if err == nil {
			_, err = s.next()
		}
		if err == nil {
			err = s.reset()
		}
		if err != nil {
			s.close()
			return false, err
		}
	}
	if err = s.close(); err != nil {
		return false, err
	}
	if err = d.exec("COMMIT"); err != nil {
		return false, err
	}
	return true, nil
}

func (d *sqliteBackend) ftsQuery(prefix string) ([]string, error) {
	if len(prefix) == 0 || len(prefix) > 16 {
		return nil, errors.New("invalid fixed token prefix")
	}
	for _, r := range prefix {
		if r < 'a' || r > 'z' {
			return nil, errors.New("invalid fixed token prefix")
		}
	}
	s, err := d.prepare("SELECT domain_id FROM probe_fts WHERE tokens MATCH ? ORDER BY domain_id LIMIT 4097")
	if err != nil {
		return nil, err
	}
	if err = s.bind(`"` + prefix + `"*`); err != nil {
		s.close()
		return nil, err
	}
	got := []string{}
	for {
		row, err := s.next()
		if err != nil {
			s.close()
			return nil, err
		}
		if !row {
			break
		}
		id, err := s.blob(0, 64)
		if err != nil || len(id) == 0 || len(got) == 4096 {
			s.close()
			return nil, errors.New("invalid FTS result")
		}
		got = append(got, string(id))
	}
	if err = s.close(); err != nil {
		return nil, err
	}
	return got, nil
}

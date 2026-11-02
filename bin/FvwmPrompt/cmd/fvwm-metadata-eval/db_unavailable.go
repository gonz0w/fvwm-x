//go:build fvwm_metadata_eval && (!fvwm_sqlite_eval || !cgo)

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import "errors"

const sqliteCompiled = false

type sqliteBackend struct{}

func openSQLite(string, bool, bool) (*sqliteBackend, error) {
	return nil, errors.New("optional sqlite probe unavailable in this build")
}
func (*sqliteBackend) load(envelope) error { return errors.New("sqlite unavailable") }
func (*sqliteBackend) query(query) (answer, error) {
	return answer{}, errors.New("sqlite unavailable")
}
func (*sqliteBackend) close() error  { return nil }
func sqliteIdentity() map[string]any { return map[string]any{"compiled": false} }

func (*sqliteBackend) ftsBuild(envelope) (bool, error) {
	return false, errors.New("sqlite unavailable")
}
func (*sqliteBackend) ftsQuery(string) ([]string, error) {
	return nil, errors.New("sqlite unavailable")
}
func databaseControls(string) ([]control, error) {
	return nil, errors.New("sqlite unavailable")
}

func (*sqliteBackend) reopen() error { return errors.New("sqlite unavailable") }

func (*sqliteBackend) readback() (envelope, error) {
	return envelope{}, errors.New("sqlite unavailable")
}

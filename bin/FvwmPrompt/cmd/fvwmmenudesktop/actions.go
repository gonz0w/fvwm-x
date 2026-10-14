// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

type launchRecord struct {
	Argv            []string
	Directory       string
	TerminalCommand string
}
type actionCatalog struct {
	Version int
	Records map[string]launchRecord
}
type actionEncoder struct {
	path, terminal string
	catalog        actionCatalog
	bytes          int
	lock           *os.File
}

func newActionEncoder(userdir, title, terminal string) (*actionEncoder, error) {
	if !filepath.IsAbs(userdir) {
		return nil, errors.New("application action directory must be absolute")
	}
	hash := sha256.Sum256([]byte(title))
	e := &actionEncoder{path: filepath.Join(userdir, ".XDGMenu-actions-"+hex.EncodeToString(hash[:16])), terminal: terminal, bytes: xdgsource.MaxJobBytes - 64,
		catalog: actionCatalog{Version: 1, Records: make(map[string]launchRecord)}}
	lock, err := xdgsource.LockRecord(e.path + ".lock")
	if err != nil {
		return nil, err
	}
	e.lock = lock
	ok := false
	defer func() {
		if !ok {
			e.close()
		}
	}()
	previous, err := xdgsource.ReadPrivate(e.path)
	if errors.Is(err, fs.ErrNotExist) {
		ok = true
		return e, nil
	}
	if err != nil {
		return nil, err
	}
	if !utf8.Valid(previous) {
		return nil, errors.New("invalid application catalog text")
	}
	if err := json.Unmarshal(previous, &e.catalog); err != nil {
		return nil, err
	}
	if e.catalog.Version != 1 || len(e.catalog.Records) > xdgmenu.MaxItems {
		return nil, errors.New("invalid application action catalog")
	}
	if e.catalog.Records == nil {
		e.catalog.Records = make(map[string]launchRecord)
	}
	// Keep stable action references from existing and exported menus. Refuse a
	// full catalog rather than evict records while the core may still use them.
	for id, record := range e.catalog.Records {
		if err := validateLaunch(record); err != nil {
			return nil, err
		}
		data, err := json.Marshal(record)
		if err != nil {
			return nil, err
		}
		digest := sha256.Sum256(data)
		if id != hex.EncodeToString(digest[:]) || len(data)+len(id)+8 > e.bytes {
			return nil, errors.New("invalid or full application action catalog")
		}
		e.bytes -= len(data) + len(id) + 8
	}
	ok = true
	return e, nil
}
func (e *actionEncoder) close() {
	if e.lock != nil {
		e.lock.Close()
		e.lock = nil
	}
}
func (e *actionEncoder) encode(item xdgmenu.PresentedItem) (string, error) {
	record := launchRecord{Argv: item.Argv, Directory: item.WorkingDir}
	if item.Terminal {
		record.TerminalCommand = e.terminal
	}
	if err := validateLaunch(record); err != nil {
		return "", err
	}
	data, err := json.Marshal(record)
	if err != nil {
		return "", err
	}
	hash := sha256.Sum256(data)
	id := hex.EncodeToString(hash[:])
	if _, present := e.catalog.Records[id]; !present {
		if len(e.catalog.Records) >= xdgmenu.MaxItems || len(data)+len(id)+8 > e.bytes {
			return "", errors.New("application action catalog exceeds limit")
		}
		e.bytes -= len(data) + len(id) + 8
		e.catalog.Records[id] = record
	}
	// Only fixed syntax, URL-base64 and hexadecimal tokens reach the shell and
	// FVWM expanders. Application metadata is retrieved as private typed data.
	return "Exec exec fvwm-menu-desktop --launch " + base64.RawURLEncoding.EncodeToString([]byte(e.path)) + " " + id, nil
}
func (e *actionEncoder) publish() error {
	defer e.close()
	data, err := json.Marshal(e.catalog)
	if err != nil {
		return err
	}
	return xdgsource.PublishPrivate(e.path, data)
}
func validateLaunch(record launchRecord) error {
	if len(record.Argv) == 0 || len(record.Argv) > xdgmenu.MaxItems || record.Argv[0] == "" {
		return errors.New("invalid application arguments")
	}
	bytes := xdgmenu.MaxSourceBytes
	for _, value := range append(append([]string(nil), record.Argv...), record.Directory, record.TerminalCommand) {
		if len(value) > xdgmenu.MaxValueBytes || !utf8.ValidString(value) || strings.ContainsRune(value, 0) || len(value) > bytes {
			return errors.New("application action exceeds limits")
		}
		bytes -= len(value)
	}
	if record.Directory != "" && !filepath.IsAbs(record.Directory) {
		return errors.New("application directory must be absolute")
	}
	return nil
}
func launch(args []string) error {
	if len(args) != 2 || len(args[0]) > 2*xdgmenu.MaxValueBytes || len(args[1]) != 64 {
		return errors.New("invalid application action reference")
	}
	if _, err := hex.DecodeString(args[1]); err != nil {
		return err
	}
	path, err := base64.RawURLEncoding.DecodeString(args[0])
	if err != nil {
		return err
	}
	data, err := xdgsource.ReadPrivate(string(path))
	if err != nil {
		return err
	}
	var catalog actionCatalog
	if err = json.Unmarshal(data, &catalog); err != nil {
		return err
	}
	if catalog.Version != 1 || len(catalog.Records) > xdgmenu.MaxItems {
		return errors.New("invalid application action catalog")
	}
	record, present := catalog.Records[args[1]]
	if !present {
		return errors.New("application action is no longer available; regenerate the menu")
	}
	if err = validateLaunch(record); err != nil {
		return err
	}
	if record.Directory != "" {
		if err = os.Chdir(record.Directory); err != nil {
			return err
		}
	}
	argv := record.Argv
	if record.TerminalCommand != "" {
		// --term-cmd is an explicitly configured command, as in the existing
		// provider. Its program syntax remains usable; desktop argv is quoted
		// separately and cannot add syntax to that configured command.
		quoted := make([]string, len(argv))
		for i, value := range argv {
			quoted[i] = "'" + strings.ReplaceAll(value, "'", `'\''`) + "'"
		}
		argv = []string{"/bin/sh", "-c", "exec " + record.TerminalCommand + " " + strings.Join(quoted, " ")}
	}
	program, err := exec.LookPath(argv[0])
	if err != nil {
		return err
	}
	return syscall.Exec(program, argv, os.Environ())
}

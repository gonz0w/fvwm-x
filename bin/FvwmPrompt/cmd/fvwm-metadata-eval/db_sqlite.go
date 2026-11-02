//go:build fvwm_metadata_eval && fvwm_sqlite_eval && cgo

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

/*
#cgo LDFLAGS: -lsqlite3
#include <sqlite3.h>
#include <stdlib.h>
static int eval_bind_text(sqlite3_stmt *s, int n, const void *p, int size) {
	return sqlite3_bind_text(s, n, p, size, SQLITE_TRANSIENT);
}
static int eval_bind_blob(sqlite3_stmt *s, int n, const void *p, int size) {
	return sqlite3_bind_blob(s, n, p, size, SQLITE_TRANSIENT);
}
*/
import "C"

import (
	"bytes"
	"crypto/sha256"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"
	"unsafe"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

const sqliteCompiled = true
const sqliteFileMax = 16 * 1024 * 1024

type sqliteFailure struct {
	Code int
	Op   string
	Text string
}

func (e *sqliteFailure) Error() string {
	return fmt.Sprintf("sqlite %s: code %d: %s", e.Op, e.Code, e.Text)
}

type loadFailure struct {
	Cause    error
	Deleted  bool
	Inserted int
}

func (e *loadFailure) Error() string {
	return fmt.Sprintf("replacement deleted=%t inserted=%d: %v", e.Deleted, e.Inserted, e.Cause)
}
func (e *loadFailure) Unwrap() error { return e.Cause }

type sqliteBackend struct {
	db         *C.sqlite3
	generation uint64
}

type statement struct {
	db *sqliteBackend
	s  *C.sqlite3_stmt
}

func (d *sqliteBackend) failure(code C.int, op string) error {
	return &sqliteFailure{int(code), op, C.GoString(C.sqlite3_errmsg(d.db))}
}

func (d *sqliteBackend) prepare(sql string) (*statement, error) {
	text := C.CString(sql)
	defer C.free(unsafe.Pointer(text))
	s := &statement{db: d}
	code := C.sqlite3_prepare_v2(d.db, text, -1, &s.s, nil)
	if code != C.SQLITE_OK {
		if s.s != nil {
			C.sqlite3_finalize(s.s)
		}
		return nil, d.failure(code, "prepare")
	}
	return s, nil
}

func (s *statement) close() error {
	code := C.sqlite3_finalize(s.s)
	s.s = nil
	if code != C.SQLITE_OK {
		return s.db.failure(code, "finalize")
	}
	return nil
}

func (s *statement) bind(values ...any) error {
	for i, value := range values {
		var code C.int
		switch v := value.(type) {
		case string:
			p := C.CString(v)
			code = C.eval_bind_text(s.s, C.int(i+1), unsafe.Pointer(p), C.int(len(v)))
			C.free(unsafe.Pointer(p))
		case []byte:
			if len(v) == 0 {
				code = C.sqlite3_bind_zeroblob(s.s, C.int(i+1), 0)
			} else {
				p := C.CBytes(v)
				code = C.eval_bind_blob(s.s, C.int(i+1), p, C.int(len(v)))
				C.free(p)
			}
		case int64:
			code = C.sqlite3_bind_int64(s.s, C.int(i+1), C.sqlite3_int64(v))
		default:
			return errors.New("unsupported fixed probe binding")
		}
		if code != C.SQLITE_OK {
			return s.db.failure(code, "bind")
		}
	}
	return nil
}

func (s *statement) reset() error {
	if code := C.sqlite3_reset(s.s); code != C.SQLITE_OK {
		return s.db.failure(code, "reset")
	}
	if code := C.sqlite3_clear_bindings(s.s); code != C.SQLITE_OK {
		return s.db.failure(code, "clear bindings")
	}
	return nil
}

func (s *statement) next() (bool, error) {
	code := C.sqlite3_step(s.s)
	if code == C.SQLITE_ROW {
		return true, nil
	}
	if code != C.SQLITE_DONE {
		return false, s.db.failure(code, "step")
	}
	return false, nil
}

func (s *statement) blob(column, maximum int) ([]byte, error) {
	length := int(C.sqlite3_column_bytes(s.s, C.int(column)))
	if length < 0 || length > maximum {
		return nil, errors.New("database value exceeds probe bound")
	}
	return C.GoBytes(C.sqlite3_column_blob(s.s, C.int(column)), C.int(length)), nil
}

func (d *sqliteBackend) exec(sql string, values ...any) error {
	s, err := d.prepare(sql)
	if err != nil {
		return err
	}
	if err = s.bind(values...); err != nil {
		s.close()
		return err
	}
	row, err := s.next()
	closeErr := s.close()
	if err != nil {
		return err
	}
	if row {
		return errors.New("unexpected database result row")
	}
	return closeErr
}

func (d *sqliteBackend) scalar(sql string) (int64, error) {
	s, err := d.prepare(sql)
	if err != nil {
		return 0, err
	}
	defer s.close()
	row, err := s.next()
	if err != nil || !row {
		return 0, fmt.Errorf("missing integer control: %v", err)
	}
	return int64(C.sqlite3_column_int64(s.s, 0)), nil
}

func openSQLite(path string, readonly, fresh bool) (*sqliteBackend, error) {
	if info, err := os.Stat(path); err == nil && (info.Size() > sqliteFileMax || !info.Mode().IsRegular()) {
		return nil, errors.New("database file exceeds probe bound")
	}
	if fresh {
		f, err := os.OpenFile(path, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
		if err != nil {
			return nil, err
		}
		if err := f.Close(); err != nil {
			return nil, err
		}
	}
	flags := C.int(C.SQLITE_OPEN_READWRITE)
	if readonly {
		flags = C.SQLITE_OPEN_READONLY
	}
	name := C.CString(path)
	defer C.free(unsafe.Pointer(name))
	d := &sqliteBackend{}
	code := C.sqlite3_open_v2(name, &d.db, flags, nil)
	if code != C.SQLITE_OK {
		err := d.failure(code, "open")
		if d.db != nil {
			C.sqlite3_close(d.db)
		}
		return nil, err
	}
	C.sqlite3_busy_timeout(d.db, 0)
	C.sqlite3_limit(d.db, C.SQLITE_LIMIT_LENGTH, filemodel.MaxWire)
	C.sqlite3_limit(d.db, C.SQLITE_LIMIT_SQL_LENGTH, 65536)
	C.sqlite3_limit(d.db, C.SQLITE_LIMIT_COLUMN, 32)
	C.sqlite3_limit(d.db, C.SQLITE_LIMIT_VARIABLE_NUMBER, 16)
	C.sqlite3_limit(d.db, C.SQLITE_LIMIT_ATTACHED, 0)
	C.sqlite3_limit(d.db, C.SQLITE_LIMIT_WORKER_THREADS, 0)
	if err := d.exec("PRAGMA trusted_schema=OFF"); err != nil {
		d.close()
		return nil, err
	}
	pageSize, err := d.scalar("PRAGMA page_size")
	if err != nil || pageSize < 512 || pageSize > 65536 || pageSize&(pageSize-1) != 0 {
		d.close()
		return nil, errors.New("invalid SQLite page size")
	}
	if !readonly {
		pages := int64(sqliteFileMax) / pageSize
		actual, err := d.scalar("PRAGMA max_page_count=" + strconv.FormatInt(pages, 10))
		if err != nil || actual != pages {
			d.close()
			return nil, errors.New("private database page ceiling unavailable")
		}
	}
	if fresh {
		// Record these durability modes; do not equate them to the snapshot's
		// rename-only process-reopen publication or claim power-loss tests.
		for _, sql := range []string{
			"PRAGMA synchronous=FULL",
			"CREATE TABLE probe_meta (key TEXT PRIMARY KEY, value BLOB NOT NULL)",
			"CREATE TABLE probe_items (ordinal INTEGER PRIMARY KEY, domain_id TEXT UNIQUE NOT NULL, name BLOB NOT NULL, kind TEXT NOT NULL, mode INTEGER NOT NULL, device BLOB NOT NULL, inode BLOB NOT NULL, size INTEGER NOT NULL, modified INTEGER NOT NULL, label TEXT NOT NULL, normalized TEXT NOT NULL)",
			"CREATE INDEX probe_names ON probe_items(name)",
		} {
			if err := d.exec(sql); err != nil {
				d.close()
				return nil, err
			}
		}
	}
	return d, nil
}

func (d *sqliteBackend) rollback() error {
	if C.sqlite3_get_autocommit(d.db) != 0 {
		return nil
	}
	return d.exec("ROLLBACK")
}

func (d *sqliteBackend) close() error {
	if d == nil || d.db == nil {
		return nil
	}
	code := C.sqlite3_close(d.db)
	if code != C.SQLITE_OK {
		return d.failure(code, "close")
	}
	d.db = nil
	return nil
}

func u64(value uint64) []byte {
	out := make([]byte, 8)
	binary.LittleEndian.PutUint64(out, value)
	return out
}

func (d *sqliteBackend) load(e envelope) (err error) {
	if err := validateEnvelope(e); err != nil {
		return err
	}
	wire, err := json.Marshal(e)
	if err != nil {
		return err
	}
	digest := sha256.Sum256(wire)
	if err := d.exec("BEGIN IMMEDIATE"); err != nil {
		return err
	}
	deleted, inserted := false, 0
	defer func() {
		if err != nil {
			err = &loadFailure{Cause: errors.Join(err, d.rollback()), Deleted: deleted, Inserted: inserted}
		}
	}()
	if d.generation != 0 {
		actual, readErr := d.readGeneration()
		if readErr != nil || actual != d.generation || e.Generation < actual {
			return errors.New("stale database update generation")
		}
		if e.Generation == actual {
			stored, readErr := d.metadata("input_digest", sha256.Size)
			if readErr != nil || !bytes.Equal(stored, digest[:]) {
				return errors.New("changed metadata under unchanged generation")
			}
			return d.exec("COMMIT")
		}
	}
	if err = d.exec("DELETE FROM probe_items"); err != nil {
		return err
	}
	deleted = true
	root := e.Snapshot
	root.Items = nil
	rootData, err := json.Marshal(root)
	if err != nil {
		return err
	}
	for _, pair := range []struct {
		key   string
		value []byte
	}{{"schema", []byte{corpusVersion}}, {"generation", u64(e.Generation)}, {"root", rootData}, {"input_digest", digest[:]}} {
		if err = d.exec("INSERT OR REPLACE INTO probe_meta VALUES(?,?)", pair.key, pair.value); err != nil {
			return err
		}
	}
	s, err := d.prepare("INSERT INTO probe_items VALUES(?,?,?,?,?,?,?,?,?,?,?)")
	if err != nil {
		return err
	}
	labels := e.Snapshot.Entries()
	for i, item := range e.Snapshot.Items {
		err = s.bind(int64(i+1), item.ID, item.Name, item.Kind, int64(item.Identity.Mode),
			u64(item.Identity.Device), u64(item.Identity.Inode), item.Identity.Size,
			item.Identity.MTime, labels[i].Label, strings.ToLower(labels[i].Label))
		if err == nil {
			var row bool
			row, err = s.next()
			if row && err == nil {
				err = errors.New("insert returned row")
			}
		}
		if err != nil {
			s.close()
			return err
		}
		inserted++
		if err = s.reset(); err != nil {
			s.close()
			return err
		}
	}
	if err = s.close(); err != nil {
		return err
	}
	if err = d.exec("COMMIT"); err != nil {
		return err
	}
	d.generation = e.Generation
	return nil
}

func (d *sqliteBackend) metadata(key string, maximum int) ([]byte, error) {
	s, err := d.prepare("SELECT value FROM probe_meta WHERE key=?")
	if err != nil {
		return nil, err
	}
	defer s.close()
	if err := s.bind(key); err != nil {
		return nil, err
	}
	row, err := s.next()
	if err != nil || !row {
		return nil, fmt.Errorf("missing metadata %s: %v", key, err)
	}
	if C.sqlite3_column_type(s.s, 0) != C.SQLITE_BLOB {
		return nil, errors.New("invalid metadata storage type")
	}
	return s.blob(0, maximum)
}

func (d *sqliteBackend) readGeneration() (uint64, error) {
	version, err := d.metadata("schema", 1)
	if err != nil || len(version) != 1 || version[0] != corpusVersion {
		return 0, errors.New("wrong database probe schema")
	}
	data, err := d.metadata("generation", 8)
	if err != nil || len(data) != 8 || binary.LittleEndian.Uint64(data) == 0 {
		return 0, errors.New("invalid database generation")
	}
	return binary.LittleEndian.Uint64(data), nil
}

func (d *sqliteBackend) reopen() error {
	generation, err := d.readGeneration()
	if err != nil {
		return err
	}
	d.generation = generation
	return nil
}

func ordering(q query) (string, error) {
	columns := map[string][]string{"name": {"name"}, "kind": {"kind", "name"},
		"size": {"size", "name"}, "modified": {"modified", "name"}}
	names, ok := columns[q.Order]
	if !ok {
		return "", errors.New("unsupported file ordering")
	}
	var validation collection.Model
	if err := validation.SetFilter(q.Filter); err != nil {
		return "", err
	}
	direction := " ASC"
	if q.Reverse {
		direction = " DESC"
	}
	parts := []string{"CASE WHEN kind='directory' THEN 0 ELSE 1 END ASC"}
	for _, name := range names {
		parts = append(parts, name+direction)
	}
	return strings.Join(parts, ","), nil
}

func (d *sqliteBackend) query(q query) (result answer, err error) {
	order, err := ordering(q)
	if err != nil {
		return answer{}, err
	}
	if err = d.exec("BEGIN"); err != nil {
		return answer{}, err
	}
	defer func() {
		if err != nil {
			err = errors.Join(err, d.rollback())
		}
	}()
	generation, err := d.readGeneration()
	if err != nil || generation != d.generation {
		return answer{}, errors.New("stale database generation")
	}
	s, err := d.prepare("SELECT domain_id FROM probe_items WHERE instr(normalized,?)>0 ORDER BY " + order + " LIMIT 4097")
	if err != nil {
		return answer{}, err
	}
	if err = s.bind(strings.ToLower(q.Filter)); err != nil {
		s.close()
		return answer{}, err
	}
	result = answer{Generation: generation, IDs: []string{}}
	for {
		var row bool
		row, err = s.next()
		if err != nil || !row {
			break
		}
		if C.sqlite3_column_type(s.s, 0) != C.SQLITE_TEXT {
			err = errors.New("invalid result identity type")
			break
		}
		var id []byte
		id, err = s.blob(0, 64)
		if err != nil || len(id) == 0 || len(result.IDs) == collection.MaxItems {
			err = errors.New("invalid database result identity/count")
			break
		}
		result.IDs = append(result.IDs, string(id))
	}
	closeErr := s.close()
	if err == nil {
		err = closeErr
	}
	if err != nil {
		return answer{}, err
	}
	if err = d.exec("COMMIT"); err != nil {
		return answer{}, err
	}
	return result, nil
}

func sqliteIdentity() map[string]any {
	options := []string{}
	complete := false
	for i := 0; i < 256; i++ {
		option := C.sqlite3_compileoption_get(C.int(i))
		if option == nil {
			complete = true
			break
		}
		options = append(options, C.GoString(option))
	}
	return map[string]any{"compiled": true, "version": C.GoString(C.sqlite3_libversion()),
		"source_id": C.GoString(C.sqlite3_sourceid()), "header_version": C.SQLITE_VERSION,
		"header_source_id": C.SQLITE_SOURCE_ID, "threadsafe": int(C.sqlite3_threadsafe()),
		"compile_options": options, "compile_options_enumeration_complete": complete,
		"sqlite_reported_memory_current":   int64(C.sqlite3_memory_used()),
		"sqlite_reported_memory_highwater": int64(C.sqlite3_memory_highwater(0)),
		"counter_scope":                    "Provider-reported global allocator counters; may be disabled, not total memory or independent runtime enablement proof",
		"binding":                          "direct optional cgo; explicit transient copies"}
}

// Full bounded readback is a correctness oracle, outside cost-only runs. It
// checks original bytes and integer domains rather than only selected IDs.
func (d *sqliteBackend) readback() (result envelope, err error) {
	if err = d.exec("BEGIN"); err != nil {
		return result, err
	}
	defer func() {
		if err != nil {
			err = errors.Join(err, d.rollback())
		}
	}()
	generation, err := d.readGeneration()
	if err != nil {
		return result, err
	}
	root, err := d.metadata("root", filemodel.MaxWire)
	if err != nil {
		return result, err
	}
	decoder := json.NewDecoder(bytes.NewReader(root))
	decoder.DisallowUnknownFields()
	result.Version, result.Generation = corpusVersion, generation
	if err = decoder.Decode(&result.Snapshot); err != nil {
		return envelope{}, err
	}
	if decoder.Decode(new(any)) != io.EOF || len(result.Snapshot.Items) != 0 {
		return envelope{}, errors.New("invalid database root record")
	}
	s, err := d.prepare("SELECT domain_id,name,kind,mode,device,inode,size,modified,label,normalized FROM probe_items ORDER BY ordinal LIMIT 4097")
	if err != nil {
		return envelope{}, err
	}
	for {
		var row bool
		row, err = s.next()
		if err != nil || !row {
			break
		}
		if len(result.Snapshot.Items) == collection.MaxItems {
			err = errors.New("database item count exceeds limit")
			break
		}
		for i, want := range []C.int{C.SQLITE_TEXT, C.SQLITE_BLOB, C.SQLITE_TEXT, C.SQLITE_INTEGER, C.SQLITE_BLOB, C.SQLITE_BLOB, C.SQLITE_INTEGER, C.SQLITE_INTEGER, C.SQLITE_TEXT, C.SQLITE_TEXT} {
			if C.sqlite3_column_type(s.s, C.int(i)) != want {
				err = errors.New("invalid complete metadata storage type")
				break
			}
		}
		if err != nil {
			break
		}
		values := make([][]byte, 7)
		for i, column := range []int{0, 1, 2, 4, 5, 8, 9} {
			values[i], err = s.blob(column, filemodel.MaxPath)
			if err != nil {
				break
			}
		}
		if err != nil {
			break
		}
		if len(values[3]) != 8 || len(values[4]) != 8 {
			err = errors.New("invalid unsigned metadata encoding")
			break
		}
		mode := int64(C.sqlite3_column_int64(s.s, 3))
		if mode < 0 || uint64(mode) > uint64(^uint32(0)) {
			err = errors.New("invalid metadata mode")
			break
		}
		item := filemodel.Item{ID: string(values[0]), Name: values[1], Kind: string(values[2]),
			Identity: filemodel.Identity{Mode: os.FileMode(mode),
				Device: binary.LittleEndian.Uint64(values[3]), Inode: binary.LittleEndian.Uint64(values[4]),
				Size: int64(C.sqlite3_column_int64(s.s, 6)), MTime: int64(C.sqlite3_column_int64(s.s, 7))}}
		label := (&filemodel.Snapshot{Items: []filemodel.Item{item}}).Entries()[0].Label
		if label != string(values[5]) || strings.ToLower(label) != string(values[6]) {
			err = errors.New("stored projection differs from original metadata")
			break
		}
		result.Snapshot.Items = append(result.Snapshot.Items, item)
	}
	err = errors.Join(err, s.close())
	if err != nil {
		return envelope{}, err
	}
	if err = validateEnvelope(result); err != nil {
		return envelope{}, err
	}
	wire, err := json.Marshal(result)
	if err != nil {
		return envelope{}, err
	}
	stored, err := d.metadata("input_digest", sha256.Size)
	digest := sha256.Sum256(wire)
	if err != nil || !bytes.Equal(stored, digest[:]) {
		return envelope{}, errors.New("database complete metadata differs from admitted snapshot")
	}
	if err = d.exec("COMMIT"); err != nil {
		return envelope{}, err
	}
	return result, nil
}

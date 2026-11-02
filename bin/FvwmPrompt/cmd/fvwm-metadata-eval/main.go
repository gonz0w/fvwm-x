//go:build fvwm_metadata_eval

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Command fvwm-metadata-eval is a manually selected public-data experiment.
// It is excluded from normal builds and is never installed by Meson.
package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"os"
	"path/filepath"
	"reflect"
	"runtime"
	"sort"
	"syscall"
	"time"
)

type control struct {
	Name   string `json:"name"`
	Status string `json:"status"`
	Detail string `json:"detail"`
}

type sample struct {
	Phase         string `json:"phase"`
	Nanoseconds   int64  `json:"nanoseconds"`
	GoAllocBytes  uint64 `json:"go_alloc_bytes"`
	GoAllocations uint64 `json:"go_allocations"`
	UserCPUUS     int64  `json:"user_cpu_us"`
	SystemCPUUS   int64  `json:"system_cpu_us"`
	RawMaxRSS     int64  `json:"raw_max_rss"`
}

type report struct {
	Mode                    string         `json:"mode"`
	ResultSHA256            string         `json:"result_sha256,omitempty"`
	FTSIncrementalFileBytes int64          `json:"fts_incremental_file_bytes"`
	Schema                  int            `json:"schema"`
	Status                  string         `json:"status"`
	Error                   string         `json:"error,omitempty"`
	Backend                 string         `json:"backend"`
	Count                   int            `json:"count"`
	Repetitions             int            `json:"repetitions"`
	GOOS                    string         `json:"goos"`
	GOARCH                  string         `json:"goarch"`
	GoVersion               string         `json:"go_version"`
	SQLite                  map[string]any `json:"sqlite"`
	Samples                 []sample       `json:"samples"`
	Controls                []control      `json:"controls"`
	FinalFileBytes          int64          `json:"final_file_bytes"`
	Scope                   string         `json:"scope"`
}

func measured(phase string, fn func() error) (sample, error) {
	var before, after runtime.MemStats
	var cpuBefore, cpuAfter syscall.Rusage
	runtime.ReadMemStats(&before)
	if err := syscall.Getrusage(syscall.RUSAGE_SELF, &cpuBefore); err != nil {
		return sample{}, err
	}
	start := time.Now()
	err := fn()
	ns := time.Since(start).Nanoseconds()
	if usageErr := syscall.Getrusage(syscall.RUSAGE_SELF, &cpuAfter); usageErr != nil {
		return sample{}, errors.Join(err, usageErr)
	}
	runtime.ReadMemStats(&after)
	micros := func(v syscall.Timeval) int64 { return int64(v.Sec)*1000000 + int64(v.Usec) }
	return sample{Phase: phase, Nanoseconds: ns,
		GoAllocBytes:  after.TotalAlloc - before.TotalAlloc,
		GoAllocations: after.Mallocs - before.Mallocs,
		UserCPUUS:     micros(cpuAfter.Utime) - micros(cpuBefore.Utime),
		SystemCPUUS:   micros(cpuAfter.Stime) - micros(cpuBefore.Stime),
		RawMaxRSS:     cpuAfter.Maxrss}, err
}

func dataBytes(root string) (int64, error) {
	var total int64
	for _, name := range []string{"snapshot.json", "metadata.db", "metadata.db-journal", "metadata.db-wal", "metadata.db-shm"} {
		info, err := os.Stat(filepath.Join(root, name))
		if err == nil {
			total += info.Size()
		} else if !os.IsNotExist(err) {
			return 0, err
		}
	}
	return total, nil
}

func tokenScan(entries []collection.Entry, prefix string) []string {
	ids := []string{}
	for _, entry := range entries {
		if tokenMatch(entry.Label, prefix) {
			ids = append(ids, entry.ID)
		}
	}
	sort.Strings(ids)
	return ids
}

func perform(r *report, root string, e envelope) (err error) {
	var memory *memoryBackend
	var database *sqliteBackend
	defer func() {
		if database != nil {
			err = errors.Join(err, database.close())
		}
	}()
	observe := func(name string, fn func() error) error {
		row, err := measured(name, fn)
		r.Samples = append(r.Samples, row)
		return err
	}
	if r.Mode == "controls" {
		controlRoot := filepath.Join(root, "controls")
		if err := os.Mkdir(controlRoot, 0700); err != nil {
			return err
		}
		return observe("failure-controls", func() error {
			var rows []control
			if r.Backend == "sqlite" {
				rows, err = databaseControls(controlRoot)
			} else {
				rows, err = snapshotControls(controlRoot)
			}
			r.Controls = append(r.Controls, rows...)
			return err
		})
	}
	if err := observe("load", func() error {
		switch r.Backend {
		case "memory":
			memory, err = newMemory(e)
		case "snapshot":
			if err = saveSnapshot(root, e); err == nil {
				var loaded envelope
				loaded, err = loadSnapshot(filepath.Join(root, "snapshot.json"))
				if err == nil {
					memory, err = newMemory(loaded)
				}
			}
		case "sqlite":
			database, err = openSQLite(filepath.Join(root, "metadata.db"), false, true)
			if err == nil {
				err = database.load(e)
			}
		}
		return err
	}); err != nil {
		return err
	}
	checksum := sha256.New()
	digest := json.NewEncoder(checksum)
	queries := workloadQueries()
	var wanted []answer
	if r.Mode == "verify" {
		// Differential checks complement independent hand-authored goldens.
		// Cost-only runs create no reference model and run no fault controls.
		oracle, buildErr := newMemory(e)
		if buildErr != nil {
			return buildErr
		}
		wanted = make([]answer, len(queries))
		for i, q := range queries {
			wanted[i], err = oracle.query(q)
			if err != nil {
				return err
			}
		}
	}
	get := func(q query) (answer, error) {
		if database != nil {
			return database.query(q)
		}
		return memory.query(q)
	}
	for repetition := 0; repetition < r.Repetitions; repetition++ {
		if err := observe("ordered-literal-query-batch", func() error {
			for i, q := range queries {
				got, queryErr := get(q)
				if queryErr != nil {
					return queryErr
				}
				if wanted != nil && !reflect.DeepEqual(got, wanted[i]) {
					return fmt.Errorf("query %d mismatch", i)
				}
				if err := digest.Encode(got); err != nil {
					return err
				}
			}
			return nil
		}); err != nil {
			return err
		}
	}
	apply := func(next envelope) error {
		if database != nil {
			return database.load(next)
		}
		staged, changed, err := memory.staged(next)
		if err != nil {
			return err
		}
		if changed && r.Backend == "snapshot" {
			if err := saveSnapshot(root, next); err != nil {
				return err
			}
		}
		memory = staged
		return nil
	}
	checkCurrent := func() error {
		got, err := get(query{Order: "name"})
		if err != nil {
			return err
		}
		if r.Mode == "verify" {
			want, err := baseline(e, query{Order: "name"})
			if err != nil || !reflect.DeepEqual(got, want) {
				return errors.New("updated metadata projection differs")
			}
			if database != nil {
				stored, err := database.readback()
				if err != nil || !reflect.DeepEqual(stored, e) {
					return errors.New("complete database metadata readback differs")
				}
			} else if !reflect.DeepEqual(memory.source, e) {
				return errors.New("complete snapshot metadata differs")
			}
		}
		return digest.Encode(got)
	}
	if err := observe("no-op-reconciliation", func() error { return apply(e) }); err != nil {
		return err
	}
	for _, large := range []bool{false, true} {
		next := changed(e, large)
		phase := "small-update-and-replacement"
		if large {
			phase = "bounded-large-update"
		}
		if err := observe(phase, func() error { return apply(next) }); err != nil {
			return err
		}
		previous := e
		e = next
		if r.Mode == "verify" {
			if err := apply(previous); err == nil {
				return errors.New("stale metadata update admitted")
			}
		}
		if err := checkCurrent(); err != nil {
			return err
		}
	}
	if r.Backend != "memory" {
		if err := observe("committed-reopen", func() error {
			if database != nil {
				if err := database.close(); err != nil {
					return err
				}
				database, err = openSQLite(filepath.Join(root, "metadata.db"), false, false)
				if err == nil {
					err = database.reopen()
				}
				return err
			}
			loaded, err := loadSnapshot(filepath.Join(root, "snapshot.json"))
			if err == nil {
				memory, err = newMemory(loaded)
			}
			return err
		}); err != nil {
			return err
		}
	}
	// Include the same final observation in every backend's result digest.
	if err := checkCurrent(); err != nil {
		return err
	}
	r.FinalFileBytes, err = dataBytes(root)
	if err != nil {
		return err
	}
	entries := e.Snapshot.Entries()
	prefixes := []string{"alp", "ig", "pipe", "item", "sum", "absent"}
	var tokenWanted [][]string
	if r.Mode == "verify" {
		for _, prefix := range prefixes {
			tokenWanted = append(tokenWanted, tokenScan(entries, prefix))
		}
	}
	available := true
	if database != nil {
		if err := observe("fts-index-build", func() error { available, err = database.ftsBuild(e); return err }); err != nil {
			return err
		}
		current, err := dataBytes(root)
		if err != nil {
			return err
		}
		r.FTSIncrementalFileBytes = current - r.FinalFileBytes
	}
	if !available {
		r.Controls = append(r.Controls, control{"fts5", "unavailable", "runtime module absent; literal-query result remains separate"})
	} else {
		for repetition := 0; repetition < r.Repetitions; repetition++ {
			if err := observe("ascii-token-prefix-query-batch", func() error {
				for i, prefix := range prefixes {
					var ids []string
					if database != nil {
						ids, err = database.ftsQuery(prefix)
					} else {
						ids = tokenScan(entries, prefix)
					}
					if err != nil {
						return err
					}
					if tokenWanted != nil && !reflect.DeepEqual(ids, tokenWanted[i]) {
						return fmt.Errorf("token prefix %s differs", prefix)
					}
					if err := digest.Encode(answer{Generation: e.Generation, IDs: ids}); err != nil {
						return err
					}
				}
				return nil
			}); err != nil {
				return err
			}
		}
	}
	r.ResultSHA256 = hex.EncodeToString(checksum.Sum(nil))
	return nil
}

func run() int {
	mode := flag.String("mode", "verify", "verify, measure (requires matching verified result hash), or controls")
	selected := flag.String("backend", "memory", "memory, snapshot or optional sqlite")
	count := flag.Int("count", 12, "public corpus size: 0, 12, 256 or 4096")
	repetitions := flag.Int("repetitions", 5, "bounded query batches: 1..9")
	execute := flag.Bool("run", false, "explicitly execute the public-data experiment")
	describe := flag.Bool("describe", false, "describe this build without a corpus/database")
	expectedSource := flag.String("expect-sqlite-source", "", "exact frozen target SQLite source ID required for --run --backend=sqlite")
	flag.Parse()
	if flag.NArg() != 0 || (*execute == *describe) || *repetitions < 1 || *repetitions > 9 ||
		(*selected != "memory" && *selected != "snapshot" && *selected != "sqlite") ||
		(*mode != "verify" && *mode != "measure" && *mode != "controls") || (*mode == "controls" && *count != 12) {
		flag.Usage()
		return 64
	}
	r := report{Schema: 1, Mode: *mode, Status: "prepared", Backend: *selected, Count: *count,
		Repetitions: *repetitions, GOOS: runtime.GOOS, GOARCH: runtime.GOARCH,
		GoVersion: runtime.Version(), SQLite: sqliteIdentity(),
		Scope: "Public synthetic metadata only; no installed backend or live filesystem. Phase timings exclude external process startup. Raw native max RSS units require the target profile; Go/SQLite counters are not total memory. Verify-mode RSS includes reference/oracle work; measure mode omits it and needs matching verified digest; controls use separate invocations. Final file sizes are not peak journal/storage costs. No daemon/idle, power-loss or WM speed claim."}
	emit := func(code int, err error) int {
		if err != nil {
			r.Error = err.Error()
		}
		if outputErr := json.NewEncoder(os.Stdout).Encode(r); outputErr != nil {
			fmt.Fprintln(os.Stderr, outputErr)
			return 1
		}
		return code
	}
	if *describe {
		return emit(0, nil)
	}
	if *selected == "sqlite" && !sqliteCompiled {
		r.Status = "unavailable"
		return emit(77, errors.New("optional sqlite evaluation build unavailable"))
	}
	if *selected == "sqlite" && (*expectedSource == "" || r.SQLite["source_id"] != *expectedSource) {
		r.Status = "refused"
		return emit(1, errors.New("SQLite source ID differs from frozen target"))
	}
	e, err := corpus(*count)
	if err != nil {
		r.Status = "refused"
		return emit(1, err)
	}
	root, err := os.MkdirTemp("", "fvwm-metadata-eval-")
	if err != nil {
		r.Status = "failed"
		return emit(1, err)
	}
	err = perform(&r, root, e)
	cleanup := os.RemoveAll(root) // Only this invocation's fresh private tree.
	if err = errors.Join(err, cleanup); err != nil {
		r.Status = "failed"
		return emit(1, err)
	}
	r.SQLite = sqliteIdentity()
	r.Status = "passed"
	if r.Mode == "measure" {
		r.Status = "measured; matching verification required"
	}
	for _, c := range r.Controls {
		if c.Status == "unavailable" {
			r.Status = "partial; capability unavailable"
			return emit(77, nil)
		}
	}
	return emit(0, nil)
}

func main() { os.Exit(run()) }

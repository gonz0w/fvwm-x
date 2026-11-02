//go:build fvwm_metadata_eval

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"reflect"
	"strconv"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

const corpusVersion = 1

// Probe-only envelopes are not an installed cache/schema. The snapshot retains
// domain IDs and original name bytes; collection labels never replace either.
type envelope struct {
	Version    int                `json:"probe_version"`
	Generation uint64             `json:"generation"`
	Snapshot   filemodel.Snapshot `json:"snapshot"`
}

type query struct {
	Order   string
	Reverse bool
	Filter  string
}

type answer struct {
	Generation uint64   `json:"generation"`
	IDs        []string `json:"ids"`
}

func corpus(count int) (envelope, error) {
	if count != 0 && count != 12 && count != 256 && count != 4096 {
		return envelope{}, errors.New("unsupported public corpus size")
	}
	names := [][]byte{
		[]byte("alpha.txt"), []byte("Bravo"), []byte("delta.log"),
		[]byte("ΣIGMA"), {0xff}, []byte(`"\xff"`),
		[]byte("İstanbul"), []byte("résumé"), []byte("middle.jpeg"),
		[]byte("PIPE"), []byte("item[100%]_x"), []byte("line\nx"),
	}
	e := envelope{Version: corpusVersion, Generation: 41,
		Snapshot: filemodel.Snapshot{Schema: 1,
			Path: []byte("/synthetic/fvwm-metadata-eval"), Hidden: true,
			Root: filemodel.Identity{Device: 7, Inode: 99,
				Mode: os.ModeDir | 0700}}}
	for i := 0; i < count; i++ {
		index, group := i%len(names), i/len(names)
		name := append([]byte(nil), names[index]...)
		if group != 0 {
			suffix := fmt.Sprintf("-%04d", group)
			if index == 5 {
				name = []byte(strconv.QuoteToGraphic(string(
					append([]byte{0xff}, suffix...))))
			} else {
				name = append(name, suffix...)
			}
		}
		kind, mode := "file", os.FileMode(0600)
		switch index {
		case 1:
			kind, mode = "directory", os.ModeDir|0700
		case 8:
			kind, mode = "symlink", os.ModeSymlink|0600
		case 9:
			kind, mode = "special", os.ModeNamedPipe|0600
		}
		size := int64(i + 1)
		if i >= 12 && i%31 == 0 {
			size += 1 << 62
		}
		e.Snapshot.Items = append(e.Snapshot.Items, filemodel.Item{
			ID: fmt.Sprintf("meta-%04d", i), Name: name, Kind: kind,
			Identity: filemodel.Identity{Device: 7, Inode: uint64(i + 100),
				Mode: mode, Size: size, MTime: int64(1<<62) - int64(i)},
		})
	}
	return e, validateEnvelope(e)
}

func validateEnvelope(e envelope) error {
	if e.Version != corpusVersion || e.Generation == 0 ||
		e.Snapshot.Schema != 1 || len(e.Snapshot.Path) == 0 ||
		len(e.Snapshot.Path) > filemodel.MaxPath ||
		len(e.Snapshot.Items) > collection.MaxItems {
		return errors.New("invalid metadata envelope")
	}
	for _, item := range e.Snapshot.Items {
		if len(item.Name) == 0 || len(item.Name) > filemodel.MaxPath ||
			bytes.ContainsAny(item.Name, "/\x00") {
			return errors.New("invalid raw metadata name")
		}
		switch item.Kind {
		case "file", "directory", "symlink", "special":
		default:
			return errors.New("invalid metadata kind")
		}
	}
	var m collection.Model
	if err := m.Replace(e.Snapshot.Entries()); err != nil {
		return err
	}
	data, err := json.Marshal(e)
	if err != nil || len(data) > filemodel.MaxWire {
		return errors.New("metadata wire limit")
	}
	return nil
}

type memoryBackend struct {
	source   envelope
	model    collection.Model
	order    string
	reverse  bool
	prepared bool
}

func newMemory(e envelope) (*memoryBackend, error) {
	if err := validateEnvelope(e); err != nil {
		return nil, err
	}
	return &memoryBackend{source: e}, nil
}

func (m *memoryBackend) query(q query) (answer, error) {
	// Reuse the actual view between filters. Revalidating/encoding the whole
	// snapshot for each search would overstate the current browser's cost.
	if !m.prepared || q.Order != m.order || q.Reverse != m.reverse {
		entries, err := m.source.Snapshot.OrderedEntries(q.Order, q.Reverse)
		if err != nil {
			return answer{}, err
		}
		if err := m.model.Replace(entries); err != nil {
			return answer{}, err
		}
		m.order, m.reverse, m.prepared = q.Order, q.Reverse, true
	}
	if err := m.model.SetFilter(q.Filter); err != nil {
		return answer{}, err
	}
	result := answer{Generation: m.source.Generation, IDs: []string{}}
	for _, entry := range m.model.VisibleEntries() {
		result.IDs = append(result.IDs, entry.ID)
	}
	return result, nil
}

func (m *memoryBackend) staged(e envelope) (*memoryBackend, bool, error) {
	if err := validateEnvelope(e); err != nil {
		return nil, false, err
	}
	if e.Generation < m.source.Generation ||
		(e.Generation == m.source.Generation && !reflect.DeepEqual(e, m.source)) {
		return nil, false, errors.New("stale metadata generation")
	}
	if reflect.DeepEqual(e, m.source) {
		return m, false, nil
	}
	next := *m
	if m.prepared {
		entries, err := e.Snapshot.OrderedEntries(m.order, m.reverse)
		if err != nil {
			return nil, false, err
		}
		// Reconcile publishes fresh slices/maps; the old view stays intact
		// until a snapshot write (if selected) succeeds and the caller adopts.
		if _, err := next.model.Reconcile(next.model.Generation, entries); err != nil {
			return nil, false, err
		}
	}
	next.source = e
	return &next, true, nil
}

func changed(e envelope, large bool) envelope {
	next := e
	next.Generation++
	next.Snapshot.Root.MTime++
	next.Snapshot.Items = append([]filemodel.Item(nil), e.Snapshot.Items...)
	count := 1
	if large {
		count = 300
	}
	if count > len(next.Snapshot.Items) {
		count = len(next.Snapshot.Items)
	}
	for i := 0; i < count; i++ {
		next.Snapshot.Items[i].Identity.Size++
	}
	if !large && len(next.Snapshot.Items) != 0 {
		i := len(next.Snapshot.Items) - 1
		next.Snapshot.Items[i].ID = fmt.Sprintf("replacement-%04d", i)
	}
	return next
}

func baseline(e envelope, q query) (answer, error) {
	m, err := newMemory(e)
	if err != nil {
		return answer{}, err
	}
	return m.query(q)
}

func decodeEnvelope(data []byte) (envelope, error) {
	var e envelope
	if len(data) > filemodel.MaxWire {
		return e, errors.New("metadata wire limit")
	}
	d := json.NewDecoder(bytes.NewReader(data))
	d.DisallowUnknownFields()
	if err := d.Decode(&e); err != nil {
		return envelope{}, err
	}
	if err := d.Decode(new(any)); err != io.EOF {
		return envelope{}, errors.New("trailing metadata record")
	}
	if err := validateEnvelope(e); err != nil {
		return envelope{}, err
	}
	return e, nil
}

func loadSnapshot(path string) (envelope, error) {
	f, err := os.Open(path)
	if err != nil {
		return envelope{}, err
	}
	defer f.Close()
	data, err := io.ReadAll(io.LimitReader(f, filemodel.MaxWire+1))
	if err != nil {
		return envelope{}, err
	}
	return decodeEnvelope(data)
}

func saveSnapshot(root string, e envelope) error {
	if err := validateEnvelope(e); err != nil {
		return err
	}
	data, err := json.Marshal(e)
	if err != nil {
		return err
	}
	f, err := os.CreateTemp(root, "snapshot-pending-")
	if err != nil {
		return err
	}
	name := f.Name()
	defer os.Remove(name)
	_, writeErr := f.Write(data)
	closeErr := f.Close()
	if writeErr != nil {
		return writeErr
	}
	if closeErr != nil {
		return closeErr
	}
	// Rename tests process-reopen publication, not power-loss durability.
	return os.Rename(name, filepath.Join(root, "snapshot.json"))
}

func workloadQueries() []query {
	queries := []query{}
	for _, order := range []string{"name", "kind", "size", "modified"} {
		for _, reverse := range []bool{false, true} {
			for _, filter := range []string{"", "ALPHA", "σigma", "İ", "[100%]_", `\xff`, "absent"} {
				queries = append(queries, query{order, reverse, filter})
			}
		}
	}
	return queries
}

// FTS is a separate ASCII-token/prefix experiment. The browser's real literal
// Unicode-lowercase substring filter above is never replaced with token search.
func tokenMatch(text, prefix string) bool {
	for _, token := range strings.FieldsFunc(strings.ToLower(text), func(r rune) bool {
		return r < 'a' || r > 'z'
	}) {
		if strings.HasPrefix(token, prefix) {
			return true
		}
	}
	return false
}

func snapshotControls(root string) ([]control, error) {
	e, _ := corpus(12)
	if err := saveSnapshot(root, e); err != nil {
		return nil, err
	}
	path := filepath.Join(root, "snapshot.json")
	valid, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	for _, data := range [][]byte{[]byte("broken"), valid[:len(valid)/2], bytes.Replace(valid, []byte(`"probe_version":1`), []byte(`"probe_version":2`), 1)} {
		if err := os.WriteFile(path, data, 0600); err != nil {
			return nil, err
		}
		if _, err := loadSnapshot(path); err == nil {
			return nil, errors.New("invalid snapshot accepted")
		}
	}
	if err := saveSnapshot(root, e); err != nil {
		return nil, err
	}
	got, err := loadSnapshot(path)
	if err != nil || !reflect.DeepEqual(e, got) {
		return nil, errors.New("snapshot clean rebuild failed")
	}
	return []control{{"snapshot-invalid-rebuild", "passed", "corrupt/truncated/wrong-version refuse; clean owned rewrite recovers"}}, nil
}

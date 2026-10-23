package filemodel

import (
	"bytes"
	"math"
	"os"
	"path/filepath"
	"reflect"
	"runtime"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
)

func TestSelectionValidatesLiteralPathsAndCompleteObservations(t *testing.T) {
	root := t.TempDir()
	names := [][]byte{[]byte("-quote \" $[value];`\n"), {'r', 'a', 'w', '-', 0xff}}
	// APFS cannot create invalid UTF-8 names; both supported native targets
	// exercise the original byte case, while host policy retains the other names.
	if runtime.GOOS == "darwin" {
		names[1] = []byte("raw-host")
	}
	for _, name := range names {
		if err := os.WriteFile(filepath.Join(root, string(name)), []byte("before"), 0600); err != nil {
			t.Fatal(err)
		}
	}
	s, err := Scan(root, false)
	if err != nil {
		t.Fatal(err)
	}
	ids := []string{s.Items[1].ID, s.Items[0].ID}
	paths, err := s.ValidateSelection(ids)
	if err != nil || paths[0] != filepath.Join(root, string(s.Items[1].Name)) || paths[1] != filepath.Join(root, string(s.Items[0].Name)) {
		t.Fatal(paths, err)
	}
	os.WriteFile(paths[1], []byte("changed observation"), 0600)
	if paths, err := s.ValidateSelection(ids); err == nil || paths != nil {
		t.Fatal("partial stale selection returned", paths, err)
	}
}

func TestSelectionRejectsUnknownDuplicateUnsupportedAndReplacedRoot(t *testing.T) {
	root := t.TempDir()
	os.WriteFile(filepath.Join(root, "file"), nil, 0600)
	os.Symlink(filepath.Join(root, "file"), filepath.Join(root, "link"))
	s, _ := Scan(root, false)
	id := s.Items[0].ID
	for _, ids := range [][]string{nil, {id, id}, {id, "missing"}, {strings.Repeat("x", 65)}, make([]string, collection.MaxSelected+1), {s.Items[1].ID}} {
		if paths, err := s.ValidateSelection(ids); err == nil || paths != nil {
			t.Fatal("invalid selection accepted", ids, paths)
		}
	}
	s.Items = append(s.Items, s.Items[0])
	if _, err := s.ValidateSelection([]string{id}); err == nil {
		t.Fatal("ambiguous snapshot accepted")
	}
	s.Items = s.Items[:2]
	moved := root + "-old"
	os.Rename(root, moved)
	defer os.RemoveAll(moved)
	os.Mkdir(root, 0700)
	if _, err := s.ValidateSelection([]string{id}); err == nil {
		t.Fatal("replaced parent accepted")
	}
	var absent *Snapshot
	if _, err := absent.ValidateSelection([]string{id}); err == nil {
		t.Fatal("missing snapshot accepted")
	}
}

func TestSelectionArgumentBudgetsPrecedeFilesystemWork(t *testing.T) {
	// This admitted metadata names a nonexistent directory. Budget rejection
	// must precede its stat; the result never contains a truncated argument list.
	root := "/" + strings.Repeat("d", 999)
	s := &Snapshot{Schema: 1, Path: []byte(root)}
	ids := []string{}
	for i := 0; i < 33; i++ {
		id := strings.Repeat("x", i+1)
		ids = append(ids, id)
		s.Items = append(s.Items, Item{ID: id, Name: []byte(id), Kind: "file"})
	}
	if paths, err := s.ValidateSelection(ids); err == nil || paths != nil || !strings.Contains(err.Error(), "argument budget") {
		t.Fatal(paths, err)
	}
	s.Items = []Item{{ID: "one", Name: bytes.Repeat([]byte("x"), MaxPath+1), Kind: "file"}}
	if _, err := s.ValidateSelection([]string{"one"}); err == nil || !strings.Contains(err.Error(), "path limit") {
		t.Fatal(err)
	}
	longRoot := *s
	longRoot.Path = bytes.Repeat([]byte("x"), MaxPath+1)
	if _, err := longRoot.ValidateSelection([]string{"one"}); err == nil || !strings.Contains(err.Error(), "snapshot path") {
		t.Fatal("oversize root reached path construction", err)
	}
	for _, name := range []string{".", "..", "a/b", "nul\x00"} {
		s.Items[0].Name = []byte(name)
		if _, err := s.ValidateSelection([]string{"one"}); err == nil || !strings.Contains(err.Error(), "invalid file target") {
			t.Fatal(name, err)
		}
	}
}

func TestObservedSelectionBoundsTotalsAndNoFilesystemAccess(t *testing.T) {
	s := &Snapshot{Schema: 1, Path: []byte("/unavailable"), Items: []Item{
		{ID: "file", Kind: "file", Identity: Identity{Size: 12}},
		{ID: "folder", Kind: "directory", Identity: Identity{Size: 99999}},
		{ID: "link", Kind: "symlink", Identity: Identity{Size: 123}},
		{ID: "special", Kind: "special"},
	}}
	ids := []string{"file", "folder", "link", "special"}
	before := append([]Item(nil), s.Items...)
	details, err := s.ObservedSelection(ids)
	if err != nil || !strings.Contains(strings.Join(details, "\n"), "Regular-file bytes: 12; 1 links; 1 special entries") || !reflect.DeepEqual(before, s.Items) {
		t.Fatal(details, err)
	}
	for _, bad := range [][]string{nil, {"file", "file"}, {"file", "missing"}} {
		if _, err := s.ObservedSelection(bad); err == nil {
			t.Fatal("invalid properties admitted", bad)
		}
	}
	s.Items[0].Identity.Size = math.MaxInt64
	s.Items[1].Kind = "file"
	s.Items[1].Identity.Size = 1
	if _, err := s.ObservedSelection([]string{"file", "folder"}); err == nil {
		t.Fatal("overflow accepted")
	}
	s.Items[0].Identity.Size = -1
	if _, err := s.ObservedSelection([]string{"file", "folder"}); err == nil {
		t.Fatal("negative total accepted")
	}
}

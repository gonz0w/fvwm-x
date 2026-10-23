package filemodel

import (
	"math"
	"os"
	"reflect"
	"strings"
	"testing"
)

func TestOrderingUsesObservedMetadataAndLiteralNames(t *testing.T) {
	s := &Snapshot{Items: []Item{
		{ID: "a", Name: []byte("a"), Kind: "file", Identity: Identity{Size: math.MaxInt64, MTime: math.MinInt64}},
		{ID: "z", Name: []byte("z"), Kind: "file", Identity: Identity{Size: math.MinInt64, MTime: math.MaxInt64}},
		{ID: "d", Name: []byte("folder"), Kind: "directory"},
		{ID: "link", Name: []byte{0xff}, Kind: "symlink", Identity: Identity{Mode: os.ModeSymlink}},
	}}
	before := append([]Item(nil), s.Items...)
	for _, check := range []struct {
		key     string
		reverse bool
		ids     []string
	}{
		{"name", false, []string{"d", "a", "z", "link"}},
		{"name", true, []string{"d", "link", "z", "a"}},
		{"kind", false, []string{"d", "a", "z", "link"}},
		{"size", false, []string{"d", "z", "link", "a"}},
		{"modified", false, []string{"d", "a", "link", "z"}},
		{"modified", true, []string{"d", "z", "link", "a"}},
	} {
		entries, err := s.OrderedEntries(check.key, check.reverse)
		if err != nil {
			t.Fatal(err)
		}
		ids := []string{}
		for _, e := range entries {
			ids = append(ids, e.ID)
		}
		if !reflect.DeepEqual(ids, check.ids) {
			t.Fatal(check.key, check.reverse, ids)
		}
	}
	if !reflect.DeepEqual(s.Items, before) {
		t.Fatal("sort mutated domain observations")
	}
	if _, err := s.OrderedEntries("unknown", false); err == nil {
		t.Fatal("unknown ordering")
	}
	// No path is available: this metadata inspection must not touch the filesystem.
	details, err := s.ObservedDetails("link")
	if err != nil || len(details) != 3 || !strings.Contains(details[1], "symlink") {
		t.Fatal(details, err)
	}
	if _, err := s.ObservedDetails("gone"); err == nil {
		t.Fatal("unknown metadata target")
	}
}

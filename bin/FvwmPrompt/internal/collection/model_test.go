package collection

import (
	"fmt"
	"reflect"
	"testing"
)

func entries(count int) []Entry {
	value := make([]Entry, count)
	for i := range value {
		value[i] = Entry{ID: fmt.Sprintf("item%d", i), Label: "same display label", Kind: "fixture"}
	}
	return value
}

func TestRefreshPreservesOnlySurvivingIdentities(t *testing.T) {
	var m Model
	if err := m.Replace(entries(20)); err != nil {
		t.Fatal(err)
	}
	if err := m.SelectPage(m.Generation, 0, []string{"item0", "item3"}); err != nil {
		t.Fatal(err)
	}
	if err := m.SelectPage(m.Generation, 12, []string{"item15"}); err != nil {
		t.Fatal(err)
	}
	m.Focus = "item3"
	next := entries(20)[2:]
	next[1].Label = "renamed display only"
	if err := m.Replace(next); err != nil {
		t.Fatal(err)
	}
	if m.Generation != 2 || m.Focus != "item3" || !reflect.DeepEqual(m.SelectedIDs(), []string{"item3", "item15"}) {
		t.Fatalf("%+v", m)
	}
	if err := m.SelectPage(1, 0, nil); err == nil {
		t.Fatal("accepted stale generation")
	}
}

func TestPageSelectionRefusesAtomically(t *testing.T) {
	var m Model
	m.Replace(entries(20))
	m.SelectPage(1, 0, []string{"item1"})
	for _, ids := range [][]string{{"item1", "item1"}, {"item15"}, {"unknown"}} {
		if err := m.SelectPage(1, 0, ids); err == nil {
			t.Fatal("accepted", ids)
		}
		if !reflect.DeepEqual(m.SelectedIDs(), []string{"item1"}) {
			t.Fatal("partial selection mutation")
		}
	}
	for _, offset := range []int{-1, 1, 24} {
		if _, err := m.Page(offset); err == nil {
			t.Fatal("accepted page", offset)
		}
	}
	page, _ := m.Page(12)
	if len(page) != 8 {
		t.Fatal(len(page))
	}
	page[0].Label = "caller copy"
	if m.Entries[12].Label == page[0].Label {
		t.Fatal("borrowed mutable page")
	}
}

func TestBoundsAndReplacementPublication(t *testing.T) {
	var m Model
	m.Replace(entries(MaxItems))
	for offset := 0; offset < MaxSelected; offset += PageSize {
		page, _ := m.Page(offset)
		ids := []string{}
		for _, e := range page {
			if len(m.Selected)+len(ids) < MaxSelected {
				ids = append(ids, e.ID)
			}
		}
		if err := m.SelectPage(m.Generation, offset, ids); err != nil {
			t.Fatal(err)
		}
	}
	if len(m.Selected) != MaxSelected {
		t.Fatal(len(m.Selected))
	}
	if err := m.SelectPage(m.Generation, 264, []string{"item264"}); err == nil {
		t.Fatal("selection cap bypass")
	}
	for _, bad := range [][]Entry{entries(MaxItems + 1), {{ID: "duplicate"}, {ID: "duplicate"}}, {{ID: ""}}} {
		if err := m.Replace(bad); err == nil {
			t.Fatal("invalid replacement accepted")
		}
		if m.Generation != 1 || len(m.Entries) != MaxItems || len(m.Selected) != MaxSelected {
			t.Fatal("replacement was partially published")
		}
	}
}

func TestFilteredProjectionRetainsIdentityAndSelection(t *testing.T) {
	var m Model
	m.Replace([]Entry{{ID: "a", Label: "Alpha"}, {ID: "b", Label: "beta"}, {ID: "c", Label: "Alphabet"}})
	m.SelectPage(m.Generation, 0, []string{"a", "b"})
	m.Focus = "b"
	previous := m.Generation
	if err := m.SetFilter("ALP"); err != nil {
		t.Fatal(err)
	}
	page, _ := m.Page(0)
	if len(page) != 2 || page[1].ID != "c" || m.HiddenSelectedCount() != 1 || m.Focus != "a" {
		t.Fatal(page, m.HiddenSelectedCount(), m.Focus)
	}
	if err := m.SelectPage(previous, 0, nil); err == nil {
		t.Fatal("old projection selected new rows")
	}
	if err := m.SelectPage(m.Generation, 0, []string{"c"}); err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(m.SelectedIDs(), []string{"b", "c"}) {
		t.Fatal("filtered selection lost", m.SelectedIDs())
	}
	m.Replace([]Entry{{ID: "c", Label: "Alphabet"}, {ID: "b", Label: "beta"}})
	if m.Filter() != "ALP" || m.VisibleCount() != 1 || m.HiddenSelectedCount() != 1 || m.Focus != "c" {
		t.Fatal("refresh discarded projection policy")
	}
	m.SetFilter("no matches")
	page, err := m.Page(0)
	if err != nil || len(page) != 0 || m.Focus != "" || len(m.SelectedIDs()) != 2 {
		t.Fatal("empty view lost domain selection", err)
	}
	m.SetFilter("")
	if m.VisibleCount() != 2 || m.HiddenSelectedCount() != 0 {
		t.Fatal("clear filter lost entries")
	}
}

func TestProjectionBoundsAndBulkSelection(t *testing.T) {
	var m Model
	m.Replace(entries(MaxItems))
	m.SetFilter("same")
	generation := m.Generation
	for _, bad := range []string{string(make([]byte, MaxFilter+1)), "\xff", "a\x00b"} {
		if m.SetFilter(bad) == nil || m.Generation != generation || m.Filter() != "same" {
			t.Fatal("invalid projection partially published")
		}
	}
	for offset := 0; offset < 252; offset += PageSize {
		if err := m.SelectWholePage(m.Generation, offset); err != nil {
			t.Fatal(err)
		}
	}
	before := m.SelectedIDs()
	if m.SelectWholePage(m.Generation, 252) == nil || !reflect.DeepEqual(before, m.SelectedIDs()) {
		t.Fatal("bulk cap refused non-atomically")
	}
	if m.ClearSelection(generation-1) == nil || len(m.SelectedIDs()) != 252 {
		t.Fatal("stale clear changed selection")
	}
	if m.ClearSelection(generation) != nil || len(m.SelectedIDs()) != 0 {
		t.Fatal("clear failed")
	}
	if m.OffsetFor("item25") != 24 || m.OffsetFor("missing") != 0 {
		t.Fatal("bad visible anchor")
	}
	m.Generation = ^uint64(0)
	if m.SetFilter("new") == nil || m.Filter() != "same" {
		t.Fatal("generation wrapped")
	}
}

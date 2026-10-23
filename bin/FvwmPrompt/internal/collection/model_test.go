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

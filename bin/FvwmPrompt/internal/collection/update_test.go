package collection

import (
	"reflect"
	"strings"
	"testing"
)

func TestDeltaPublishesOneOwnedOrderedTransaction(t *testing.T) {
	var m Model
	m.Replace(entries(4))
	m.SelectIDs(m.Generation, []string{"item0", "item1"})
	m.Focus = "item1"
	delta := Delta{Remove: []string{"item1"}, Upsert: []Entry{
		{ID: "item2", Label: "changed", Revision: "revision2"},
		{ID: "new", Label: "new entry"},
	}, Order: []string{"new", "item3", "item2", "item0"}}
	result, err := m.Apply(m.Generation, delta)
	if err != nil || result != (Changes{Added: 1, Updated: 1, Removed: 1, SelectionLost: 1}) {
		t.Fatal(result, err)
	}
	if m.Generation != 2 || m.Focus != "item2" || !reflect.DeepEqual(m.SelectedIDs(), []string{"item0"}) {
		t.Fatalf("state after transaction: %+v", m)
	}
	delta.Upsert[0].Label = "caller changed"
	delta.Order[0] = "caller changed"
	if m.Entries[2].Label != "changed" || m.Entries[0].ID != "new" {
		t.Fatal("retained caller-owned update storage")
	}
}

func TestInvalidDeltaNeverPublishesPartialState(t *testing.T) {
	cases := map[string]Delta{
		"oversize removed ID":    {Remove: []string{strings.Repeat("x", 65)}},
		"oversize ordered ID":    {Order: []string{"item0", strings.Repeat("x", 65)}},
		"combined change budget": {Remove: []string{"item0"}, Upsert: entries(MaxChanges)},
		"unknown remove":         {Remove: []string{"missing"}, Order: []string{"item0", "item1"}},
		"duplicate remove":       {Remove: []string{"item1", "item1"}, Order: []string{"item0"}},
		"conflict":               {Remove: []string{"item1"}, Upsert: []Entry{{ID: "item1"}}, Order: []string{"item0", "item1"}},
		"duplicate upsert":       {Upsert: []Entry{{ID: "new"}, {ID: "new"}}, Order: []string{"item0", "item1", "new"}},
		"missing order":          {Remove: []string{"item1"}},
		"unknown order":          {Order: []string{"item0", "missing"}},
		"duplicate order":        {Order: []string{"item0", "item0"}},
		"oversize changes":       {Upsert: entries(MaxChanges + 1)},
		"oversize order":         {Order: make([]string, MaxItems+1)},
		"invalid entry":          {Upsert: []Entry{{ID: "item0", Revision: strings.Repeat("x", 65)}}, Order: []string{"item0", "item1"}},
	}
	for name, delta := range cases {
		t.Run(name, func(t *testing.T) {
			var m Model
			m.Replace(entries(2))
			m.SelectIDs(m.Generation, []string{"item1"})
			m.SetFilter("same")
			m.Focus = "item1"
			before := m
			before.Entries = append([]Entry(nil), m.Entries...)
			before.visible = append([]int(nil), m.visible...)
			before.Selected = map[string]bool{"item1": true}
			if _, err := m.Apply(m.Generation, delta); err == nil || !reflect.DeepEqual(m, before) {
				t.Fatalf("invalid update published: %v %+v", err, m)
			}
		})
	}
	var m Model
	m.Replace(entries(1))
	for _, generation := range []uint64{0, 2} {
		if _, err := m.Apply(generation, Delta{Order: []string{"item0"}}); err == nil {
			t.Fatal("stale generation accepted")
		}
	}
	m.Generation = ^uint64(0)
	if _, err := m.Apply(m.Generation, Delta{Remove: []string{"item0"}}); err == nil || len(m.Entries) != 1 {
		t.Fatal("exhausted generation changed")
	}
}

func TestReconcileNoOpLargeFallbackAndReorder(t *testing.T) {
	var m Model
	initial := entries(MaxItems)
	m.Replace(initial)
	m.SelectIDs(m.Generation, []string{"item4095"})
	m.Focus = "item4095"
	if result, err := m.Reconcile(m.Generation, initial); err != nil || result != (Changes{}) || m.Generation != 1 {
		t.Fatal(result, err, m.Generation)
	}
	next := entries(MaxItems)
	for i := 0; i <= MaxChanges; i++ {
		next[i].Revision = "changed observation"
	}
	result, err := m.Reconcile(m.Generation, next)
	if err != nil || !result.Replaced || result.Updated != MaxChanges+1 || m.Generation != 2 || m.Focus != "item4095" || !m.Selected["item4095"] {
		t.Fatal(result, err)
	}
	next[0], next[1] = next[1], next[0]
	if result, err := m.Reconcile(m.Generation, next); err != nil || result != (Changes{}) || m.Generation != 3 || m.Entries[0].ID != "item1" {
		t.Fatal("reorder lost", result, err)
	}
	if _, err := m.Reconcile(2, nil); err == nil || m.Generation != 3 {
		t.Fatal("stale snapshot published")
	}
	if _, err := m.Reconcile(3, entries(MaxItems+1)); err == nil || m.Generation != 3 {
		t.Fatal("over-limit snapshot published")
	}
}

func TestRefreshFocusNeighborsAndFilteredSelection(t *testing.T) {
	var m Model
	m.Replace([]Entry{{ID: "hidden", Label: "hidden"}, {ID: "a", Label: "visible"},
		{ID: "b", Label: "visible"}, {ID: "c", Label: "visible"}})
	m.SelectIDs(m.Generation, []string{"hidden", "b"})
	m.SetFilter("visible")
	m.Focus = "b"
	if _, err := m.Reconcile(m.Generation, []Entry{{ID: "hidden", Label: "hidden"},
		{ID: "a", Label: "visible"}, {ID: "c", Label: "visible"}}); err != nil || m.Focus != "c" || m.HiddenSelectedCount() != 1 {
		t.Fatal(m, err)
	}
	m.Reconcile(m.Generation, []Entry{{ID: "hidden", Label: "hidden"}, {ID: "a", Label: "visible"}})
	if m.Focus != "a" {
		t.Fatal("previous visible neighbor lost", m.Focus)
	}
	m.Reconcile(m.Generation, []Entry{{ID: "hidden", Label: "hidden"}, {ID: "new", Label: "visible"}})
	if m.Focus != "new" {
		t.Fatal("new visible fallback lost", m.Focus)
	}
	m.Reconcile(m.Generation, []Entry{{ID: "hidden", Label: "hidden"}})
	if m.Focus != "" || m.VisibleCount() != 0 || !m.Selected["hidden"] {
		t.Fatal(m)
	}
}

func TestDeltaMaximumChangeAndResultBudgets(t *testing.T) {
	var m Model
	m.Replace(entries(MaxItems))
	delta := Delta{Order: make([]string, MaxItems)}
	for i, entry := range m.Entries {
		delta.Order[i] = entry.ID
		if i < MaxChanges {
			entry.Revision = "new"
			delta.Upsert = append(delta.Upsert, entry)
		}
	}
	if result, err := m.Apply(m.Generation, delta); err != nil || result.Updated != MaxChanges || result.Replaced {
		t.Fatal(result, err)
	}
	before := m.Generation
	delta.Upsert = []Entry{{ID: "extra"}}
	delta.Order = append(delta.Order, "extra")
	if _, err := m.Apply(before, delta); err == nil || m.Generation != before {
		t.Fatal("over-limit result published")
	}
}

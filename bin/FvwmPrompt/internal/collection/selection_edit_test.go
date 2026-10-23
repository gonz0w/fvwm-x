package collection

import (
	"fmt"
	"reflect"
	"testing"
)

func TestSelectionEditsAtomicAcrossHiddenAndUnknownIDs(t *testing.T) {
	var model Model
	if err := model.Replace([]Entry{{ID: "a", Label: "visible"}, {ID: "b", Label: "hidden"}, {ID: "c", Label: "third"}}); err != nil {
		t.Fatal(err)
	}
	model.SelectIDs(model.Generation, []string{"b"})
	model.SetFilter("visible")
	for _, edit := range []struct {
		mode string
		ids  []string
	}{
		{"add", []string{"a", "unknown"}}, {"remove", []string{"unknown"}},
		{"replace", []string{"a", "a"}}, {"other", []string{"a"}}, {"clear", []string{"a"}},
	} {
		if err := model.EditSelection(model.Generation, edit.mode, edit.ids); err == nil || !reflect.DeepEqual(model.SelectedIDs(), []string{"b"}) {
			t.Fatal("refused edit changed selection", edit, err, model.SelectedIDs())
		}
	}
	if model.EditSelection(model.Generation-1, "clear", nil) == nil {
		t.Fatal("stale clear accepted")
	}
	if err := model.EditSelection(model.Generation, "add", []string{"a", "b"}); err != nil || !reflect.DeepEqual(model.SelectedIDs(), []string{"a", "b"}) || model.HiddenSelectedCount() != 1 {
		t.Fatal(err, model.SelectedIDs())
	}
	if err := model.EditSelection(model.Generation, "remove", []string{"b", "c"}); err != nil || !reflect.DeepEqual(model.SelectedIDs(), []string{"a"}) {
		t.Fatal(err, model.SelectedIDs())
	}
	if err := model.EditSelection(model.Generation, "replace", []string{"c"}); err != nil || !reflect.DeepEqual(model.SelectedIDs(), []string{"c"}) {
		t.Fatal(err, model.SelectedIDs())
	}
	if err := model.EditSelection(model.Generation, "clear", nil); err != nil || len(model.Selected) != 0 {
		t.Fatal(err, model.SelectedIDs())
	}
}

func TestSelectionEditBudgetRefusesWholeAdd(t *testing.T) {
	var model Model
	entries := make([]Entry, MaxSelected+1)
	ids := make([]string, MaxSelected)
	for i := range entries {
		entries[i] = Entry{ID: fmt.Sprint(i), Label: fmt.Sprint(i)}
		if i < len(ids) {
			ids[i] = entries[i].ID
		}
	}
	model.Replace(entries)
	model.SelectIDs(model.Generation, ids)
	before := model.SelectedIDs()
	if model.EditSelection(model.Generation, "add", []string{entries[MaxSelected].ID}) == nil || !reflect.DeepEqual(model.SelectedIDs(), before) {
		t.Fatal("over-limit edit partially published")
	}
}

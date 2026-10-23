package main

import (
	"encoding/json"
	"reflect"
	"strings"
	"testing"
)

func actionQuery(b *browser, operation, values string) fileQuery {
	return fileQuery{Version: 1, Serial: "0000000000000001", Provider: b.instance,
		Scope: "action", Generation: b.providerGeneration(), Operation: operation,
		Values: json.RawMessage(values)}
}

func TestProviderActionSchemaAndMalformedText(t *testing.T) {
	b, _ := providerFixture(t)
	for _, c := range []struct{ operation, values string }{
		{"selection", `{"mode":"replace","ids":["00000000000000000000000000000001"]}`},
		{"selection", `{"mode":"clear"}`}, {"filter", `{"text":""}`},
		{"filter", `{"text":"literal \" $[exec.touch] ;\n"}`},
		{"sort", `{"key":"size","reverse":false}`}, {"view", `{"mode":"grid"}`}, {"properties", `{}`},
	} {
		q := actionQuery(&b, c.operation, c.values)
		if !b.validFileQuery(q) {
			t.Fatal("valid frame refused", c)
		}
		if _, err := decodeProviderAction(q); err != nil {
			t.Fatal(c, err)
		}
	}
	for _, c := range []struct{ operation, values string }{
		{"selection", `{"mode":"clear","ids":[]}`}, {"selection", `{"mode":"add","ids":[]}`},
		{"selection", `{"mode":"replace","ids":["unknown"]}`},
		{"selection", `{"mode":"add","ids":["00000000000000000000000000000001","00000000000000000000000000000001"]}`},
		{"filter", `{"text":"first","text":"second"}`}, {"filter", `{"text":null}`}, {"filter", `{"text":"\u0000"}`},
		{"filter", "{\"text\":\"\xff\"}"}, {"filter", `{"text":"` + strings.Repeat("x", 129) + `"}`},
		{"sort", `{"key":"size","reverse":null}`}, {"sort", `{"key":"size","reverse":0}`},
		{"sort", `{"key":"not-a-sort","reverse":false}`}, {"view", `{"mode":"grid","text":"extra"}`},
		{"view", `{"mode":"other"}`}, {"properties", `{"extra":true}`}, {"properties", `{} {}`},
	} {
		if _, err := decodeProviderAction(actionQuery(&b, c.operation, c.values)); err == nil {
			t.Fatal("invalid schema accepted", c)
		}
	}
	q := actionQuery(&b, "properties", `{}`)
	q.Generation = ""
	if b.validFileQuery(q) {
		t.Fatal("unconditional action admitted")
	}
}

func TestProviderConditionalEditsAndAtomicRefusal(t *testing.T) {
	b, _ := providerFixture(t)
	apply := func(operation string, values any) string {
		t.Helper()
		data, _ := json.Marshal(values)
		q := actionQuery(&b, operation, string(data))
		action, err := decodeProviderAction(q)
		if err != nil {
			t.Fatal(err)
		}
		return b.applyProviderAction(q, action)
	}
	ids := []string{b.model.Entries[0].ID, b.model.Entries[1].ID}
	stale := actionQuery(&b, "selection", `{"mode":"clear"}`)
	if code := apply("selection", map[string]any{"mode": "replace", "ids": ids}); code != "" || !b.replaceSelection {
		t.Fatal(code)
	}
	before := b.model.SelectedIDs()
	action, _ := decodeProviderAction(stale)
	if code := b.applyProviderAction(stale, action); code != "stale_generation" || !reflect.DeepEqual(before, b.model.SelectedIDs()) {
		t.Fatal(code, b.model.SelectedIDs())
	}
	if code := apply("selection", map[string]any{"mode": "add", "ids": []string{b.model.Entries[2].ID, strings.Repeat("f", 32)}}); code != "invalid_selection" || !reflect.DeepEqual(before, b.model.SelectedIDs()) {
		t.Fatal("unknown ID partially applied", code)
	}
	if code := apply("filter", map[string]any{"text": "item-00"}); code != "" || b.model.HiddenSelectedCount() != 1 {
		t.Fatal(code, b.model.HiddenSelectedCount())
	}
	if code := apply("sort", map[string]any{"key": "size", "reverse": true}); code != "" || b.ordering() != "size" || !b.reverse || len(b.model.Selected) != 2 {
		t.Fatal(code)
	}
	previous := b.providerGeneration()
	if code := apply("view", map[string]any{"mode": "grid"}); code != "" || b.viewing() != "grid" || b.providerGeneration() == previous {
		t.Fatal("view did not invalidate state token", code)
	}
	if code := apply("properties", map[string]any{}); code != "" || !reflect.DeepEqual(b.inspected, b.model.SelectedIDs()) {
		t.Fatal(code, b.inspected)
	}
	if code := apply("selection", map[string]any{"mode": "remove", "ids": ids[:1]}); code != "" || len(b.model.Selected) != 1 || b.inspected != nil {
		t.Fatal(code)
	}
	if code := apply("selection", map[string]any{"mode": "clear"}); code != "" || len(b.model.Selected) != 0 {
		t.Fatal(code)
	}
	if code := apply("properties", map[string]any{}); code != "selection_unavailable" {
		t.Fatal(code)
	}
	b.snapshot = nil
	if code := b.applyProviderAction(actionQuery(&b, "properties", `{}`), providerAction{}); code != "not_ready" {
		t.Fatal(code)
	}
}

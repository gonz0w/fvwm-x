package main

import "testing"

func TestAdvertisedProviderOperationsHaveAdmittedSchemasAndRoutes(t *testing.T) {
	// Wire-contract examples are independent of the capability catalog. A new
	// advertised operation needs both a valid schema and the correct owner.
	examples := map[string]struct {
		values string
		worker bool
	}{
		"selection":             {`{"mode":"clear"}`, false},
		"filter":                {`{"text":"literal"}`, false},
		"sort":                  {`{"key":"name","reverse":false}`, false},
		"view":                  {`{"mode":"list"}`, false},
		"properties":            {`{}`, false},
		"navigate":              {`{"mode":"home"}`, true},
		"refresh":               {`{}`, true},
		"open":                  {`{"id":"00000000000000000000000000000001"}`, true},
		"terminal":              {`{}`, true},
		"custom":                {`{"index":0}`, true},
		"cancel":                {`{"id":"0000000000000001"}`, false},
		"reveal":                {`{"paths_bytes":["L2E="]}`, true},
		"inspect":               {`{"paths_bytes":["L2E="]}`, true},
		"hidden":                {`{"enabled":false}`, true},
		"watch":                 {`{"enabled":false}`, false},
		"present":               {`{}`, false},
		"file-roles":            {`{}`, true},
		"file-roles-close":      {`{}`, false},
		"file-role-launch":      {`{"generation":"0123456789abcdef0123456789abcdef","id":"0123456789abcdef0123456789abcdef"}`, true},
		"directory-roles":       {`{}`, true},
		"directory-roles-close": {`{}`, false},
		"directory-role-launch": {`{"generation":"0123456789abcdef0123456789abcdef","id":"0123456789abcdef0123456789abcdef"}`, true},
	}
	b, query := providerFixture(t)
	query.Scope = "state"
	state, code := b.providerResult(query)
	if code != "" {
		t.Fatal(code)
	}
	seen := make(map[string]bool)
	for _, name := range state["action_operations"].([]string) {
		example, found := examples[name]
		if !found || seen[name] {
			t.Fatal("missing contract example or duplicate capability", name)
		}
		seen[name] = true
		request := actionQuery(&b, name, example.values)
		if !b.validFileQuery(request) || providerWorkerAction(name) != example.worker {
			t.Fatal("capability admission/owner mismatch", name)
		}
		if _, err := decodeProviderAction(request); err != nil {
			t.Fatal(name, err)
		}
	}
	if len(seen) != len(examples) {
		t.Fatal("supported operation disappeared", seen)
	}
	if b.validFileQuery(actionQuery(&b, "unknown", `{}`)) || providerWorkerAction("unknown") {
		t.Fatal("unknown operation admitted")
	}
	// Modifying returned metadata must not change later admission/capabilities.
	state["action_operations"].([]string)[0] = "unknown"
	next, _ := b.providerResult(query)
	if next["action_operations"].([]string)[0] != "selection" {
		t.Fatal("mutable capability response escaped into the catalog")
	}
}

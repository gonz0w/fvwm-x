package main

import (
	"encoding/base64"
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

func TestProviderTargetSchemasAndCapabilities(t *testing.T) {
	b, _ := providerFixture(t)
	for _, op := range []string{"reveal", "inspect"} {
		for _, values := range []string{`{"paths_bytes":["L2E="]}`, `{"paths_bytes":["L2E=","L2I="]}`} {
			q := actionQuery(&b, op, values)
			a, err := decodeProviderAction(q)
			if !b.validFileQuery(q) || err != nil || string(a.targets[0]) != "/a" {
				t.Fatal(q, a, err)
			}
		}
		for _, values := range []string{`{"paths_bytes":null}`, `{"paths_bytes":[]}`, `{"paths_bytes":[null]}`, `{"paths_bytes":["L2E=","L2E="]}`, `{"paths_bytes":["L2E="] ,"extra":true}`, `{"paths_bytes":["Lx=="]}`} {
			if _, err := decodeProviderAction(actionQuery(&b, op, values)); err == nil {
				t.Fatal("admitted", op, values)
			}
		}
		paths := []string{}
		for i := 0; i < 9; i++ {
			paths = append(paths, base64.StdEncoding.EncodeToString([]byte{'/', byte('a' + i)}))
		}
		data, _ := json.Marshal(map[string]any{"paths_bytes": paths[:8]})
		if _, err := decodeProviderAction(actionQuery(&b, op, string(data))); err != nil {
			t.Fatal(err)
		}
		data, _ = json.Marshal(map[string]any{"paths_bytes": paths})
		if _, err := decodeProviderAction(actionQuery(&b, op, string(data))); err == nil {
			t.Fatal("target cap ignored")
		}
	}
	for _, op := range []string{"hidden", "watch"} {
		if _, err := decodeProviderAction(actionQuery(&b, op, `{"enabled":true}`)); err != nil {
			t.Fatal(err)
		}
		for _, values := range []string{`{}`, `{"enabled":null}`, `{"enabled":1}`, `{"enabled":true,"enabled":false}`} {
			if _, err := decodeProviderAction(actionQuery(&b, op, values)); err == nil {
				t.Fatal("invalid boolean admitted", values)
			}
		}
	}
	b.actions = actionFlags{{Label: "Review $[literal]", Program: "/private/executable"}}
	b.terminal = "/terminal"
	state, code := b.providerResult(fileQuery{Version: 1, Serial: "0000000000000001", Provider: b.instance, Scope: "state", Limit: 8})
	data, _ := json.Marshal(state)
	if code != "" || strings.Contains(string(data), "/private") || strings.Contains(string(data), "/terminal") {
		t.Fatal(code, string(data))
	}
	got := state.(map[string]any)
	if got["target_request_max"] != 8 || got["opener_available"] != false || got["terminal_available"] != true || len(got["configured_actions"].([]map[string]any)) != 1 {
		t.Fatal(got)
	}
}

func TestLiveLocatedPublicationClearsFilterAndReplacesSelection(t *testing.T) {
	root := t.TempDir()
	for _, name := range []string{"a", "b", ".hidden"} {
		if err := os.WriteFile(filepath.Join(root, name), []byte("data"), 0600); err != nil {
			t.Fatal(err)
		}
	}
	snapshot, err := filemodel.Scan(root, false)
	if err != nil {
		t.Fatal(err)
	}
	b := browser{snapshot: snapshot}
	b.model.Replace(snapshot.Entries())
	b.model.SelectIDs(b.model.Generation, []string{snapshot.Items[0].ID})
	b.model.SetFilter("no-match")
	paths := [][]byte{[]byte(filepath.Join(root, ".hidden")), []byte(filepath.Join(root, "b"))}
	request, navigation, code := b.planProviderWork("inspect", providerAction{targets: paths})
	if code != "" || request.Operation != "locate" || navigation != "properties" {
		t.Fatal(request, navigation, code)
	}
	result := perform(request)
	if result.Status != "loaded" {
		t.Fatal(result)
	}
	if err := b.publishLocated(result, navigation); err != nil {
		t.Fatal(err)
	}
	if b.model.Filter() != "" || !b.hidden || !b.replaceSelection || !b.revealFocus || b.model.Focus != result.Selected[0] || !reflect.DeepEqual(b.inspected, result.Selected) || len(b.model.Selected) != 2 || len(b.history) != 0 {
		t.Fatal(b.model, b.hidden, b.inspected, b.history)
	}
	for _, id := range result.Selected {
		if !b.model.Selected[id] {
			t.Fatal("requested ID not selected")
		}
	}
	// A missing or mixed-parent target never publishes a partial result.
	for _, bad := range [][][]byte{{paths[0], []byte(filepath.Join(root, "missing"))}, {paths[0], []byte(filepath.Join(t.TempDir(), "b"))}} {
		result := perform(workRequest{Operation: "locate", Targets: bad})
		if result.Status != "failed" || result.Snapshot != nil || len(result.Selected) != 0 {
			t.Fatal(result)
		}
	}
}

func TestLiveLocatedRefusalKeepsWholePriorState(t *testing.T) {
	b, _ := providerFixture(t)
	b.model.SetFilter("item-00")
	b.model.SelectIDs(b.model.Generation, []string{b.model.Entries[0].ID})
	b.history = []string{"/previous"}
	beforeModel, _ := json.Marshal(b.model)
	beforeSnapshot := b.snapshot
	beforeFilter := b.model.Filter()
	beforeGeneration := b.providerGeneration()
	bad := workResult{Status: "loaded", Snapshot: b.snapshot, Selected: []string{b.model.Entries[0].ID, strings.Repeat("f", 32)}}
	if b.publishLocated(bad, "properties") == nil {
		t.Fatal("invalid target set admitted")
	}
	after, _ := json.Marshal(b.model)
	if string(beforeModel) != string(after) || b.snapshot != beforeSnapshot || b.model.Filter() != beforeFilter || b.providerGeneration() != beforeGeneration || !reflect.DeepEqual(b.history, []string{"/previous"}) || b.revealFocus {
		t.Fatal("refusal changed browser state")
	}
}

func TestHiddenPlanningAndWatchControlsPreserveOwnedOutcome(t *testing.T) {
	b, _ := providerFixture(t)
	b.providerWork = &providerWork{ID: "0000000000000001", Operation: "refresh", Status: "loaded"}
	request, _, code := b.planProviderWork("hidden", providerAction{enabled: true})
	if code != "" || !request.Hidden || request.Operation != "scan" || b.hidden {
		t.Fatal(code, request)
	}
	before := b.providerGeneration()
	for _, enabled := range []bool{true, false} {
		q := actionQuery(&b, "watch", `{"enabled":true}`)
		if code := b.applyProviderAction(q, providerAction{enabled: enabled}); code != "" {
			t.Fatal(code)
		}
		if b.watch.enabled != enabled || b.providerWork.Status != "loaded" {
			t.Fatal("watch changed operation ownership")
		}
		if enabled && b.providerGeneration() == before {
			t.Fatal("watch control failed to invalidate snapshot")
		}
	}
	if b.watch.desired != nil {
		t.Fatal("disabled observer retained desired folder")
	}
}

package main

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestProviderWorkerSchemasAndLiteralPathBounds(t *testing.T) {
	b, _ := providerFixture(t)
	path := []byte("/literal ' $[exec.touch] ;\n\xff")
	encoded := base64.StdEncoding.EncodeToString(path)
	for _, c := range []struct{ op, values string }{
		{"navigate", `{"mode":"home"}`}, {"navigate", `{"mode":"up"}`}, {"navigate", `{"mode":"back"}`},
		{"navigate", `{"mode":"path","path_bytes":"` + encoded + `"}`},
		{"navigate", `{"mode":"item","id":"` + strings.Repeat("a", 32) + `"}`},
		{"open", `{"id":"` + strings.Repeat("a", 32) + `"}`}, {"refresh", `{}`}, {"terminal", `{}`},
		{"custom", `{"index":0}`}, {"custom", `{"index":3}`}, {"cancel", `{"id":"0000000000000001"}`},
	} {
		q := actionQuery(&b, c.op, c.values)
		if !b.validFileQuery(q) {
			t.Fatal(c)
		}
		a, err := decodeProviderAction(q)
		if err != nil {
			t.Fatal(c, err)
		}
		if a.mode == "path" && !bytes.Equal(a.path, path) {
			t.Fatal("path bytes changed")
		}
	}
	for _, c := range []struct{ op, values string }{
		{"navigate", `{"mode":"home","id":"` + strings.Repeat("a", 32) + `"}`},
		{"navigate", `{"mode":"path","path_bytes":"Lw==="}`}, {"navigate", `{"mode":"path","path_bytes":"Lx=="}`},
		{"navigate", `{"mode":"path","path_bytes":"AA=="}`}, {"navigate", `{"mode":"path","path_bytes":"eA=="}`},
		{"navigate", `{"mode":"path","path_bytes":"` + base64.StdEncoding.EncodeToString(append([]byte("/"), bytes.Repeat([]byte("x"), 192)...)) + `"}`},
		{"open", `{"id":"unknown"}`}, {"custom", `{"index":null}`}, {"custom", `{"index":4}`}, {"custom", `{"index":-1}`},
		{"custom", `{"index":1.5}`}, {"custom", `{"index":1,"index":0}`},
		{"cancel", `{"id":"0000000000000000"}`}, {"cancel", `{"id":"0000000000000001","mode":"clear"}`},
		{"terminal", `{"program":"untrusted"}`}, {"refresh", `{"extra":true}`},
	} {
		if _, err := decodeProviderAction(actionQuery(&b, c.op, c.values)); err == nil {
			t.Fatal("admitted", c)
		}
	}
}

func TestProviderWorkAdmissionAndRevalidatedDirectory(t *testing.T) {
	root := t.TempDir()
	child := filepath.Join(root, "folder")
	if err := os.Mkdir(child, 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "file"), []byte("data"), 0600); err != nil {
		t.Fatal(err)
	}
	snapshot, err := filemodel.Scan(root, false)
	if err != nil {
		t.Fatal(err)
	}
	b := browser{snapshot: snapshot, home: root, opener: "/unavailable", actions: actionFlags{{Program: "/configured"}}}
	entries, _ := snapshot.OrderedEntries("name", false)
	b.model.Replace(entries)
	var folderID, fileID string
	for _, item := range snapshot.Items {
		if item.Kind == "directory" {
			folderID = item.ID
		} else {
			fileID = item.ID
		}
	}
	for _, c := range []struct {
		op   string
		a    providerAction
		code string
	}{
		{"navigate", providerAction{mode: "back"}, "no_history"}, {"navigate", providerAction{mode: "item", id: fileID}, "not_directory"},
		{"open", providerAction{id: strings.Repeat("f", 32)}, "invalid_target"}, {"terminal", providerAction{}, "application_unavailable"},
		{"custom", providerAction{index: 1}, "action_unavailable"}, {"custom", providerAction{}, "selection_unavailable"},
	} {
		if _, _, code := b.planProviderWork(c.op, c.a); code != c.code {
			t.Fatal(c, code)
		}
	}
	request, _, code := b.planProviderWork("navigate", providerAction{mode: "item", id: folderID})
	if code != "" || perform(request).Status != "loaded" {
		t.Fatal(code, perform(request))
	}
	if err := os.Rename(child, child+"-old"); err != nil {
		t.Fatal(err)
	}
	if err := os.Mkdir(child, 0700); err != nil {
		t.Fatal(err)
	}
	if result := perform(request); result.Status != "failed" || result.Snapshot != nil {
		t.Fatal("replacement directory accepted", result)
	}
	b.model.SelectIDs(b.model.Generation, []string{fileID})
	request, _, code = b.planProviderWork("custom", providerAction{})
	if code != "" || request.Program != "/configured" || len(request.IDs) != 1 || request.IDs[0] != fileID {
		t.Fatal(code, request)
	}
	b.active = true
	if code := b.startProviderWork(actionQuery(&b, "refresh", `{}`), providerAction{}); code != "busy" || b.providerWork != nil {
		t.Fatal(code)
	}
}

func TestProviderWorkCancellationRetainsSlotAndCorrelatedOutcome(t *testing.T) {
	t.Setenv("FVWM_TEST_FILE_WORKER", "block")
	executable, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	b, _ := providerFixture(t)
	b.executable = executable
	b.results = make(chan completedWork, 1)
	before := b.providerGeneration()
	q := actionQuery(&b, "refresh", `{}`)
	if code := b.startProviderWork(q, providerAction{}); code != "" {
		t.Fatal(code)
	}
	if !b.active || b.providerWork.Status != "accepted" || b.providerGeneration() == before {
		t.Fatal(b.providerWork)
	}
	if code := b.cancelProviderWork("0000000000000002"); code != "operation_unavailable" {
		t.Fatal(code)
	}
	if code := b.cancelProviderWork(q.Serial); code != "" || !b.active || b.providerWork.Status != "cancelling" {
		t.Fatal(code)
	}
	if code := b.startProviderWork(q, providerAction{}); code != "busy" {
		t.Fatal("released before reap", code)
	}
	select {
	case event := <-b.results:
		b.finishProviderWork(event)
		if b.providerWork.Status != "cancelled" || b.providerWork.ID != q.Serial {
			t.Fatal(b.providerWork)
		}
	case <-time.After(5 * time.Second):
		t.Fatal("cancel did not reap")
	}
	if code := b.cancelProviderWork(q.Serial); code != "not_cancellable" {
		t.Fatal(code)
	}
}

func TestProviderWorkOutcomesAndSupersession(t *testing.T) {
	b, _ := providerFixture(t)
	for _, status := range []string{"loaded", "started", "failed", "cancelled"} {
		b.serial = 9
		b.providerWork = &providerWork{ID: "0000000000000001", Operation: "open", Status: "accepted", serial: 9}
		result := workResult{Status: status}
		if status == "loaded" {
			result.Snapshot = b.snapshot
		}
		b.finishProviderWork(completedWork{work: pendingWork{serial: 9}, result: result})
		if b.providerWork.Status != status {
			t.Fatal(status, b.providerWork)
		}
		data, _ := json.Marshal(b.providerWork)
		if bytes.Contains(data, []byte("serial")) {
			t.Fatal("private worker serial exposed")
		}
	}
	b.serial = 10
	b.providerWork = &providerWork{ID: "0000000000000001", Operation: "refresh", Status: "superseding", serial: 9}
	b.finishProviderWork(completedWork{work: pendingWork{serial: 8}})
	if b.providerWork.Status != "superseding" {
		t.Fatal("unrelated work replaced outcome")
	}
	b.finishProviderWork(completedWork{work: pendingWork{serial: 9}, result: workResult{Status: "loaded", Snapshot: b.snapshot}})
	if b.providerWork.Status != "superseded" {
		t.Fatal(b.providerWork)
	}
}

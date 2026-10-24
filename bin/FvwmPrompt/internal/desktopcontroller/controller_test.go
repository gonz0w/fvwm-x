// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopworker"
	"golang.org/x/sys/unix"
)

func fixture(t *testing.T) (*Controller, *[]string) {
	t.Helper()
	root, err := filepath.Abs("../../../../default-config")
	if err != nil {
		t.Fatal(err)
	}
	commands := []string{}
	d, err := New(root, filepath.Join(t.TempDir(), "desktop-state.json"), "Linux", "no-worker-needed", "no-capture", true, func(s string) error { commands = append(commands, s); return nil })
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(d.Close)
	if err := d.finishConfiguration(); err != nil {
		t.Fatal(err)
	}
	return d, &commands
}
func code(response values) string {
	v, _ := response["error"].(map[string]any)
	return stringAt(v, "code")
}
func TestSettingsCommitOverridesAndRefusals(t *testing.T) {
	d, commands := fixture(t)
	if err := d.configure("palette", "grove"); err != nil {
		t.Fatal(err)
	}
	if err := d.finishConfiguration(); err != nil {
		t.Fatal(err)
	}
	generation := d.generation
	if err := d.changeSettings(values{"palette": "ink", "motion": false}); err != nil {
		t.Fatal(err)
	}
	if generation == d.generation || d.saved["palette"] != "ink" || d.effective["palette"] != "grove" {
		t.Fatal("snapshot authority lost")
	}
	before, _ := os.ReadFile(d.statePath)
	count := len(*commands)
	generation = d.generation
	for _, changes := range []values{{"motion": 3}, {"day": "19:00", "night": "19:00"}, {"unknown": "ink"}} {
		if err := d.changeSettings(changes); err == nil {
			t.Fatal("invalid changes committed")
		}
	}
	after, _ := os.ReadFile(d.statePath)
	if string(after) != string(before) || d.generation != generation || len(*commands) != count {
		t.Fatal("refusal mutated state")
	}
	if err := d.resetSettings("palette"); err != nil {
		t.Fatal(err)
	}
	if d.saved["palette"] != "slate" || d.effective["palette"] != "grove" {
		t.Fatal("reset erased configuration override")
	}
}
func TestEditorAtomicityExpiryAndExternalConflict(t *testing.T) {
	d, commands := fixture(t)
	if err := d.editSettings("group:effects", false, nil); err != nil {
		t.Fatal(err)
	}
	e := d.editor
	count := len(*commands)
	if err := d.editSettings("unknown", false, nil); err == nil || d.editor != e || len(*commands) != count {
		t.Fatal("failed plan retired draft")
	}
	arguments := e.token
	changes := map[string]string{"motion": "off", "transparency": "on", "launcher_opacity": "63"}
	for _, key := range e.keys {
		arguments += " " + changes[key]
	}
	if err := d.handle("apply-settings", arguments); err != nil {
		t.Fatal(err)
	}
	if d.editor != nil || d.saved["motion"] != false || d.saved["launcher_opacity"] != 63 {
		t.Fatal("group not applied atomically")
	}
	if _, err := d.currentEditor(e.token); err == nil {
		t.Fatal("retired callback admitted")
	}
	if err := d.editSettings("palette", false, nil); err != nil {
		t.Fatal(err)
	}
	d.editor.deadline = time.Now().Add(-time.Second)
	if _, err := d.currentEditor(d.editor.token); err == nil || d.editor != nil {
		t.Fatal("expired editor active")
	}
	if err := d.editSettings("palette", false, nil); err != nil {
		t.Fatal(err)
	}
	e = d.editor
	if err := os.WriteFile(d.statePath, []byte("{}"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := d.handle("apply-settings", e.token+" ink"); err == nil || d.editor != e {
		t.Fatal("external conflict erased draft")
	}
	if err := d.reloadSettings(); err != nil || d.editor != nil {
		t.Fatal("reload did not retire editor", err)
	}
}
func TestSettingsIPCReadinessValidationAndTargeting(t *testing.T) {
	d, commands := fixture(t)
	d.ready = false
	if code(d.settingsRequest(values{"operation": "query", "scope": "all"})) != "provider_not_ready" {
		t.Fatal("provisional settings exposed")
	}
	if d.settingsRequest(values{"operation": "provider", "provider": strings.Repeat("f", 32)}) != nil {
		t.Fatal("foreign owner replied")
	}
	d.ready = true
	for _, g := range []any{nil, true, "bad"} {
		if code(d.settingsRequest(values{"operation": "query", "scope": "all", "provider": d.instance, "generation": g})) != "invalid_request" {
			t.Fatalf("accepted generation %v", g)
		}
	}
	response := d.settingsRequest(values{"operation": "query", "scope": "palette", "provider": d.instance, "generation": d.generation})
	if response["result"].(map[string]any)["state"] != "unchanged" {
		t.Fatal("conditional snapshot")
	}
	count := len(*commands)
	generation := d.generation
	response = d.settingsRequest(values{"operation": "validate", "provider": d.instance, "generation": generation, "values": values{"palette": "ink", "motion": false}})
	if code(response) != "" || d.saved["palette"] == "ink" || len(*commands) != count || d.generation != generation {
		t.Fatalf("validation mutated state: %v", response)
	}
	if _, err := os.Stat(d.statePath); !os.IsNotExist(err) {
		t.Fatal("validation persisted")
	}
	response = d.settingsRequest(values{"operation": "set", "provider": d.instance, "generation": strings.Repeat("f", 32), "values": values{"palette": "ink"}})
	if code(response) != "stale_generation" {
		t.Fatal(response)
	}
}
func TestCommitRemainsObservableAfterPublicationFailure(t *testing.T) {
	d, _ := fixture(t)
	before := d.generation
	d.send = func(string) error { return errors.New("closed publication pipe") }
	response := d.settingsRequest(values{"operation": "set", "provider": d.instance, "generation": before, "values": values{"palette": "ink"}})
	result, ok := response["result"].(map[string]any)
	if !ok || result["state"] != "committed" || result["application"] != "submission-failed" || d.generation == before {
		t.Fatalf("false refusal: %v", response)
	}
	stored, _, err := loadSettings(d.statePath)
	if err != nil || stored["palette"] != "ink" {
		t.Fatal("commit lost", err)
	}
}
func replyTo(p *preview, result values) {
	data, _ := json.Marshal(result)
	serial := p.pending.serial
	prefix := "MFL_CORE_1 " + p.desktop.instance + " " + serial + " "
	p.reply(prefix + "part 0 " + hex.EncodeToString(data))
	p.reply(prefix + "end 1")
}
func activePreview(t *testing.T, d *Controller, saved bool) {
	t.Helper()
	if err := d.preview.start(63, saved, nil); err != nil {
		t.Fatal(err)
	}
	replyTo(&d.preview, success(values{"generation": strings.Repeat("a", 48)}))
	replyTo(&d.preview, success(values{"scope": "menu-opacity", "state": "active", "generation": strings.Repeat("b", 48), "lease": strings.Repeat("c", 48)}))
	if d.preview.control == nil || d.preview.control.lease == "" {
		t.Fatal("preview not active")
	}
}
func TestPreviewKeepSaveAndDraftOwnership(t *testing.T) {
	d, commands := fixture(t)
	activePreview(t, d, true)
	control := d.preview.control
	before := d.generation
	if err := d.preview.action(control.view, "save"); err != nil {
		t.Fatal(err)
	}
	*commands = nil
	replyTo(&d.preview, success(values{"scope": "menu-opacity", "state": "confirmed"}))
	if d.saved["launcher_opacity"] != 63 || d.generation == before || d.preview.control != nil {
		t.Fatal("saved preview commit")
	}
	for _, command := range *commands {
		if strings.HasPrefix(command, "Colorset ") || strings.HasPrefix(command, "CopyMenuStyle ") {
			t.Fatal("full theme replayed after core confirmation")
		}
	}
	if err := d.editSettings("group:effects", false, nil); err != nil {
		t.Fatal(err)
	}
	e := d.editor
	argument := e.token
	for _, key := range e.keys {
		value := map[string]string{"motion": "on", "transparency": "on", "launcher_opacity": "61"}[key]
		argument += " " + value
	}
	if err := d.handle("preview-editor", argument); err != nil {
		t.Fatal(err)
	}
	if d.editor != nil || d.preview.control.editor == nil {
		t.Fatal("draft was not transferred")
	}
	d.preview.finish("Reverted", false, false)
	if d.editor == nil || d.saved["motion"] != false {
		t.Fatal("draft not resumed without saving other fields")
	}
}
func TestPreviewRefusalQueueTimeoutAndStaleness(t *testing.T) {
	d, commands := fixture(t)
	activePreview(t, d, true)
	control := d.preview.control
	if err := d.preview.request("query"); err != nil {
		t.Fatal(err)
	}
	if err := d.preview.action(control.view, "save"); err != nil {
		t.Fatal(err)
	}
	if err := d.preview.action(control.view, "revert"); err != nil {
		t.Fatal(err)
	}
	if err := d.preview.action(control.view, "confirm"); err != nil || d.preview.queued != "revert" {
		t.Fatal("Revert lost precedence")
	}
	d.preview.pending.deadline = time.Now().Add(-time.Second)
	d.preview.tick()
	if d.preview.control != nil || !strings.Contains(strings.Join(*commands, "\n"), " abandon ") {
		t.Fatal("deadline did not abandon")
	}
	activePreview(t, d, true)
	control = d.preview.control
	if err := os.WriteFile(d.statePath, []byte("{}"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := d.preview.action(control.view, "save"); err == nil {
		t.Fatal("external revision accepted")
	}
	d.preview.finish("refused", true, false)
	activePreview(t, d, false)
	old := d.preview.pending
	d.preview.reply("MFL_CORE_1 other invalid end 1")
	if d.preview.pending != old {
		t.Fatal("foreign reply changed owner")
	}
}
func TestOperationAdmissionCancelAndRetainedCompletion(t *testing.T) {
	d, _ := fixture(t)
	request := values{"resource": "operations", "operation": "start", "provider": d.instance, "generation": d.generation, "target": "applications"}
	response := d.operationRequest(request)
	if code(response) != "" {
		t.Fatal(response)
	}
	record := d.operations[0]
	response = d.operationRequest(values{"resource": "operations", "operation": "cancel", "provider": d.instance, "target": record.id, "generation": record.generation})
	if code(response) != "" || record.state != "cancelled" || len(d.jobs) != 0 || len(d.queue) != 0 {
		t.Fatal("queued cancel failed", response)
	}
	response = d.operationRequest(request)
	if code(response) != "" {
		t.Fatal(response)
	}
	record = d.operations[1]
	d.jobs["apps"].running = true
	d.queue = nil
	d.active = 1
	d.operationRequest(values{"resource": "operations", "operation": "cancel", "provider": d.instance, "target": record.id, "generation": record.generation})
	d.completed <- job{key: "apps", result: []any{values{"name": "discarded", "argv": []any{"/bin/true"}, "cwd": nil, "terminal": false}}}
	d.pollJobs()
	if record.state != "cancelled" || len(d.apps) != 0 || d.active != 0 {
		t.Fatal("late worker result published")
	}
	record.finished = time.Now().Add(-301 * time.Second)
	d.expireOperations()
	if len(d.operations) != 1 {
		t.Fatal("terminal retention unbounded")
	}
}
func TestOperationFailureHidesWorkerDetails(t *testing.T) {
	d, _ := fixture(t)
	d.operationRequest(values{"resource": "operations", "operation": "start", "provider": d.instance, "generation": d.generation, "target": "status"})
	record := d.operations[0]
	d.queue = nil
	d.active = 1
	d.completed <- job{key: "status", err: errors.New("secret worker content")}
	d.pollJobs()
	if record.state != "failed" || record.err != "worker_failed" || record.application != "not-submitted" {
		t.Fatal("worker failure exposed")
	}
}
func TestNativeFramingAndQueueBounds(t *testing.T) {
	packet := []byte{}
	for _, v := range []uint64{0xffffffff, stringMessage, 8, 0, 0, 0, 0, 0} {
		packet = binary.NativeEndian.AppendUint64(packet, v)
	}
	for i := 0; i < len(packet); i++ {
		_, n, err := nextFrame(packet[:i])
		if err != nil || n != 0 {
			t.Fatal("partial frame lost")
		}
	}
	f, n, err := nextFrame(packet)
	if err != nil || n != 64 || f.kind != stringMessage {
		t.Fatal("complete frame")
	}
	invalid := append([]byte{}, packet...)
	binary.NativeEndian.PutUint64(invalid[16:], 4097)
	if _, _, err := nextFrame(invalid); err == nil {
		t.Fatal("oversize frame admitted")
	}
	queue, err := appendCommand(nil, "Nop")
	if err != nil || binary.NativeEndian.Uint64(queue[8:]) != 3 {
		t.Fatal("command framing")
	}
	for _, text := range []string{"a\nb", "bad\x00", strings.Repeat("x", 4097)} {
		if _, err := appendCommand(nil, text); err == nil {
			t.Fatal("bad command admitted")
		}
	}
	if _, err := appendCommand(make([]byte, queueLimit-1), "Nop"); err == nil {
		t.Fatal("queue unbounded")
	}
}
func TestInvalidReportsAndApplicationsCannotPublish(t *testing.T) {
	d, commands := fixture(t)
	count := len(*commands)
	for _, raw := range []any{nil, []any{values{"name": "bad", "argv": []any{}, "cwd": nil, "terminal": false}}, []any{values{"name": "bad", "argv": []any{"/bin/true"}, "cwd": nil, "terminal": nil}}} {
		if err := d.publishJob(job{key: "apps", result: raw}); err == nil {
			t.Fatal("invalid apps admitted")
		}
	}
	if _, err := checkedReport(values{"schema_version": 1}); err == nil {
		t.Fatal("incomplete report admitted")
	}
	if len(*commands) != count {
		t.Fatal("partial publication")
	}
}
func TestShellWordsAndLabelsRemainLiteral(t *testing.T) {
	words, err := shellWords(`xterm -e 'a b' "\$HOME"`)
	if err != nil || !reflect.DeepEqual(words, []string{"xterm", "-e", "a b", `\$HOME`}) {
		t.Fatalf("%q %v", words, err)
	}
	if quote(`a"$%&\b`) != "\"ab\"" {
		t.Fatal("label can expand")
	}
	for _, text := range []string{`xterm "bad`, `xterm \`} {
		if _, err := shellWords(text); err == nil {
			t.Fatal("incomplete quoting accepted")
		}
	}
}
func TestBusyFileCommitPreservesEditor(t *testing.T) {
	d, _ := fixture(t)
	if err := d.changeSettings(values{"palette": "ink"}); err != nil {
		t.Fatal(err)
	}
	if err := d.editSettings("palette", false, nil); err != nil {
		t.Fatal(err)
	}
	e := d.editor
	lock, err := os.OpenFile(filepath.Join(filepath.Dir(d.statePath), ".desktop-state.lock"), os.O_RDWR, 0600)
	if err != nil {
		t.Fatal(err)
	}
	defer lock.Close()
	if err := unix.Flock(int(lock.Fd()), unix.LOCK_EX|unix.LOCK_NB); err != nil {
		t.Fatal(err)
	}
	if err := d.handle("apply-settings", e.token+" dune"); err == nil || d.editor != e || d.saved["palette"] != "ink" {
		t.Fatal("failed commit lost draft")
	}
}

func TestUnicodeSearchRetainsFullCasefold(t *testing.T) {
	d, commands := fixture(t)
	d.apps = []desktopworker.Application{{Name: "Straße", Argv: []string{"/bin/true"}}}
	*commands = nil
	query := "STRASSE"
	d.applicationMenu(&query)
	if !strings.Contains(strings.Join(*commands, "\n"), `"Straße" SendToModule FvwmDesktop launch 0`) {
		t.Fatal("full Unicode caseless search lost the existing match")
	}
}

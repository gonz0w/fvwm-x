// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/viewbinding"
)

func appResult(name string) []any {
	return []any{values{"name": name, "argv": []any{"/bin/true"}, "cwd": nil, "terminal": false}}
}
func statusResult(network string) values {
	return values{"cpu": nil, "memory": nil, "network": network, "battery": nil}
}

// The job owner normally dispatches this completion from pollJobs. Controlling
// the worker boundary here makes slow/burst/loss ordering deterministic without
// sleeps, test-only hooks, actual application launches or a second scheduler.
func finishMenuJob(t *testing.T, d *Controller, key string, data any, failure error) error {
	t.Helper()
	j := d.jobs[key]
	if j == nil {
		t.Fatal("missing job", key)
	}
	for i, queued := range d.queue {
		if queued == j {
			d.queue = append(d.queue[:i], d.queue[i+1:]...)
			break
		}
	}
	delete(d.jobs, key)
	result := job{key: key, origin: j, binding: j.binding, ticket: j.ticket, result: data, err: failure}
	err := d.completeMenuJob(result)
	if err == nil {
		err = d.publishJob(result)
	}
	return err
}

func collectMenu(t *testing.T, d *Controller, key string, data any) {
	t.Helper()
	if !d.requestMenuJob(key) {
		t.Fatal("request refused", key)
	}
	if err := finishMenuJob(t, d, key, data, nil); err != nil {
		t.Fatal(err)
	}
}

func TestMenuBindingsBurstRetainsOneLatestIntent(t *testing.T) {
	d, commands := fixture(t)
	if !d.requestMenuJob("apps") {
		t.Fatal("initial request refused")
	}
	first := d.jobs["apps"]
	for i := 0; i < 10000; i++ {
		if !d.requestMenuJob("apps") {
			t.Fatal("pending intent refused")
		}
	}
	if d.jobs["apps"] != first || len(d.jobs) != 1 || len(d.queue) != 1 || !d.appBinding.policy.Pending() {
		t.Fatal("refresh burst expanded worker ownership")
	}
	before := len(*commands)
	if err := finishMenuJob(t, d, "apps", appResult("obsolete"), nil); !errors.Is(err, errMenuSuperseded) {
		t.Fatal("obsolete completion accepted", err)
	}
	if len(*commands) != before || len(d.apps) != 0 {
		t.Fatal("obsolete result published")
	}
	if !d.startMenuJob("apps") || len(d.jobs) != 1 || len(d.queue) != 1 {
		t.Fatal("one pending refresh did not start")
	}
	if err := finishMenuJob(t, d, "apps", appResult("latest"), nil); err != nil {
		t.Fatal(err)
	}
	if len(d.apps) != 1 || d.apps[0].Name != "latest" || d.appBinding.policy.Pending() || d.startMenuJob("apps") {
		t.Fatal("latest commit did not quiesce")
	}
}

func TestMenuBindingsDeltaAndCommittedSettings(t *testing.T) {
	d, commands := fixture(t)
	if err := d.settingsMenu("all"); err != nil {
		t.Fatal(err)
	}
	d.changed(newID(16))
	before := len(*commands)
	if err := d.settingsMenu("all"); err != nil || len(*commands) != before {
		t.Fatal("equal settings projection republished", err)
	}
	if err := d.settingsMenu("palette"); err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(strings.Join(*commands, "\n"), "edit-settings palette") {
		t.Fatal("requested settings scope lost")
	}
	collectMenu(t, d, "apps", appResult("Equal"))
	collectMenu(t, d, "status", statusResult("same"))
	before = len(*commands)
	collectMenu(t, d, "apps", appResult("Equal"))
	collectMenu(t, d, "status", statusResult("same"))
	if len(*commands) != before {
		t.Fatal("identical native projection emitted commands", (*commands)[before:])
	}
}

func TestMenuBindingsLossRetainsDataAndFreshCommitRestores(t *testing.T) {
	d, commands := fixture(t)
	collectMenu(t, d, "apps", appResult("Retained"))
	query := "Retained"
	d.applicationMenu(&query)
	collectMenu(t, d, "status", statusResult("old-link"))
	before := len(*commands)
	for _, key := range []string{"apps", "status"} {
		d.requestMenuJob(key)
		if err := finishMenuJob(t, d, key, nil, errors.New("worker lost")); err == nil {
			t.Fatal("failed producer succeeded")
		}
		b := d.menuBinding(key)
		if !b.lost || b.policy.State() != viewbinding.Unavailable || d.startMenuJob(key) {
			t.Fatal("lost producer retried or stayed available")
		}
	}
	text := strings.Join((*commands)[before:], "\n")
	for _, expected := range []string{`SlateLauncherMenu Greyed "Retained"`, `SlateSearchResults Greyed "Retained"`, `SlateSession Greyed "Power off..."`, "old-link", "Status unavailable"} {
		if !strings.Contains(text, expected) {
			t.Fatal("missing unavailable projection", expected, text)
		}
	}
	if strings.Contains(text, "Menu SlateSearchResults Monitor") || len(d.apps) != 1 || d.status.Network != "old-link" || d.requireApplications() == nil {
		t.Fatal("loss reopened search, lost data or left launch enabled")
	}
	if err := d.handle("volume", "up"); err == nil || d.jobs["volume"] != nil {
		t.Fatal("unavailable sound action admitted")
	}
	d.power("reboot")
	if d.jobs["power"] != nil {
		t.Fatal("unavailable power action admitted")
	}
	before = len(*commands)
	collectMenu(t, d, "apps", appResult("Retained"))
	collectMenu(t, d, "status", statusResult("new-link"))
	text = strings.Join((*commands)[before:], "\n")
	if d.requireApplications() != nil || !d.workerAvailable() || !strings.Contains(text, `"Retained" SendToModule FvwmDesktop launch-bound `+d.instance+" "+d.appGeneration+" 0") || !strings.Contains(text, "confirm poweroff") {
		t.Fatal("fresh commit did not restore relevant actions", text)
	}
}

func TestMenuBindingsMalformedResultCannotReplaceSnapshot(t *testing.T) {
	d, _ := fixture(t)
	collectMenu(t, d, "apps", appResult("Good"))
	d.requestMenuJob("apps")
	if err := finishMenuJob(t, d, "apps", appResult("bad\x00name"), nil); err == nil {
		t.Fatal("malformed producer result accepted")
	}
	if d.apps[0].Name != "Good" || !d.appBinding.lost {
		t.Fatal("malformed snapshot replaced retained data")
	}
}

func TestMenuBindingsReloadAndCloseRejectOldLifetimes(t *testing.T) {
	d, commands := fixture(t)
	collectMenu(t, d, "apps", appResult("Good"))
	d.requestMenuJob("apps")
	old := d.jobs["apps"]
	if err := d.reloadSettings(); err != nil {
		t.Fatal(err)
	}
	if old.binding == d.appBinding || old.binding.policy.State() != viewbinding.Closed {
		t.Fatal("reload reused old binding owner")
	}
	before := len(*commands)
	if err := finishMenuJob(t, d, "apps", appResult("Old lifetime"), nil); !errors.Is(err, errMenuSuperseded) || len(*commands) != before {
		t.Fatal("reload accepted an old completion", err)
	}
	if !d.startMenuJob("apps") {
		t.Fatal("replacement did not retain its pending intent")
	}
	if err := finishMenuJob(t, d, "apps", appResult("New lifetime"), nil); err != nil {
		t.Fatal(err)
	}
	// No real jobs were started. Close must invalidate queued work as well.
	d.requestMenuJob("apps")
	d.Close()
	before = len(*commands)
	if err := finishMenuJob(t, d, "apps", appResult("Destroyed"), nil); !errors.Is(err, errMenuSuperseded) || len(*commands) != before || d.requestMenuJob("apps") {
		t.Fatal("destroyed view revived", err)
	}
}

func TestMenuBindingProjectionBoundAndLiteralData(t *testing.T) {
	d, commands := fixture(t)
	data := make([]any, 512)
	for i := range data {
		data[i] = appResult(fmt.Sprintf("literal \"$[x]%%&\\; Exec bad %d", i))[0]
	}
	collectMenu(t, d, "apps", data)
	for i := 0; i < 100; i++ {
		query := fmt.Sprint(i % 10)
		d.applicationMenu(&query)
	}
	if len(d.appBinding.menus) > 54 || d.appQuery == nil || len(*d.appQuery) > 320 {
		t.Fatal("unbounded native projection retention")
	}
	for _, command := range *commands {
		if strings.ContainsAny(command, "\x00\r\n") || strings.HasPrefix(command, "Exec ") || strings.Contains(command, "$[x]") {
			t.Fatal("incoming label became executable syntax", command)
		}
	}
	collectMenu(t, d, "apps", []any{})
	if len(d.appBinding.menus) != 2 || !strings.Contains(strings.Join(*commands, "\n"), "DestroyMenu SlateApps25") {
		t.Fatal("obsolete pages survived projection replacement")
	}
}

func TestMenuBindingPendingWaitsForSharedJobSlot(t *testing.T) {
	d, _ := fixture(t)
	for _, key := range []string{"a", "b", "c", "d"} {
		d.jobs[key] = &job{key: key}
	}
	if !d.requestMenuJob("apps") || len(d.jobs) != 4 || len(d.queue) != 0 || !d.appBinding.policy.Pending() {
		t.Fatal("full pool expanded or dropped pending intent")
	}
	delete(d.jobs, "a")
	if !d.startMenuJob("apps") || len(d.jobs) != 4 || len(d.queue) != 1 {
		t.Fatal("freed pool slot did not admit one refresh")
	}
	if err := finishMenuJob(t, d, "apps", appResult("Bounded"), nil); err != nil {
		t.Fatal(err)
	}
}

func TestMenuBindingBarRefreshResubmitsStatus(t *testing.T) {
	d, commands := fixture(t)
	collectMenu(t, d, "status", statusResult("same"))
	if err := d.handle("refresh", "1440"); err != nil {
		t.Fatal(err)
	}
	before := len(*commands)
	collectMenu(t, d, "status", statusResult("same"))
	if !strings.Contains(strings.Join((*commands)[before:], "\n"), "ChangeButton status Title") {
		t.Fatal("new bar did not receive equal current status")
	}
}

func TestMenuBindingDiagnosticIsBoundedAndReadOnly(t *testing.T) {
	d, commands := fixture(t)
	if err := d.settingsMenu("all"); err != nil {
		t.Fatal(err)
	}
	d.requestMenuJob("apps")
	before := len(*commands)
	for i := 0; i < 2; i++ {
		if err := d.handle("bindings", "snapshot-1"); err != nil {
			t.Fatal(err)
		}
	}
	if len(*commands) != before+6 {
		t.Fatal("diagnostic did not emit three domain records per snapshot")
	}
	report := map[string]bindingDomainReport{}
	for i, name := range [3]string{"settings", "apps", "system"} {
		command := (*commands)[before+i]
		if command != (*commands)[before+i+3] || len(command) > 1000 {
			t.Fatal("diagnostic changed source state or exceeded native command bound")
		}
		payload := strings.TrimPrefix(command, "Echo FVWM_BINDINGS_1 snapshot-1 "+name+" ")
		data, err := hex.DecodeString(payload)
		if err != nil {
			t.Fatal(err)
		}
		var domain bindingDomainReport
		if err := json.Unmarshal(data, &domain); err != nil {
			t.Fatal("invalid domain diagnostic projection", string(data), err)
		}
		report[name] = domain
	}
	if report["settings"].Menus == 0 || !report["settings"].Known || report["apps"].Known {
		t.Fatal("invalid reconstructed diagnostic projection", report)
	}
	before = len(*commands)
	for _, token := range []string{"", "bad token", "x; Exec bogus", strings.Repeat("x", 65)} {
		if err := d.handle("bindings", token); err == nil {
			t.Fatal("invalid diagnostic token admitted", token)
		}
	}
	if len(*commands) != before || len(d.jobs) != 1 || len(d.queue) != 1 {
		t.Fatal("diagnostic affected worker or publication ownership")
	}
}

func TestMenuBindingSupersededWorkerLossPreservesPendingRecovery(t *testing.T) {
	d, _ := fixture(t)
	collectMenu(t, d, "apps", appResult("Retained"))
	d.requestMenuJob("apps")
	d.requestMenuJob("apps")
	if err := finishMenuJob(t, d, "apps", nil, errors.New("worker lost")); !errors.Is(err, errMenuSuperseded) {
		t.Fatal("superseded request was not retired", err)
	}
	if !d.appBinding.lost || !d.appBinding.policy.Pending() || d.requireApplications() == nil || !d.startMenuJob("apps") {
		t.Fatal("worker loss hid unavailability or dropped pending recovery")
	}
	if err := finishMenuJob(t, d, "apps", appResult("Fresh"), nil); err != nil || d.requireApplications() != nil || d.apps[0].Name != "Fresh" {
		t.Fatal("pending recovery did not commit", err)
	}
}

func applicationCallback(t *testing.T, d *Controller, label string) string {
	t.Helper()
	for _, menu := range d.appBinding.menus {
		for _, row := range menu.Rows {
			if row[0] == label && strings.HasPrefix(row[1], "SendToModule FvwmDesktop launch-bound ") {
				return strings.TrimPrefix(row[1], "SendToModule FvwmDesktop launch-bound ")
			}
		}
	}
	t.Fatal("missing bound application row", label)
	return ""
}

func TestMenuBindingMappedOldRowCannotLaunchReorderedSnapshot(t *testing.T) {
	d, commands := fixture(t)
	first, second := appResult("First")[0], appResult("Second")[0]
	collectMenu(t, d, "apps", []any{first, second})
	old := applicationCallback(t, d, "First")
	generation := d.appGeneration
	query := "First"
	d.applicationMenu(&query)
	// A mapped native menu can retain this callback when DestroyMenu refuses
	// replacement. Preparing a refresh still leaves the working data usable.
	d.requestMenuJob("apps")
	if d.appGeneration != generation {
		t.Fatal("preparation advanced the published application generation")
	}
	if err := finishMenuJob(t, d, "apps", []any{second, first}, nil); err != nil {
		t.Fatal(err)
	}
	before := len(*commands)
	if err := d.handle("launch-bound", old); err == nil || len(*commands) != before || len(d.spawnSlots) != 0 {
		t.Fatal("old native row launched a newly indexed application", err)
	}
	if d.appGeneration == generation || !strings.HasSuffix(applicationCallback(t, d, "First"), " 1") {
		t.Fatal("accepted reorder retained an obsolete callback identity")
	}
	for _, menu := range d.appBinding.menus {
		for _, row := range menu.Rows {
			if strings.Contains(row[1], " launch-bound "+d.instance+" "+generation+" ") {
				t.Fatal("retained search or launcher kept an obsolete callback")
			}
		}
	}
	// Bound and public launch paths both reach the existing launch admission.
	// A full slot prevents a real process launch in this dispatcher test.
	d.spawnSlots = make(chan struct{}, 1)
	d.spawnSlots <- struct{}{}
	if err := d.handle("launch-bound", applicationCallback(t, d, "First")); err != nil {
		t.Fatal("current bound callback was refused", err)
	}
	if err := d.handle("launch", "0"); err != nil {
		t.Fatal("public launch index contract changed", err)
	}
}

func TestMenuBindingApplicationIdentityChangesOnlyWithCommittedData(t *testing.T) {
	d, commands := fixture(t)
	collectMenu(t, d, "apps", appResult("Same label"))
	callback := applicationCallback(t, d, "Same label")
	generation := d.appGeneration
	before := len(*commands)
	collectMenu(t, d, "apps", appResult("Same label"))
	if d.appGeneration != generation || len(*commands) != before {
		t.Fatal("identical application data unnecessarily changed callbacks")
	}
	changed := appResult("Same label")
	changed[0].(values)["argv"] = []any{"/bin/false"}
	collectMenu(t, d, "apps", changed)
	if d.appGeneration == generation || applicationCallback(t, d, "Same label") == callback || len(*commands) == before {
		t.Fatal("changed argv behind equal labels did not republish callbacks")
	}
	other, otherCommands := fixture(t)
	collectMenu(t, other, "apps", changed)
	before = len(*otherCommands)
	if err := other.handle("launch-bound", applicationCallback(t, d, "Same label")); err == nil || len(*otherCommands) != before || len(other.spawnSlots) != 0 {
		t.Fatal("producer restart accepted a previous controller's callback", err)
	}
}

func TestMenuBindingNarrowSettingsRetainsFullSubmenuInventory(t *testing.T) {
	d, commands := fixture(t)
	if err := d.theme(false); err != nil {
		t.Fatal(err)
	}
	inventory := map[string]bool{}
	for _, menu := range d.settingsBinding.menus {
		inventory[menu.Name] = true
	}
	for _, scope := range []string{"palette", "group:appearance", "group:effects", "motion"} {
		before := len(*commands)
		if err := d.settingsMenu(scope); err != nil {
			t.Fatal(err)
		}
		current := map[string]bool{}
		for _, menu := range d.settingsBinding.menus {
			current[menu.Name] = true
		}
		for name := range inventory {
			if !current[name] {
				t.Fatal("narrow scope destroyed a retained popup target", scope, name)
			}
		}
		for _, command := range (*commands)[before:] {
			if strings.HasPrefix(command, "DestroyMenu ") && !strings.HasPrefix(command, "DestroyMenu recreate ") {
				t.Fatal("scope removed a retained native submenu", command)
			}
		}
		if !strings.Contains(strings.Join((*commands)[before:], "\n"), "edit-settings "+scope) {
			t.Fatal("narrow scope lost its public settings root", scope)
		}
	}
}

func retainedMenu(t *testing.T, binding *menuBinding, name string) desktopsettings.Menu {
	t.Helper()
	for _, menu := range binding.menus {
		if menu.Name == name {
			return menu
		}
	}
	t.Fatal("missing desired menu", name)
	return desktopsettings.Menu{}
}

func popdownArgument(d *Controller, domain string, menu desktopsettings.Menu) string {
	return d.instance + " " + domain + " " + menu.Name + " " + menuRevision(menu)
}

func TestMenuBindingPopdownRepairsRefusedMappedProjection(t *testing.T) {
	d, commands := fixture(t)
	collectMenu(t, d, "apps", appResult("Original"))
	mapped := retainedMenu(t, d.appBinding, "SlateLauncherMenu")
	initialHook := "AddToMenu SlateLauncherMenu DynamicPopDownAction SendToModule FvwmDesktop binding-menu-popdown " + popdownArgument(d, "apps", mapped)
	if !strings.Contains(strings.Join(*commands, "\n"), initialHook) {
		t.Fatal("first publication did not install owned popdown hook")
	}
	// Model the native refusal to mutate an already mapped menu: the user
	// closes the old installed projection after the desired cache advances.
	collectMenu(t, d, "apps", appResult("Replacement"))
	current := retainedMenu(t, d.appBinding, "SlateLauncherMenu")
	before := len(*commands)
	if err := d.handle("binding-menu-popdown", popdownArgument(d, "apps", mapped)); err != nil {
		t.Fatal(err)
	}
	text := strings.Join((*commands)[before:], "\n")
	if !strings.Contains(text, "DestroyMenu recreate SlateLauncherMenu") || !strings.Contains(text, `"Replacement"`) || !strings.Contains(text, popdownArgument(d, "apps", current)) {
		t.Fatal("stale installed hash failed to replay desired projection and hook", text)
	}
	before = len(*commands)
	if err := d.handle("binding-menu-popdown", popdownArgument(d, "apps", current)); err != nil || len(*commands) != before {
		t.Fatal("unchanged installed projection unnecessarily republished", err)
	}
	// The same event repair must restore honest disabled state after loss.
	d.requestMenuJob("apps")
	if err := finishMenuJob(t, d, "apps", nil, errors.New("worker lost")); err == nil {
		t.Fatal("worker loss accepted")
	}
	before = len(*commands)
	if err := d.handle("binding-menu-popdown", popdownArgument(d, "apps", current)); err != nil || !strings.Contains(strings.Join((*commands)[before:], "\n"), `SlateLauncherMenu Greyed "Replacement"`) {
		t.Fatal("mapped action remained ready after loss", err)
	}
}

func TestMenuBindingPopdownRetiresOnlyBoundedEverOwnedNames(t *testing.T) {
	d, commands := fixture(t)
	data := make([]any, 512)
	for i := range data {
		data[i] = appResult(fmt.Sprint(i))[0]
	}
	collectMenu(t, d, "apps", data)
	obsolete := retainedMenu(t, d.appBinding, "SlateApps25")
	collectMenu(t, d, "apps", appResult("Small"))
	before := len(*commands)
	if err := d.handle("binding-menu-popdown", popdownArgument(d, "apps", obsolete)); err != nil || len(*commands) != before+1 || (*commands)[before] != "DestroyMenu SlateApps25" {
		t.Fatal("removed mapped page was not retired on popdown", err)
	}
	for _, name := range []string{"SlateApps26", "SlateApps025", "SlateResults25", "PersonalMenu", "SlateSession"} {
		before = len(*commands)
		argument := d.instance + " apps " + name + " " + menuRevision(obsolete)
		if err := d.handle("binding-menu-popdown", argument); err == nil || len(*commands) != before {
			t.Fatal("unowned native menu callback admitted", name, err)
		}
	}
	before = len(*commands)
	if err := d.handle("binding-menu-popdown", newID(16)+" apps SlateApps25 "+menuRevision(obsolete)); err != nil || len(*commands) != before {
		t.Fatal("previous controller lifetime callback was not ignored", err)
	}
	if len(d.appBinding.owned) > 54 {
		t.Fatal("ever-owned menu ledger exceeded fixed domain bound")
	}
	d.Close()
	if err := d.handle("binding-menu-popdown", popdownArgument(d, "apps", obsolete)); err != nil || len(*commands) != before {
		t.Fatal("closed owner callback emitted commands", err)
	}
}

func TestMenuBindingDiagnosticMaximumCountersFitNativeCommandLimit(t *testing.T) {
	maximum := ^uint64(0)
	domain := bindingDomainReport{Policy: viewbinding.Stats{
		State: viewbinding.Unavailable, Sources: maximum,
		IdentityBytes: maximum, Invalidations: maximum, Started: maximum,
		Published: maximum, Rejected: maximum,
	}, Menus: 54}
	report := [3]bindingDomainReport{domain, domain, domain}
	commands, err := bindingDiagnosticCommands(strings.Repeat("x", 64), report)
	if err != nil {
		t.Fatal("maximum diagnostic metadata exceeded native limit", err)
	}
	for _, command := range commands {
		if len(command) > 1000 || !strings.Contains(command, "3138343436373434303733373039353531363135") {
			t.Fatal("saturating counters were truncated or oversized", len(command))
		}
	}
	// Even a later oversized domain must return no earlier partial commands.
	report[2].Policy.State = viewbinding.State(strings.Repeat("x", 1000))
	if commands, err := bindingDiagnosticCommands("bounded", report); err == nil || commands != [3]string{} {
		t.Fatal("diagnostic validation returned a partial snapshot", err)
	}
}

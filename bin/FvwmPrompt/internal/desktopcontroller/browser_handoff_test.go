// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"errors"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
	"time"
)

func browserMenu(t *testing.T, d *Controller, commands *[]string, path string) string {
	t.Helper()
	d.pendingDirectory = path
	before := len(*commands)
	if err := d.publishJob(job{key: "files", result: []any{}}); err != nil {
		t.Fatal(err)
	}
	return filesBrowserCallback(t, (*commands)[before:])
}

// The fixed shell fixture records its original argv without evaluating it,
// then waits on a fixture-owned file. Production browser launches use no shell.
func browserChild(t *testing.T, d *Controller) (string, string, string) {
	t.Helper()
	root := t.TempDir()
	helper, args, gate := filepath.Join(root, "browser"), filepath.Join(root, "arguments"), filepath.Join(root, "finish")
	t.Setenv("FVWM_TEST_BROWSER_ARGS", args)
	t.Setenv("FVWM_TEST_BROWSER_GATE", gate)
	t.Setenv("FVWM_TEST_BROWSER_STARTS", filepath.Join(root, "starts"))
	t.Setenv("FVWM_TEST_BROWSER_EXIT", "0")
	script := "#!/bin/sh\nprintf 'x\\n' >> \"$FVWM_TEST_BROWSER_STARTS\"\nprintf '%s\\000' \"$@\" > \"$FVWM_TEST_BROWSER_ARGS\"\nwhile test ! -f \"$FVWM_TEST_BROWSER_GATE\"; do /bin/sleep .01; done\nexit \"$FVWM_TEST_BROWSER_EXIT\"\n"
	if err := os.WriteFile(helper, []byte(script), 0700); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		_ = os.WriteFile(gate, nil, 0600)
		deadline := time.Now().Add(3 * time.Second)
		for len(d.browser.pending) > 0 && time.Now().Before(deadline) {
			d.pollBrowserLaunches()
			time.Sleep(time.Millisecond)
		}
		if len(d.browser.pending) > 0 {
			for command := range d.browser.pending {
				if command.Process != nil {
					_ = command.Process.Kill()
				}
			}
			t.Error("browser test children did not finish after their gate opened")
		}
	})
	return helper, args, gate
}

func awaitBrowser(t *testing.T, label string, predicate func() bool) {
	t.Helper()
	deadline := time.Now().Add(3 * time.Second)
	for !predicate() {
		if !time.Now().Before(deadline) {
			t.Fatal("browser fixture deadline:", label)
		}
		time.Sleep(time.Millisecond)
	}
}

func finishBrowser(t *testing.T, d *Controller, gate string) {
	t.Helper()
	if err := os.WriteFile(gate, nil, 0600); err != nil {
		t.Fatal(err)
	}
	awaitBrowser(t, "all real child Wait results consumed", func() bool {
		d.pollBrowserLaunches()
		return len(d.browser.pending) == 0 && len(d.spawnSlots) == 0
	})
}

func TestFilesBrowserLiteralArgvAndCommittedDirectory(t *testing.T) {
	d, commands := fixture(t)
	helper, args, gate := browserChild(t, d)
	d.browser.helper = helper
	path := filepath.Join(t.TempDir(), strings.Repeat("界", 60)+"-literal-$[];\" spaced-\xff")
	token := browserMenu(t, d, commands, path)
	// An in-flight listing is not a new committed menu.
	d.pendingDirectory = filepath.Join(t.TempDir(), "not yet committed")
	if err := d.handle("files-browser", token); err != nil {
		t.Fatal(err)
	}
	awaitBrowser(t, "literal child arguments", func() bool {
		data, err := os.ReadFile(args)
		return err == nil && len(data) > 0 && data[len(data)-1] == 0
	})
	data, err := os.ReadFile(args)
	if err != nil {
		t.Fatal(err)
	}
	got := strings.Split(strings.TrimSuffix(string(data), "\x00"), "\x00")
	if !reflect.DeepEqual(got, []string{"--reuse", "--", path}) {
		t.Fatalf("literal path was reinterpreted: %q", got)
	}
	if len(d.browser.pending) != 1 || len(d.spawnSlots) != 1 {
		t.Fatal("Start was mistaken for completed browser admission")
	}
	before := len(*commands)
	finishBrowser(t, d, gate)
	if len(*commands) != before {
		t.Fatal("successful completion emitted a failure notice")
	}
}

func TestFilesBrowserRejectsStaleMalformedAndRetiredMenus(t *testing.T) {
	d, commands := fixture(t)
	helper, args, _ := browserChild(t, d)
	d.browser.helper = helper
	old := browserMenu(t, d, commands, "/tmp/old folder")
	current := browserMenu(t, d, commands, "/tmp/new folder")
	other, otherCommands := fixture(t)
	foreign := browserMenu(t, other, otherCommands, "/tmp/other controller")
	for _, token := range []string{old, foreign, "", current + " extra", "A" + current[1:], strings.Repeat("g", 32)} {
		if err := d.handle("files-browser", token); err == nil {
			t.Fatalf("invalid/stale browser action admitted: %q", token)
		}
	}
	if _, err := os.Stat(args); !os.IsNotExist(err) || len(d.browser.pending) != 0 || len(d.spawnSlots) != 0 {
		t.Fatal("refused callback launched a child", err)
	}
	d.Close()
	if err := d.handle("files-browser", current); err == nil || len(d.spawnSlots) != 0 {
		t.Fatal("retired controller admitted browser callback", err)
	}
}

func TestFilesBrowserMissingAndStartFailureAreVisible(t *testing.T) {
	for _, mode := range []string{"unset", "missing", "invalid interpreter", "global capacity"} {
		t.Run(mode, func(t *testing.T) {
			d, commands := fixture(t)
			token := browserMenu(t, d, commands, "/tmp/safe folder")
			want := "File browser is unavailable"
			switch mode {
			case "missing":
				d.browser.helper = filepath.Join(t.TempDir(), "missing")
			case "invalid interpreter":
				d.browser.helper = filepath.Join(t.TempDir(), "bad interpreter")
				if err := os.WriteFile(d.browser.helper, []byte("#!/no-such-browser-interpreter\n"), 0700); err != nil {
					t.Fatal(err)
				}
				want = "Could not start file browser"
			case "global capacity":
				helper, _, _ := browserChild(t, d)
				d.browser.helper = helper
				d.spawnSlots = make(chan struct{}, 1)
				d.spawnSlots <- struct{}{}
				want = "Could not start file browser"
			}
			before := len(*commands)
			d.Handle("files-browser " + token)
			if !strings.Contains(literalCommands(t, (*commands)[before:]), want) || len(d.browser.pending) != 0 {
				t.Fatal("missing/start failure was silent or retained admission")
			}
			if mode != "global capacity" && len(d.spawnSlots) != 0 {
				t.Fatal("failed start leaked global application slot")
			}
		})
	}
}

func TestFilesBrowserWaitFailureRequiresLoopAndNeverRetries(t *testing.T) {
	d, commands := fixture(t)
	helper, args, gate := browserChild(t, d)
	t.Setenv("FVWM_TEST_BROWSER_EXIT", "23")
	d.browser.helper = helper
	token := browserMenu(t, d, commands, "/tmp/request outcome may be unknown")
	before := len(*commands)
	if err := d.handle("files-browser", token); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(gate, nil, 0600); err != nil {
		t.Fatal(err)
	}
	awaitBrowser(t, "actual failed child completion queued", func() bool { return len(d.browser.completed) == 1 })
	if len(d.browser.pending) != 1 || len(d.spawnSlots) != 0 || len(*commands) != before {
		t.Fatal("waiter mutated resident admission or emitted commands")
	}
	d.pollBrowserLaunches()
	if len(d.browser.pending) != 0 || !strings.Contains(literalCommands(t, (*commands)[before:]), "unknown outcome") {
		t.Fatal("event loop lost asynchronous failure")
	}
	count := len(*commands)
	for i := 0; i < 16; i++ {
		d.pollBrowserLaunches()
	}
	if len(d.spawnSlots) != 0 || len(d.browser.pending) != 0 || len(*commands) != count {
		t.Fatal("failure retried or replayed its notice")
	}
	starts, err := os.ReadFile(filepath.Join(filepath.Dir(args), "starts"))
	if err != nil || string(starts) != "x\n" {
		t.Fatal("uncertain browser handoff launched more than once", string(starts), err)
	}
}

func TestFilesBrowserAdmissionIncludesUnconsumedCompletions(t *testing.T) {
	d, commands := fixture(t)
	helper, _, gate := browserChild(t, d)
	d.browser.helper = helper
	token := browserMenu(t, d, commands, "/tmp/bounded browser")
	for i := 0; i < maxBrowserLaunches; i++ {
		if err := d.handle("files-browser", token); err != nil {
			t.Fatal(err)
		}
	}
	if err := d.handle("files-browser", token); err == nil || len(d.browser.pending) != maxBrowserLaunches {
		t.Fatal("live browser admission exceeded capacity")
	}
	if err := os.WriteFile(gate, nil, 0600); err != nil {
		t.Fatal(err)
	}
	awaitBrowser(t, "all bounded completions buffered", func() bool { return len(d.browser.completed) == maxBrowserLaunches })
	if err := d.handle("files-browser", token); err == nil || len(d.spawnSlots) != 0 {
		t.Fatal("unconsumed completion released dedicated admission")
	}
	d.pollBrowserLaunches()
	if len(d.browser.pending) != 0 || len(d.browser.completed) != 0 {
		t.Fatal("loop did not release exact completed launches")
	}
	if err := d.handle("files-browser", token); err != nil {
		t.Fatal("consumed completion did not restore capacity", err)
	}
	finishBrowser(t, d, gate)
}

func TestFilesBrowserCloseLeavesIndependentChildAndBufferedWaitOwned(t *testing.T) {
	d, commands := fixture(t)
	helper, _, gate := browserChild(t, d)
	t.Setenv("FVWM_TEST_BROWSER_EXIT", "19")
	d.browser.helper = helper
	token := browserMenu(t, d, commands, "/tmp/independent browser")
	if err := d.handle("files-browser", token); err != nil {
		t.Fatal(err)
	}
	d.Close()
	before := len(*commands)
	if len(d.browser.pending) != 1 || len(d.spawnSlots) != 1 {
		t.Fatal("Close adopted or completed the independent browser")
	}
	if err := os.WriteFile(gate, nil, 0600); err != nil {
		t.Fatal(err)
	}
	awaitBrowser(t, "post-Close actual child Wait buffered", func() bool { return len(d.browser.completed) == 1 })
	d.pollBrowserLaunches()
	if len(d.browser.pending) != 0 || len(d.spawnSlots) != 0 || len(*commands) != before {
		t.Fatal("retired completion leaked admission or emitted a late notice")
	}
}

func TestFilesBrowserCompletionMustBelongToExactLaunch(t *testing.T) {
	d, commands := fixture(t)
	owned, foreign := &exec.Cmd{}, &exec.Cmd{}
	d.browser.pending[owned] = struct{}{}
	d.browser.completed <- browserCompletion{command: foreign, err: errors.New("foreign failure")}
	before := len(*commands)
	d.pollBrowserLaunches()
	if len(d.browser.pending) != 1 || len(*commands) != before {
		t.Fatal("foreign completion changed current launch ownership")
	}
	d.browser.completed <- browserCompletion{command: owned}
	d.pollBrowserLaunches()
	if len(d.browser.pending) != 0 || len(*commands) != before {
		t.Fatal("matching completion did not retire its launch")
	}
}

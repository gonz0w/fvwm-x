// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"bufio"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"os"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

func TestNativeWireStrictness(t *testing.T) {
	desktop, run := strings.Repeat("a", 32), strings.Repeat("b", 32)
	line, err := nativeApply(desktop, run, 1, nativeDesired{true, 128, "Noto Sans 12", "Adwaita", ""})
	if err != nil || line != "FNS1 APPLY "+desktop+" "+run+" 1 1 128 4e6f746f2053616e73203132 41647761697461 -\n" {
		t.Fatal(line, err)
	}
	valid := "FNS1 RESULT " + desktop + " " + run + " 1 ready inherited 4294967295 none\n"
	if r, err := nativeResultLine(valid); err != nil || r.seq != 1 || r.xserial != ^uint32(0) {
		t.Fatal(r, err)
	}
	for _, bad := range []string{strings.TrimSuffix(valid, "\n"), valid + "\n", strings.Replace(valid, " 1 ready", " 0 ready", 1), strings.Replace(valid, " 1 ready", " +1 ready", 1), strings.Replace(valid, "4294967295", "4294967296", 1), strings.Replace(valid, "none", "external-diagnostic", 1), strings.Replace(valid, " ready ", "\tready ", 1), strings.Replace(valid, "RESULT ", "RESULT  ", 1), strings.Replace(valid, run, strings.ToUpper(run), 1), valid[:len(valid)-1] + "\r\n", strings.Repeat("x", 2048) + "\n"} {
		if _, err := nativeResultLine(bad); err == nil {
			t.Fatal("accepted", bad)
		}
	}
	for _, bad := range []nativeDesired{{false, 129, "", "", ""}, {false, 0, "bad\nfont", "", ""}, {false, 0, "", "../icon", ""}} {
		if _, err := nativeApply(desktop, run, 1, bad); err == nil {
			t.Fatal(bad)
		}
	}
}

// The copied test executable is an actual child, never a shell stand-in. It
// exercises pipe fragments, held ACKs, loss, late retired events and actual Wait.
func TestMain(m *testing.M) {
	if os.Getenv("FVWM_NATIVE_TEST_EXEC") == os.Args[0] {
		nativeSettingsChild(os.Getenv("FVWM_NATIVE_TEST_CHILD"))
		os.Exit(0)
	}
	os.Exit(m.Run())
}

func nativeSettingsChild(mode string) {
	if marker := os.Getenv("FVWM_NATIVE_TEST_MARKER"); marker != "" {
		if err := os.WriteFile(marker, []byte("started"), 0600); err != nil {
			os.Exit(3)
		}
	}
	reader := bufio.NewReader(os.Stdin)
	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			os.Exit(0)
		}
		words := strings.Fields(line)
		if len(words) != 10 {
			os.Exit(2)
		}
		result := fmt.Sprintf("FNS1 RESULT %s %s %s ready inherited 1 none\n", words[2], words[3], words[4])
		switch mode {
		case "held":
			// No ACK: hold until parent EOF, so timeout must initiate cleanup.
			for {
				if _, err := reader.ReadByte(); err != nil {
					os.Exit(0)
				}
			}
		case "stubborn":
			signal.Ignore(syscall.SIGTERM)
			fmt.Print(result)
			for {
				time.Sleep(time.Second)
			}
		case "partial":
			fmt.Print(result[:17])
			time.Sleep(40 * time.Millisecond)
			fmt.Print(result[17:])
		case "incomplete":
			fmt.Print(strings.TrimSuffix(result, "\n"))
			os.Exit(0)
		case "lost":
			fmt.Print(result)
			fmt.Printf("FNS1 RESULT %s %s %s lost inherited 1 owner-conflict\n", words[2], words[3], words[4])
			os.Exit(0)
		default:
			fmt.Print(result)
		}
	}
}

func nativeTestHelper(t *testing.T, mode string) string {
	t.Helper()
	self, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(self)
	if err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(t.TempDir(), "native-helper.test")
	if err = os.WriteFile(path, data, 0700); err != nil {
		t.Fatal(err)
	}
	// The no-argument installed helper interface is preserved. TestMain runs
	// the child only when argv[0] matches the explicit private executable.
	t.Setenv("FVWM_NATIVE_TEST_CHILD", mode)
	t.Setenv("FVWM_NATIVE_TEST_EXEC", path)
	t.Setenv("FVWM_NATIVE_TEST_MARKER", path+".started")
	return path
}

func nativePump(t *testing.T, d *Controller, predicate func() bool) {
	t.Helper()
	deadline := time.Now().Add(4 * time.Second)
	for time.Now().Before(deadline) {
		d.nativeTick(time.Now())
		if predicate() {
			return
		}
		time.Sleep(10 * time.Millisecond)
	}
	t.Fatal("native condition timed out", d.nativeObservation())
}

func TestNativeControllerHeldPartialLostAndReap(t *testing.T) {
	for _, mode := range []string{"partial", "held", "lost", "incomplete", "stubborn"} {
		t.Run(mode, func(t *testing.T) {
			path := nativeTestHelper(t, mode)
			d, _ := fixture(t)
			d.native.helper = path
			if err := d.changeSettings(values{"native_settings_enabled": true}); err != nil {
				t.Fatal(err)
			}
			slot := d.native.slot
			if slot == nil || d.nativeObservation().Readiness != "starting" {
				t.Fatal("Start claimed ready")
			}
			if mode == "partial" {
				nativePump(t, d, func() bool { return d.nativeObservation().Readiness == "ready" })
				oldRun := slot.run
				if err := d.changeSettings(values{"application_font": "Sans 13"}); err != nil {
					t.Fatal(err)
				}
				if err := d.changeSettings(values{"application_font": "Sans 14"}); err != nil {
					t.Fatal(err)
				}
				if err := d.changeSettings(values{"application_font": "Sans 15"}); err != nil {
					t.Fatal(err)
				}
				nativePump(t, d, func() bool { return d.native.inflight == 0 && d.native.sent.font == "Sans 15" })
				if d.native.seq != 3 {
					t.Fatal("queued every intermediate preference", d.native.seq)
				}
				if err := d.retryNativeSettings(); err != nil {
					t.Fatal(err)
				}
				if d.native.slot != slot {
					t.Fatal("slot released before Wait")
				}
				nativePump(t, d, func() bool {
					return d.native.slot != nil && d.native.slot.run != oldRun && d.nativeObservation().Readiness == "ready"
				})
				before := d.native.observation
				d.nativeEvent(nativeEvent{run: oldRun, failure: "helper-io"})
				if before != d.native.observation {
					t.Fatal("retired run mutated current observation")
				}
			} else {
				if mode == "stubborn" {
					nativePump(t, d, func() bool { return d.nativeObservation().Readiness == "ready" })
					if err := d.changeSettings(values{"native_settings_enabled": false}); err != nil {
						t.Fatal(err)
					}
					if d.native.slot != slot {
						t.Fatal("unreaped stubborn child released")
					}
				}
				if mode == "held" {
					nativePump(t, d, func() bool { _, err := os.Stat(path + ".started"); return err == nil })
					d.nativeTick(time.Now().Add(4 * time.Second))
				}
				nativePump(t, d, func() bool { return d.native.slot == nil })
				if mode == "lost" && d.nativeObservation().Readiness != "lost" {
					t.Fatal(d.nativeObservation())
				}
				if d.native.retry {
					t.Fatal("automatic respawn")
				}
			}
			if err := d.changeSettings(values{"native_settings_enabled": false}); err != nil {
				t.Fatal(err)
			}
			nativePump(t, d, func() bool { return d.native.slot == nil })
			select {
			case <-slot.done:
			default:
				t.Fatal("slot released before supervisor retirement")
			}
		})
	}
}

func TestApplicationEditorDecodeAndInvalidSaveLeavePublication(t *testing.T) {
	d, _ := fixture(t)
	if err := d.editSettings("group:application", false, nil); err != nil {
		t.Fatal(err)
	}
	font := `Noto Sans "A" $x 13`
	args := d.editor.token + " off x" + base64.RawURLEncoding.EncodeToString([]byte(font)) + " x x 0"
	if err := d.handle("apply-settings", args); err != nil || d.saved["application_font"] != font {
		t.Fatal(err, d.saved)
	}
	before, _ := os.ReadFile(d.statePath)
	oldGeneration := d.generation
	oldNative := d.native
	if err := d.changeSettings(values{"application_font": "bad\nfont"}); err == nil {
		t.Fatal("bad save")
	}
	after, _ := os.ReadFile(d.statePath)
	if string(before) != string(after) || d.generation != oldGeneration || d.native != oldNative {
		t.Fatal("invalid save mutated publication")
	}
	var saved map[string]any
	if err := json.Unmarshal(after, &saved); err != nil || saved["schema_version"] != float64(desktopsettings.StateVersion) {
		t.Fatal(err, saved)
	}
}

func TestNativeStopUnknownRetainsSlotAndStaleResultsCannotApply(t *testing.T) {
	d, _ := fixture(t)
	d.effective["native_settings_enabled"] = true
	run := strings.Repeat("c", 32)
	slot := &nativeSupervisor{run: run, input: make(chan string, 1), events: make(chan nativeEvent, 16), fault: make(chan nativeEvent, 1), terminal: make(chan nativeTerminal, 1), stop: make(chan struct{}), done: make(chan struct{})}
	// This fake slot models a failed external reaper observation, not a process.
	close(slot.done)
	d.native = nativeControl{helper: "/no-helper", slot: slot, initialized: true, enabled: true, seq: 1, inflight: 1, desired: nativeTuple(d.effective), observation: desktopsettings.NativeObservation{Readiness: "starting", Run: run}}
	for _, r := range []nativeResult{{desktop: d.instance, run: strings.Repeat("d", 32), seq: 1, xstate: "ready"}, {desktop: d.instance, run: run, seq: 2, xstate: "ready"}} {
		d.nativeEvent(nativeEvent{run: run, result: &r})
		if d.native.observation.Readiness != "starting" {
			t.Fatal("stale/unsent result applied")
		}
	}
	slot.terminal <- nativeTerminal{run: run, reaped: false}
	d.nativeTick(time.Now())
	if d.native.observation.Readiness != "stop-unknown" || d.native.slot != slot {
		t.Fatal(d.native.observation)
	}
	if err := d.retryNativeSettings(); err != nil {
		t.Fatal(err)
	}
	if d.native.slot != slot {
		t.Fatal("new launch without confirmed Wait")
	}
}

func TestNativeObservationFreshnessPreservesOnlyCurrentDrafts(t *testing.T) {
	for _, state := range []string{"current", "stale", "expired"} {
		t.Run(state, func(t *testing.T) {
			d, commands := fixture(t)
			if err := d.editSettings("group:application", false, nil); err != nil {
				t.Fatal(err)
			}
			e := d.editor
			before := d.generation
			if state == "stale" {
				e.generation = strings.Repeat("e", 32)
			}
			if state == "expired" {
				e.deadline = time.Now().Add(-time.Second)
			}
			oldEditorGeneration := e.generation
			saved, _ := json.Marshal(d.saved)
			effective, _ := json.Marshal(d.effective)
			d.native.observation = desktopsettings.NativeObservation{Readiness: "ready", ResourceState: "inherited", Run: strings.Repeat("f", 32), Reason: "none", Sequence: 1, XSerial: 1}
			d.nativePublishObservation()
			if d.generation == before {
				t.Fatal("native observation retained conditional-query generation")
			}
			fresh := d.settingsRequest(values{"operation": "query", "scope": "group:application", "provider": d.instance, "generation": before})["result"].(map[string]any)
			if fresh["state"] != "observed" || fresh["settings"] == nil {
				t.Fatal("old generation hid current native state", fresh)
			}
			if state == "current" {
				if e.generation != d.generation {
					t.Fatal("current draft was invalidated")
				}
				_, changes, err := d.editorValues(e.token + " off xU2FucyAxMw x x 0")
				if err != nil || changes["application_font"] != "Sans 13" {
					t.Fatal("current draft no longer usable", changes, err)
				}
			} else if e.generation != oldEditorGeneration {
				t.Fatal("stale/expired editor became current", state)
			}
			if state == "stale" {
				if _, err := d.currentEditor(e.token); err == nil {
					t.Fatal("stale editor became usable")
				}
			}
			if state == "expired" {
				if _, err := d.currentEditor(e.token); err == nil {
					t.Fatal("expired editor became usable")
				}
			}
			afterSaved, _ := json.Marshal(d.saved)
			afterEffective, _ := json.Marshal(d.effective)
			if string(saved) != string(afterSaved) || string(effective) != string(afterEffective) {
				t.Fatal("observation changed preferences")
			}
			generation, count := d.generation, len(*commands)
			d.nativePublishObservation()
			if d.generation != generation || len(*commands) != count {
				t.Fatal("unchanged native observation churned generation/publication")
			}
			unchanged := d.settingsRequest(values{"operation": "query", "scope": "group:application", "provider": d.instance, "generation": generation})["result"].(map[string]any)
			if unchanged["state"] != "unchanged" {
				t.Fatal("current conditional query did not remain unchanged", unchanged)
			}
		})
	}
}

func TestNativeObservationPreservesOnlyValidPreviewBasis(t *testing.T) {
	for _, state := range []string{"current", "stale-generation", "stale-revision"} {
		t.Run(state, func(t *testing.T) {
			d, _ := fixture(t)
			before := d.generation
			other := strings.Repeat("d", 64)
			c := &previewControl{saved: &previewSnapshot{generation: before, revision: d.revision}, editor: &draft{generation: before, values: values{"launcher_opacity": 74}}}
			if state == "stale-generation" {
				c.saved.generation = strings.Repeat("e", 32)
			}
			if state == "stale-revision" {
				c.saved.revision = &other
			}
			oldSaved, oldDraft := c.saved.generation, c.editor.generation
			d.preview.control = c
			d.native.observation = desktopsettings.NativeObservation{Readiness: "conflict", ResourceState: "unavailable", Reason: "owner-conflict", Run: strings.Repeat("a", 32)}
			d.nativePublishObservation()
			if state == "current" {
				if c.saved.generation != d.generation || c.editor.generation != d.generation || c.editor.values["launcher_opacity"] != 74 {
					t.Fatal("native observation consumed preview basis/draft")
				}
			} else if c.saved.generation != oldSaved || c.editor.generation != oldDraft {
				t.Fatal("stale preview basis became current")
			}
		})
	}
}

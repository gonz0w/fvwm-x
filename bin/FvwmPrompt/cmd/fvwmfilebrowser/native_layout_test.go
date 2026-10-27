// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bufio"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"net"
	"reflect"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/internal/viewbinding"
	"github.com/fvwmorg/FvwmPrompt/transport"
)

func layoutBrowser(t *testing.T, mode string) (*browser, net.Conn) {
	t.Helper()
	left, right := net.Pipe()
	t.Cleanup(func() { left.Close(); right.Close() })
	b := &browser{session: transport.New(left), instance: "layout", alias: "same-form",
		form: 3, nativeGeneration: 7, native: make(chan string, 1), nativeWatchReplies: &nativeWatchReplyGate{}, view: mode,
		renderPath: "/fixture", renderView: mode, renderSerial: 12,
		snapshot: &filemodel.Snapshot{Schema: 1, Path: []byte("/fixture")}}
	if err := b.model.Replace([]collection.Entry{
		{ID: "a", Label: "shown-a"}, {ID: "b", Label: "shown-b"},
		{ID: "c", Label: "hidden-c"}, {ID: "d", Label: "hidden-d"},
	}); err != nil {
		t.Fatal(err)
	}
	if err := b.model.SelectIDs(b.model.Generation, []string{"a", "c", "d"}); err != nil {
		t.Fatal(err)
	}
	if err := b.model.SetFilter("shown"); err != nil {
		t.Fatal(err)
	}
	b.model.Focus = "b"
	b.viewEntries = b.model.VisibleEntries()
	work := boundWork(t, b)
	if !b.intentBinding.Complete(work.ticket, true) {
		t.Fatal("initial browser intent did not publish")
	}
	b.active, b.activeSerial, b.cancel = false, 0, nil
	return b, right
}

func layoutReply(command string, generation, pending uint32, packet []byte) string {
	data := make([]byte, 4, 4+len(packet))
	binary.BigEndian.PutUint32(data, generation)
	data = append(data, packet...)
	return fmt.Sprintf("FVWM_FORM_LIST %s %d %d 0 x%s", strings.Fields(command)[4],
		generation, pending, base64.RawURLEncoding.EncodeToString(data))
}

// Exercise the consumer's real ordered transport commands. Replies come only
// from observed ListStatus requests; an Echo alone cannot publish a layout.
func layoutScript(t *testing.T, b *browser, peer net.Conn, run func() error, reply func(string) string) ([]string, error) {
	t.Helper()
	commands := make(chan string)
	stop := make(chan struct{})
	readDone := make(chan struct{})
	defer func() {
		close(stop)
		// A retry reuses this connection. Retire the prior reader before
		// starting the next script so it cannot consume that retry's commands.
		peer.SetReadDeadline(time.Now())
		<-readDone
		peer.SetReadDeadline(time.Time{})
	}()
	go func() {
		defer close(readDone)
		scanner := bufio.NewScanner(peer)
		for scanner.Scan() {
			select {
			case commands <- scanner.Text():
			case <-stop:
				return
			}
		}
	}()
	done := make(chan error, 1)
	go func() { done <- run() }()
	var observed []string
	deadline := time.NewTimer(5 * time.Second)
	defer deadline.Stop()
	for {
		select {
		case command := <-commands:
			observed = append(observed, command)
			if response := reply(command); response != "" {
				if err := b.routeNativeReply(response); err != nil {
					t.Fatal("production reply routing refused scripted response", err)
				}
			}
		case err := <-done:
			return observed, err
		case <-deadline.C:
			b.session.Close()
			t.Fatal("native script did not finish", observed)
		}
	}
}

func TestNativeLayoutSwitchWaitsForObservedGeneration(t *testing.T) {
	for _, mode := range []string{"list", "grid"} {
		t.Run(mode, func(t *testing.T) {
			b, peer := layoutBrowser(t, mode)
			oldControl := testControl(b, "view", "-", []byte{0, 1, 1})
			if _, err := b.action(oldControl); err != nil {
				t.Fatal(err)
			}
			nextMode, columns := "grid", 8
			if mode == "grid" {
				nextMode, columns = "list", 1
			}
			if b.viewing() != mode || b.requestedView() != nextMode {
				t.Fatal("requested mode advertised before native publication")
			}
			generation, providerGeneration := b.model.Generation, b.providerGeneration()
			selected, focus := b.model.SelectedIDs(), b.model.Focus
			// These bytes belong to the native Form. A consumer must never send
			// commands that replace them while changing only viewport geometry.
			drafts := map[string]string{"path": "../literal $[x] \" é", "filter": "unfinished filter"}
			originalDrafts := map[string]string{"path": drafts["path"], "filter": drafts["filter"]}
			phase := 0
			commands, err := layoutScript(t, b, peer, b.show, func(command string) string {
				fields := strings.Fields(command)
				if strings.Contains(command, " ListLayout ") {
					if phase != 1 || command != fmt.Sprintf("SendToModule same-form ListLayout picks 7 8 18 %d", columns) {
						t.Fatal("layout command/order", command, phase)
					}
					phase = 2
				}
				if strings.Contains(command, " ListStatus ") {
					switch {
					case strings.HasSuffix(fields[4], "_current"):
						if phase != 0 {
							t.Fatal("unexpected resnapshot", phase)
						}
						phase = 1
						return layoutReply(command, 7, 0, []byte{0, 1, 1})
					case strings.HasSuffix(fields[4], "_layout"):
						if phase != 2 || b.viewing() != mode || b.nativeGeneration != 7 || b.providerGeneration() != providerGeneration {
							t.Fatal("unobserved publication exposed", phase)
						}
						phase = 3
						return layoutReply(command, 8, 0, []byte{0, 1, 1})
					case strings.HasSuffix(fields[4], "_painted"):
						if phase != 3 || b.viewing() != nextMode || b.nativeGeneration != 8 {
							t.Fatal("acknowledged mode missing", phase)
						}
						phase = 4
						return layoutReply(command, 8, 0, []byte{0, 1, 1})
					default:
						t.Fatal("unexpected native request", command)
					}
				}
				if strings.Contains(command, " SetInput ") {
					drafts[fields[3]] = strings.Join(fields[4:], " ")
				}
				if !strings.HasPrefix(command, "SendToModule same-form ") && !strings.HasPrefix(command, "Echo FVWM_FILES_") ||
					strings.Contains(command, "ListFocus") || strings.Contains(command, "ListBegin") || strings.Contains(command, "ListEntry") || strings.Contains(command, "Input ") {
					t.Fatal("layout replaced native controls or revealed focus", command)
				}
				return ""
			})
			if err != nil || phase != 4 || b.alias != "same-form" || b.form != 3 || b.lastControl != 1 ||
				b.model.Generation != generation || b.model.Focus != focus || !reflect.DeepEqual(b.model.SelectedIDs(), selected) ||
				b.model.HiddenSelectedCount() != 2 || !reflect.DeepEqual(drafts, originalDrafts) || b.providerGeneration() == providerGeneration {
				t.Fatal("layout lost browser/native continuity", err, phase, commands)
			}
			if !strings.Contains(strings.Join(commands, "\n"), "SetLabel projection \"View: "+nextMode+";") {
				t.Fatal("mode label not updated", commands)
			}
			if _, err := b.action(oldControl); err == nil {
				t.Fatal("replayed old control admitted")
			}
			stale := strings.Fields(testControl(b, "clear-selection", "-", []byte{0, 1, 1}))
			stale[5] = strings.Fields(oldControl)[5]
			if _, err := b.action(strings.Join(stale, " ")); err == nil || !reflect.DeepEqual(b.model.SelectedIDs(), selected) {
				t.Fatal("old native projection acted on current Form")
			}
		})
	}
}

func TestNativeLayoutInvalidObservationRetiresBindings(t *testing.T) {
	for _, failure := range []string{"stale", "future", "pending", "refused", "packet", "unchanged-pending", "unchanged-refused", "unchanged-packet", "lost"} {
		t.Run(failure, func(t *testing.T) {
			b, peer := layoutBrowser(t, "list")
			b.view = "grid"
			before := b.providerGeneration()
			commands, err := layoutScript(t, b, peer, b.show, func(command string) string {
				if !strings.Contains(command, " ListStatus ") {
					return ""
				}
				if strings.HasSuffix(strings.Fields(command)[4], "_current") {
					return layoutReply(command, 7, 0, []byte{0, 1, 1})
				}
				if b.viewing() != "list" || b.nativeGeneration != 7 {
					t.Fatal("premature layout publication")
				}
				switch failure {
				case "stale":
					return layoutReply(command, 6, 0, []byte{0, 1, 1})
				case "future":
					return layoutReply(command, 9, 0, []byte{0, 1, 1})
				case "pending":
					return layoutReply(command, 8, 9, []byte{0, 1, 1})
				case "refused":
					return strings.Replace(layoutReply(command, 8, 0, []byte{0, 1, 1}), " 8 0 0 ", " 8 0 1 ", 1)
				case "packet":
					return layoutReply(command, 8, 0, []byte{0, 2, 1})
				case "unchanged-pending":
					return layoutReply(command, 7, 8, []byte{0, 1, 1})
				case "unchanged-refused":
					return strings.Replace(layoutReply(command, 7, 0, []byte{0, 1, 1}), " 7 0 0 ", " 7 0 1 ", 1)
				case "unchanged-packet":
					return layoutReply(command, 7, 0, []byte{0, 2, 1})
				default:
					b.session.Close()
					return ""
				}
			})
			if err == nil || b.viewing() != "list" || b.requestedView() != "list" || b.nativeGeneration != 7 ||
				b.projectionBinding.State() != viewbinding.Closed || b.intentBinding.State() != viewbinding.Closed || b.providerGeneration() == before {
				t.Fatal("failure advertised an applied projection or retained live bindings", failure, err)
			}
			for _, command := range commands {
				if strings.Contains(command, "SetLabel projection") || strings.HasPrefix(command, "Echo FVWM_FILES_RENDER") || strings.HasPrefix(command, "KillModule") {
					t.Fatal("failed layout painted/rebuilt/claimed success", command)
				}
			}
		})
	}
}

func TestNativeLayoutUnchangedObservationRetainsControlsAndAllowsRetry(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	control := testControl(b, "view", "-", []byte{0, 1, 1})
	if _, err := b.action(control); err != nil {
		t.Fatal(err)
	}
	before, selected := b.providerGeneration(), b.model.SelectedIDs()
	commands, err := layoutScript(t, b, peer, b.show, func(command string) string {
		if strings.Contains(command, " ListStatus ") {
			return layoutReply(command, 7, 0, []byte{0, 1, 1})
		}
		return ""
	})
	if err != nil || b.viewing() != "list" || b.requestedView() != "list" || b.nativeGeneration != 7 ||
		b.projectionBinding.State() != viewbinding.Current || b.intentBinding.State() != viewbinding.Current ||
		b.providerGeneration() != before || b.alias != "same-form" || b.form != 3 || b.lastControl != 1 ||
		!reflect.DeepEqual(b.model.SelectedIDs(), selected) || b.model.Focus != "b" || !strings.Contains(b.status, "enlarge") {
		t.Fatal("known unchanged native layout retired usable state or claimed a mode change", err, b.status)
	}
	for _, command := range commands {
		if strings.Contains(command, "ListFocus") || strings.Contains(command, "Input ") || strings.Contains(command, "KillModule") ||
			strings.Contains(command, "SetLabel projection \"View: grid") || strings.Contains(command, "ListBegin") {
			t.Fatal("normal layout refusal replaced controls/drafts or claimed the new mode", command)
		}
	}
	if _, err := b.action(control); err == nil {
		t.Fatal("normal refusal permitted a replayed invocation")
	}
	if _, err := b.action(testControl(b, "view", "-", []byte{0, 1, 1})); err != nil {
		t.Fatal("retained controls could not retry", err)
	}
	// Native resize admits the next request: the consumer still uses the exact
	// unchanged EXPECTED and retries NEXT only after a fresh user invocation.
	_, err = layoutScript(t, b, peer, b.show, func(command string) string {
		if strings.Contains(command, " ListLayout ") && !strings.HasSuffix(command, " 7 8 18 8") {
			t.Fatal("refusal consumed a native generation", command)
		}
		if strings.Contains(command, " ListStatus ") {
			generation := uint32(8)
			if strings.HasSuffix(strings.Fields(command)[4], "_current") {
				generation = 7
			}
			return layoutReply(command, generation, 0, []byte{0, 1, 1})
		}
		return ""
	})
	if err != nil || b.nativeGeneration != 8 || b.viewing() != "grid" || b.alias != "same-form" || b.form != 3 ||
		b.lastControl != 2 || !reflect.DeepEqual(b.model.SelectedIDs(), selected) || strings.Contains(b.status, "enlarge") {
		t.Fatal("refused layout could not be retried after resize", err)
	}
}

func TestProviderViewPublishesOnlyObservedMode(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	before := b.providerGeneration()
	query := actionQuery(b, "view", `{"mode":"grid"}`)
	action, err := decodeProviderAction(query)
	if err != nil {
		t.Fatal(err)
	}
	if code := b.applyProviderAction(query, action); code != "" || b.viewing() != "list" || b.providerGeneration() != before {
		t.Fatal("provider advertised a pending mode", code)
	}
	_, err = layoutScript(t, b, peer, b.show, func(command string) string {
		if !strings.Contains(command, " ListStatus ") {
			return ""
		}
		generation := uint32(8)
		if strings.HasSuffix(strings.Fields(command)[4], "_current") {
			generation = 7
		}
		return layoutReply(command, generation, 0, []byte{0, 1, 1})
	})
	if err != nil || b.viewing() != "grid" || b.providerGeneration() == before {
		t.Fatal("provider did not publish acknowledged mode", err)
	}
	if code := b.applyProviderAction(query, action); code != "stale_generation" {
		t.Fatal("old provider control admitted on new native layout", code)
	}
}

func TestProviderViewReplyReportsOnlyObservedApplication(t *testing.T) {
	for _, c := range []struct {
		name, mode string
		advance    bool
		applied    bool
		code       string
	}{
		{"refused", "grid", false, false, "operation_refused"},
		{"already-current", "list", false, true, ""},
		{"changed", "grid", true, true, ""},
	} {
		t.Run(c.name, func(t *testing.T) {
			b, peer := layoutBrowser(t, "list")
			query := actionQuery(b, "view", fmt.Sprintf(`{"mode":%q}`, c.mode))
			generation := uint32(7)
			layouts, parts := 0, 0
			var data []byte
			applied := false
			_, err := layoutScript(t, b, peer, func() error {
				var err error
				applied, err = b.answerProvider(query)
				return err
			}, func(command string) string {
				fields := strings.Fields(command)
				if strings.Contains(command, " ListLayout ") {
					layouts++
				}
				if strings.Contains(command, " ListStatus ") {
					if strings.HasSuffix(fields[4], "_layout") && c.advance {
						generation = 8
					}
					return layoutReply(command, generation, 0, []byte{0, 1, 1})
				}
				if strings.HasPrefix(command, "files_reply ") && fields[2] == "part" {
					if len(fields) != 5 || fields[1] != query.Serial || fields[3] != fmt.Sprint(parts) {
						t.Fatal("uncorrelated provider layout reply", command)
					}
					chunk, err := hex.DecodeString(fields[4])
					if err != nil {
						t.Fatal(err)
					}
					data = append(data, chunk...)
					parts++
				}
				return ""
			})
			var envelope struct {
				Error  struct{ Code string }
				Result struct {
					Applied bool
					View    string
				}
			}
			if err != nil || json.Unmarshal(data, &envelope) != nil || applied != c.applied || envelope.Error.Code != c.code ||
				envelope.Result.Applied != c.applied || b.nativeGeneration != generation || b.alias != "same-form" ||
				b.projectionBinding.State() != viewbinding.Current || b.intentBinding.State() != viewbinding.Current {
				t.Fatal("provider claimed unobserved layout or retired usable controls", err, string(data))
			}
			if c.applied && (b.viewing() != c.mode || envelope.Result.View != c.mode) ||
				!c.applied && (b.viewing() != "list" || b.requestedView() != "list" || !strings.Contains(b.status, "enlarge")) ||
				(c.mode == "list" && layouts != 0) || (c.mode == "grid" && layouts != 1) {
				t.Fatal("provider layout result did not match actual observation", layouts, string(data))
			}
		})
	}
}

func TestNativeRefreshCommitsBeforePendingLayout(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.view = "grid"
	// A completed same-folder refresh changes content before the pending layout
	// is serviced. Its transaction must retire before the layout can advance.
	b.snapshot.Items = []filemodel.Item{
		{ID: "a", Name: []byte("shown-a"), Kind: "file"},
		{ID: "b", Name: []byte("shown-b changed"), Kind: "file"},
		{ID: "c", Name: []byte("hidden-c"), Kind: "file"},
		{ID: "d", Name: []byte("hidden-d"), Kind: "file"},
	}
	event := completedWork{work: boundWork(t, b), result: workResult{Status: "loaded", Snapshot: b.snapshot}}
	phase := 0
	_, err := layoutScript(t, b, peer, func() error { return b.completed(event) }, func(command string) string {
		if strings.Contains(command, " ListBegin ") {
			if phase != 0 || !strings.HasSuffix(command, " 7 8") {
				t.Fatal("refresh began out of order", command)
			}
			phase = 1
		}
		if strings.Contains(command, " ListCommit ") {
			if phase != 1 || !strings.HasSuffix(command, " 8") {
				t.Fatal("refresh committed out of order", command)
			}
			phase = 2
		}
		if strings.Contains(command, " ListLayout ") {
			if phase != 3 || !strings.HasSuffix(command, " 8 9 18 8") {
				t.Fatal("layout overtook refresh", command, phase)
			}
			phase = 4
		}
		if strings.Contains(command, " ListStatus ") {
			token := strings.Fields(command)[4]
			switch {
			case strings.HasSuffix(token, "_u3"):
				return layoutReply(command, 7, 8, []byte{0, 1, 1})
			case strings.HasSuffix(token, "_committed"):
				if phase != 2 || b.nativeGeneration != 7 || b.viewing() != "list" {
					t.Fatal("unobserved content/layout generation published")
				}
				phase = 3
				return layoutReply(command, 8, 0, []byte{0, 1, 1})
			case strings.HasSuffix(token, "_layout"):
				if phase != 4 || b.nativeGeneration != 8 || b.viewing() != "list" {
					t.Fatal("layout claimed before its own ACK")
				}
				phase = 5
				return layoutReply(command, 9, 0, []byte{0, 1, 1})
			case strings.HasSuffix(token, "_painted"):
				return layoutReply(command, 9, 0, []byte{0, 1, 1})
			default:
				t.Fatal("unexpected native observation", command, phase)
			}
		}
		return ""
	})
	if err != nil || phase != 5 || b.nativeGeneration != 9 || b.viewing() != "grid" || b.alias != "same-form" ||
		b.model.Focus != "b" || b.model.HiddenSelectedCount() != 2 {
		t.Fatal("refresh/layout interleave lost continuity", err, phase)
	}
}

func TestNativeLayoutGenerationExhaustionUsesExplicitRebuild(t *testing.T) {
	for _, generation := range []uint32{2147483646, 2147483647} {
		t.Run(fmt.Sprint(generation), func(t *testing.T) {
			b, peer := layoutBrowser(t, "list")
			b.nativeGeneration, b.view = generation, "grid"
			// At max-1, a pending content edit plus layout needs two advances.
			b.replaceSelection = generation == 2147483646
			notify := ""
			staticButton := false
			commands, err := layoutScript(t, b, peer, b.show, func(command string) string {
				if strings.Contains(command, " ListLayout ") {
					t.Fatal("exhausted native generation advanced", command)
				}
				if strings.Contains(command, ": ListNotify ") {
					notify = strings.Fields(command)[2]
				}
				if strings.Contains(command, `: Button continue "View (F7)" F7`) {
					staticButton = true
				}
				if strings.HasPrefix(command, "Module FvwmForm ") {
					if notify == "" || b.nativeGeneration != generation || b.viewing() != "list" {
						t.Fatal("rebuilt view published before native observation")
					}
					return layoutReply("SendToModule unused ListStatus picks "+notify+" selection", 1, 0, []byte{0, 1, 1})
				}
				return ""
			})
			if err != nil || !staticButton || b.form != 4 || b.nativeGeneration != 1 || b.viewing() != "grid" || b.replaceSelection {
				t.Fatal("explicit exhaustion recovery failed", err, commands)
			}
		})
	}
}

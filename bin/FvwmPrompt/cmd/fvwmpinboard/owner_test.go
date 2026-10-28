package main

import (
	"bufio"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"reflect"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/pinboard"
	"github.com/fvwmorg/FvwmPrompt/internal/viewbinding"
	"github.com/fvwmorg/FvwmPrompt/transport"
)

func testBoard(t *testing.T) *board {
	t.Helper()
	binding, err := viewbinding.New("intent")
	if err != nil {
		t.Fatal(err)
	}
	return &board{protocol: pinboard.Protocol{Alias: "FvwmPinboard-" + strings.Repeat("a", 32), Token: strings.Repeat("a", 32)}, state: pinboard.Empty(), ready: true, view: 1, nextView: 1, epoch: "0000000000000001", binding: binding}
}

func TestNativeFontConfigurationPreservesSpacedFamilyAsOneLiteral(t *testing.T) {
	font := "xft:DejaVu Sans Mono:size=18:antialias=True"
	o, err := parseOptions([]string{"--font", font, "--colorset", "5"}, "/home/user", io.Discard)
	if err != nil {
		t.Fatal(err)
	}
	commands := initialConfig("FvwmPinboard-test", o)
	if len(commands) != 2 {
		t.Fatal(commands)
	}
	prefix := "*FvwmPinboard-test: Font "
	if !strings.HasPrefix(commands[0], prefix) {
		t.Fatal(commands[0])
	}
	// Validated font input excludes quotes/backslashes/expansions. A quoted
	// token must therefore decode to the complete original family and options.
	literal, err := strconv.Unquote(strings.TrimPrefix(commands[0], prefix))
	if err != nil || literal != font {
		t.Fatal("font is not one complete literal token", commands[0], literal, err)
	}
	if commands[1] != "*FvwmPinboard-test: Colorset 5" {
		t.Fatal(commands[1])
	}
	if got := initialConfig("FvwmPinboard-test", options{colorset: -1}); len(got) != 0 {
		t.Fatal("default overrides emitted", got)
	}
}

func fakeJob(t *testing.T, b *board, operation string) *job {
	t.Helper()
	if err := b.binding.Observe("intent", viewbinding.Version{Producer: b.protocol.Token, Revision: "1"}); err != nil {
		t.Fatal(err)
	}
	ticket, ok := b.binding.Begin()
	if !ok {
		t.Fatal("job did not begin")
	}
	j := &job{serial: 1, request: workRequest{Operation: operation}, ticket: ticket, epoch: b.epoch}
	b.active = j
	return j
}

func TestCancellationRetainsSlotUntilOwnedCompletionAndSaveRequiresReload(t *testing.T) {
	b := testBoard(t)
	j := fakeJob(t, b, "save")
	prior := b.state.Clone()
	cancelled := false
	b.cancel = func() { cancelled = true }
	b.cancelWork()
	if !cancelled || b.active != j || !b.binding.InFlight() {
		t.Fatal("cancel freed active worker before reap")
	}
	if b.workText() != "Worker active: save" {
		t.Fatal("readiness claimed idle before reap", b.workText())
	}
	if err := b.dataWork(workRequest{Operation: "load"}); err == nil {
		t.Fatal("queued replacement while worker active")
	}
	if err := b.complete(completedJob{j, workResult{Schema: 1, Status: "cancelled"}}); err == nil {
		t.Fatal("cancelled save accepted")
	}
	if b.active != nil || b.binding.InFlight() || b.ready || !reflect.DeepEqual(prior, b.state) {
		t.Fatal("cancelled save changed committed state or retained reaped slot")
	}
	if b.workText() != "Worker idle; no pending work" {
		t.Fatal("reaped worker readiness wrong", b.workText())
	}
}

func TestPrivatePictureResultMustBelongToExactGeneration(t *testing.T) {
	cache := "/tmp/fvwm-pinboard-" + strings.Repeat("a", 32) + "/g-1"
	base := cache + "/.fvwm-xdg-" + strings.Repeat("b", 64)
	for _, picture := range []string{base + ".png", base + ".xpm", base + ".svg:48x48", ""} {
		if !validPicture(picture, cache) {
			t.Fatal("valid private image refused", picture)
		}
	}
	for _, picture := range []string{base + ".svg:0x48", base + ".png/../other", cache + "/.fvwm-xdg-../../other.png", strings.Replace(base, "g-1", "g-2", 1) + ".png"} {
		if validPicture(picture, cache) {
			t.Fatal("foreign or malformed picture admitted", picture)
		}
	}
}

func TestForeignAndFailedWorkerResultsRetainCommittedState(t *testing.T) {
	b := testBoard(t)
	j := fakeJob(t, b, "load")
	prior := b.state.Clone()
	if err := b.complete(completedJob{&job{serial: j.serial}, workResult{Status: "loaded"}}); err != nil || b.active != j {
		t.Fatal("foreign completion retired active slot", err)
	}
	if err := b.complete(completedJob{j, workResult{Schema: 1, Status: "failed", Error: "corrupt JSON"}}); err == nil {
		t.Fatal("failed load accepted")
	}
	if !b.ready || !reflect.DeepEqual(b.state, prior) {
		t.Fatal("failed load changed committed model")
	}
}

func TestStaleFormAndOversizedPathCannotMutateData(t *testing.T) {
	b := testBoard(t)
	b.form = &formView{serial: 1, revision: 0, epoch: b.epoch, mode: "new"}
	prior := b.state.Clone()
	for _, message := range []string{
		"FVWM_PINBOARD_FORM " + b.protocol.Token + " 1 1 " + b.epoch + " 0000000000000001 create xLw",
		"FVWM_PINBOARD_FORM " + b.protocol.Token + " 1 0 0000000000000002 0000000000000001 create xLw",
	} {
		if _, err := b.control(message); err == nil {
			t.Fatal("stale Form acted")
		}
	}
	if !reflect.DeepEqual(prior, b.state) || b.active != nil {
		t.Fatal("stale callback changed state")
	}
	if _, err := formData("x"+strings.Repeat("YQ", 300), 384); err == nil {
		t.Fatal("oversized new-pin input admitted")
	}
}

// The peer implements only the frozen public transaction responses. This
// checks observable batching/barriers without starting X or another process.
func publicationPeer(t *testing.T, b *board, epoch string, initialPending bool) (<-chan []string, func()) {
	t.Helper()
	client, server := net.Pipe()
	b.session = transport.New(client)
	b.ctx = context.Background()

	b.renew = time.NewTicker(time.Hour)
	done := make(chan []string, 1)
	initialView := b.view
	go func() {
		defer server.Close()
		scanner := bufio.NewScanner(server)
		commands := []string{}
		current, pending := initialView, uint32(0)
		mapping := initialPending
		commitQueued := false
		for scanner.Scan() {
			line := scanner.Text()
			commands = append(commands, line)
			f := strings.Fields(line)
			if len(f) < 4 {
				continue
			}
			switch f[2] {
			case "Begin":
				n, _ := strconv.ParseUint(f[5], 10, 32)
				pending = uint32(n)
			case "Commit":
				if initialPending {
					commitQueued = true
				} else {
					current, pending = pending, 0
				}
			case "Abort":
				pending = 0
			case "Status":
				state := "ready"
				if pending != 0 || mapping {
					state = "pending"
				}
				replyEpoch := epoch
				if current == 0 && commitQueued {
					state = "unavailable"
					replyEpoch = "0000000000000000"
				}
				_ = b.nativeReplies.offer(f[4], fmt.Sprintf("FVWM_PINBOARD_STATUS %s %s %d %d %s 100 %s", b.protocol.Token, f[4], current, pending, state, replyEpoch))
				if commitQueued {
					current, pending = pending, 0
					commitQueued = false
					continue
				}
				if mapping && pending == 0 {
					mapping = false
					continue
				}
				if current != initialView && pending == 0 || len(commands) >= 2 && strings.Contains(commands[len(commands)-2], " Abort ") {
					done <- commands
					return
				}
			}
		}
		done <- commands
	}()
	return done, func() { b.renew.Stop(); b.session.Close() }
}

func TestSceneUsesActualNativeBarrierWithin32Records(t *testing.T) {
	b := testBoard(t)
	state := pinboard.Empty()
	for i := 0; i < 256; i++ {
		state.Pins = append(state.Pins, pinboard.Pin{ID: fmt.Sprintf("%032x", i), Target: []byte("/"), Label: "Root"})
	}
	done, close := publicationPeer(t, b, b.epoch, false)
	defer close()
	committed, err := b.publish(state, nil, nil, "")
	if err != nil || !committed || b.view != 2 {
		t.Fatal(committed, b.view, err)
	}
	commands := <-done
	between, barriers := 0, 0
	for _, command := range commands {
		if len(command) > 1000 || strings.HasPrefix(command, "Echo ") {
			t.Fatal("wrong transport barrier", command)
		}
		if strings.Contains(command, " Status ") {
			if between > 32 {
				t.Fatal("unbounded native staging", between)
			}
			between = 0
			barriers++
		} else {
			between++
		}
	}
	if barriers < 17 {
		t.Fatal("missing native barriers", barriers)
	}
}

func TestStaleImageStagingAcknowledgesAbortBeforeReturning(t *testing.T) {
	b := testBoard(t)
	state := pinboard.Empty()
	for i := 0; i < 32; i++ {
		state.Pins = append(state.Pins, pinboard.Pin{ID: fmt.Sprintf("%032x", i), Target: []byte("/"), Label: "Root"})
	}
	done, close := publicationPeer(t, b, "0000000000000002", false)
	defer close()
	committed, err := b.publish(state, nil, nil, b.epoch)
	if err == nil || committed || b.view != 1 || b.nativeLost {
		t.Fatal(committed, b.view, b.nativeLost, err)
	}
	commands := <-done
	if len(commands) < 2 || !strings.Contains(commands[len(commands)-2], " Abort ") || !strings.Contains(commands[len(commands)-1], " Status ") {
		t.Fatal("abort not acknowledged before return", commands)
	}
}

func TestCommittedSceneWaitsForActualMappingReadiness(t *testing.T) {
	b := testBoard(t)
	done, close := publicationPeer(t, b, b.epoch, true)
	defer close()
	committed, err := b.publish(pinboard.Empty(), nil, nil, "")
	if err != nil || !committed {
		t.Fatal(committed, err)
	}
	commands := <-done
	statuses := 0
	for _, command := range commands {
		if strings.Contains(command, " Status ") {
			statuses++
		}
	}
	if statuses != 3 {
		t.Fatal("mapping readiness was not acknowledged", commands)
	}
}

func TestInitialSceneWaitsForAdmittedMonitorAcquisition(t *testing.T) {
	b := testBoard(t)
	b.view, b.nextView = 0, 0
	b.epoch = "0000000000000000"
	done, close := publicationPeer(t, b, "0000000000000002", true)
	defer close()
	committed, err := b.publish(pinboard.Empty(), nil, nil, "")
	if err != nil || !committed || b.epoch != "0000000000000002" {
		t.Fatal(committed, b.epoch, err)
	}
	<-done
}

func TestReaderDiscardsUnsubmittedAndRetiredRepliesBeforeActiveMailbox(t *testing.T) {
	b := testBoard(t)
	client, peer := net.Pipe()
	b.session = transport.New(client)
	defer b.session.Close()
	defer peer.Close()
	b.ctx = context.Background()

	b.finished = make(chan error, 1)
	b.renew = time.NewTicker(time.Hour)
	defer b.renew.Stop()
	for i := 0; i < 8; i++ {
		b.state.Pins = append(b.state.Pins, pinboard.Pin{ID: fmt.Sprintf("%032x", i), Target: []byte("/"), Label: "Root"})
	}
	b.icons = &iconAcquisition{view: b.view, epoch: b.epoch}
	go b.read()
	initial := make(chan error, 1)
	peerResult := make(chan error, 1)
	retired := make(chan struct{})
	go func() {
		encoder := json.NewEncoder(peer)
		emit := func(message string) error {
			return encoder.Encode(map[string]any{"echo": map[string]string{"message": message}})
		}
		status := func(tag string) string {
			return fmt.Sprintf("FVWM_PINBOARD_STATUS %s %s 1 0 ready 100 %s", b.protocol.Token, tag, b.epoch)
		}
		for i := 0; i < 80; i++ {
			if err := emit(status(fmt.Sprintf("observer-s-%d", i))); err != nil {
				initial <- err
				return
			}
		}
		initial <- nil
		scanner := bufio.NewScanner(peer)
		if !scanner.Scan() {
			peerResult <- fmt.Errorf("active Status command missing: %v", scanner.Err())
			return
		}
		fields := strings.Fields(scanner.Text())
		if len(fields) != 5 || fields[2] != "Status" {
			peerResult <- fmt.Errorf("unexpected query %q", scanner.Text())
			return
		}
		firstTag := fields[4]
		for i := 0; i < 80; i++ {
			if err := emit(status(fmt.Sprintf("observer-active-%d", i))); err != nil {
				peerResult <- err
				return
			}
		}
		if err := emit(status(firstTag)); err != nil {
			peerResult <- err
			return
		}
		<-retired
		for i := 0; i < 80; i++ {
			if err := emit(status(firstTag)); err != nil {
				peerResult <- err
				return
			}
		}
		if !scanner.Scan() {
			peerResult <- fmt.Errorf("active Cells command missing: %v", scanner.Err())
			return
		}
		fields = strings.Fields(scanner.Text())
		if len(fields) != 7 || fields[2] != "Cells" {
			peerResult <- fmt.Errorf("unexpected query %q", scanner.Text())
			return
		}
		tag := fields[4]
		for i := 0; i < 8; i++ {
			message := fmt.Sprintf("FVWM_PINBOARD_CELL %s %s 1 %s %d %032x - 4 0 %d 0 %d 96 96 1", b.protocol.Token, tag, b.epoch, i, i, i, i*96)
			if err := emit(message); err != nil {
				peerResult <- err
				return
			}
		}
		peerResult <- emit(fmt.Sprintf("FVWM_PINBOARD_CELLS_END %s %s 1 %s 0 8 8", b.protocol.Token, tag, b.epoch))
	}()
	if err := <-initial; err != nil {
		t.Fatalf("unsubmitted same-owner replies terminated reader: %v", err)
	}
	if status, err := b.barrier(); err != nil || status.Current != 1 {
		t.Fatal("active exact-tag Status was lost", status, err)
	}
	close(retired)
	page, err := b.cells(0)
	if err != nil || len(page.Cells) != 8 {
		t.Fatal("active maximum Cells page was lost", page, err)
	}
	if err := <-peerResult; err != nil {
		t.Fatal(err)
	}
	select {
	case err := <-b.finished:
		t.Fatal("reader terminated before owned session close", err)
	default:
	}
}

func TestQueryRegistrationPrecedesSendAndRetiresOnEveryOutcome(t *testing.T) {
	for _, outcome := range []string{"success", "send-error", "parse-error", "cancelled"} {
		t.Run(outcome, func(t *testing.T) {
			b := testBoard(t)
			client, peer := net.Pipe()
			b.session = transport.New(client)
			defer b.session.Close()
			defer peer.Close()
			ctx, cancel := context.WithCancel(context.Background())
			defer cancel()
			b.ctx = ctx
			b.renew = time.NewTicker(time.Hour)
			defer b.renew.Stop()
			if outcome == "cancelled" {
				cancel()
			}
			err := b.exchange("active", func() error {
				if outcome == "send-error" {
					return errors.New("send failed")
				}
				if outcome != "cancelled" {
					return b.nativeReplies.offer("active", "FVWM_PINBOARD_SYNC token active")
				}
				return nil
			}, func(message string) (bool, error) {
				if message != "FVWM_PINBOARD_SYNC token active" {
					t.Fatal("query missed immediate response")
				}
				if outcome == "parse-error" {
					return false, errors.New("parse failed")
				}
				return true, nil
			})
			if (err == nil) != (outcome == "success") {
				t.Fatal(outcome, err)
			}
			mailbox, err := b.nativeReplies.begin("next")
			if err != nil {
				t.Fatal("query registration leaked", err)
			}
			if _, err := b.nativeReplies.begin("overlap"); err == nil {
				t.Fatal("overlapping query admitted")
			}
			b.nativeReplies.retire("active")
			if err := b.nativeReplies.offer("next", "current"); err != nil {
				t.Fatal(err)
			}
			if got := <-mailbox; got != "current" {
				t.Fatal(got)
			}
			b.nativeReplies.clear()
			if _, err := b.nativeReplies.begin("after-retirement"); err != nil {
				t.Fatal("retirement did not clear registration", err)
			}
			b.nativeReplies.retire("after-retirement")
		})
	}
}

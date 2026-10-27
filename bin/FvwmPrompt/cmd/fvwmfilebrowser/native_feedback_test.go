// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"encoding/binary"
	"encoding/json"
	"fmt"
	"reflect"
	"strings"
	"testing"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/collection"
)

const feedbackInstance = "0123456789abcdef0123456789abcdef"

func TestNativeFeedbackMarkerParser(t *testing.T) {
	prefix := "FVWM_FORM_CHANGED " + feedbackInstance + "_f"
	for _, form := range []uint64{1, 15, 16, ^uint64(0)} {
		marker := fmt.Sprintf("%s%x", prefix, form)
		change, ok := parseNativeChange(marker, feedbackInstance)
		if !ok || change.form != form || len(strings.TrimPrefix(marker, "FVWM_FORM_CHANGED ")) > 64 {
			t.Fatal(marker, change, ok)
		}
	}
	for _, marker := range []string{
		prefix, prefix + "0", prefix + "01", prefix + "A", prefix + "+1",
		prefix + "10000000000000000", prefix + "1 trailing", prefix + "1\n",
		prefix + "1\t", " " + prefix + "1", strings.Replace(prefix+"1", " ", "\t", 1),
		strings.Replace(prefix+"1", "_f", "_w", 1), "FVWM_FORM_CHANGED other_f1",
		"FVWM_FORM_LIST " + feedbackInstance + "_f1 1 0 0 xAAAA",
	} {
		if change, ok := parseNativeChange(marker, feedbackInstance); ok {
			t.Fatal("malformed or foreign marker admitted", marker, change)
		}
	}
	if _, ok := parseNativeChange("FVWM_FORM_CHANGED invalid_f1", "invalid"); ok {
		t.Fatal("invalid instance admitted")
	}
}

func TestNativeFeedbackMailboxKeepsHighestLifetime(t *testing.T) {
	mailbox := make(chan nativeChange, 1)
	for _, form := range []uint64{4, 4, 2, 8, 3, 8, 1} {
		offerNativeChange(mailbox, nativeChange{form})
	}
	if len(mailbox) != 1 || (<-mailbox).form != 8 {
		t.Fatal("retired marker overwrote the current Form latch")
	}
	// Exercise the one producer/one consumer interleaving under the race gate.
	done := make(chan struct{})
	go func() {
		defer close(done)
		for form := uint64(1); form <= 10000; form++ {
			offerNativeChange(mailbox, nativeChange{form})
		}
	}()
	deadline := time.NewTimer(time.Second)
	defer deadline.Stop()
	for {
		select {
		case <-mailbox:
		case <-done:
			return
		case <-deadline.C:
			t.Fatal("bounded mailbox blocked its sole producer")
		}
	}
}

func TestNativeFeedbackTokensDoNotConsumeControlOrRenderSerial(t *testing.T) {
	b := &browser{instance: feedbackInstance, form: ^uint64(0), renderSerial: 9, lastControl: 17}
	if len(b.nativeWatchToken()) > 64 || b.nativeWatchToken() != feedbackInstance+"_fffffffffffffffff" {
		t.Fatal("unbounded or unstable watch token", b.nativeWatchToken())
	}
	first, err := b.nextFeedbackToken()
	second, err2 := b.nextFeedbackToken()
	if err != nil || err2 != nil || first == second || b.renderSerial != 9 || b.lastControl != 17 {
		t.Fatal("feedback reused or consumed unrelated token", first, second, err, err2)
	}
	b.feedbackSerial = ^uint64(0) - 1
	last, err := b.nextFeedbackToken()
	if err != nil || len(last) > 64 {
		t.Fatal("last token invalid", last, err)
	}
	if token, err := b.nextFeedbackToken(); err == nil || token != "" || b.feedbackSerial != ^uint64(0) {
		t.Fatal("feedback request counter wrapped", token, err)
	}
}

func feedbackReply(command string, generation uint32, packet []byte) string {
	return layoutReply("SendToModule unused ListStatus picks "+strings.Fields(command)[5]+" selection", generation, 0, packet)
}

func feedbackLabels(b *browser) {
	for _, label := range []nativeLabel{{"counts", b.countText()}, {"status", b.feedbackStatus()}, {"readiness", b.readinessText()}} {
		b.rememberFeedbackLabel(label)
	}
}

func TestNativeFeedbackUsesCurrentMappingAndOnlyChangedLabels(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.instance = feedbackInstance
	// The same Form has published a new generation and reordered rows since
	// its marker was queued. Only the current acknowledged mapping may act.
	b.nativeGeneration = 8
	b.viewEntries[0], b.viewEntries[1] = b.viewEntries[1], b.viewEntries[0]
	feedbackLabels(b)
	beforeBindings, beforeModel := b.bindingStats(), b.model.Generation
	beforeSerial, beforeRender := b.serial, b.renderSerial
	current := false
	commands, err := layoutScript(t, b, peer, func() error {
		var err error
		current, err = b.nativeFeedback(nativeChange{b.form})
		return err
	}, func(command string) string {
		if strings.Contains(command, " ListWatchStatus ") {
			fields := strings.Fields(command)
			if len(fields) != 6 || fields[4] != feedbackInstance+"_f3" || fields[5] != feedbackInstance+"_w1" {
				t.Fatal("uncorrelated subscription snapshot", command)
			}
			// An unrelated ordinary transaction cannot satisfy the watch ACK.
			if err := b.routeNativeReply("FVWM_FORM_LIST " + feedbackInstance + "_w0 8 0 0 xbad"); err != nil {
				t.Fatal(err)
			}
			return feedbackReply(command, 8, []byte{0, 0, 3})
		}
		return ""
	})
	if err != nil || !current || b.model.Focus != "b" || !b.model.Selected["a"] || !b.model.Selected["b"] || b.model.HiddenSelectedCount() != 2 {
		t.Fatal("feedback lost current row identities/hidden selection", err, b.model.SelectedIDs())
	}
	if !reflect.DeepEqual(b.bindingStats(), beforeBindings) || b.model.Generation != beforeModel || b.serial != beforeSerial || b.renderSerial != beforeRender || b.nativeGeneration != 8 || b.form != 3 || b.lastControl != 0 || b.active || b.pending != nil || b.replaceSelection {
		t.Fatal("feedback advanced publication/action state")
	}
	labels := 0
	for _, command := range commands {
		if strings.Contains(command, " SetLabel ") {
			labels++
			if !strings.Contains(command, " SetLabel counts ") {
				t.Fatal("unchanged label painted", command)
			}
		} else if !strings.Contains(command, " ListWatchStatus ") && !strings.HasSuffix(command, " RenewTimeout 3000") {
			t.Fatal("feedback invoked publication/action", command)
		}
	}
	if labels != 1 {
		t.Fatal("selection did not paint exactly the changed count", commands)
	}
	beforeProvider := b.providerGeneration()
	commands, err = layoutScript(t, b, peer, func() error {
		_, err := b.nativeFeedback(nativeChange{b.form})
		return err
	}, func(command string) string {
		if strings.Contains(command, " ListWatchStatus ") {
			if !strings.HasSuffix(command, " "+feedbackInstance+"_w2") {
				t.Fatal("request token reused", command)
			}
			return feedbackReply(command, 8, []byte{0, 0, 3})
		}
		return ""
	})
	if err != nil || len(commands) != 2 || b.providerGeneration() != beforeProvider {
		t.Fatal("unchanged feedback repainted or published", err, commands)
	}
	for _, form := range []uint64{b.form - 1, b.form + 1} {
		if current, err := b.nativeFeedback(nativeChange{form}); err != nil || current || b.feedbackSerial != 2 {
			t.Fatal("retired/future Form affected current state", current, err)
		}
	}
}

func TestNativeFeedbackOverflowPreservesModelAndRecovers(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.instance = feedbackInstance
	entries := make([]collection.Entry, 258)
	selected := make([]string, 128)
	for i := range entries {
		label := "shown"
		if i < 128 {
			label = "hidden"
			selected[i] = fmt.Sprint(i)
		}
		entries[i] = collection.Entry{ID: fmt.Sprint(i), Label: label}
	}
	if err := b.model.SetFilter(""); err != nil {
		t.Fatal(err)
	}
	if err := b.model.Replace(entries); err != nil {
		t.Fatal(err)
	}
	if err := b.model.SelectIDs(b.model.Generation, selected); err != nil {
		t.Fatal(err)
	}
	if err := b.model.SetFilter("shown"); err != nil {
		t.Fatal(err)
	}
	b.viewEntries = b.model.VisibleEntries()
	feedbackLabels(b)
	before := b.model.SelectedIDs()
	focus := b.model.Focus
	packet := make([]byte, 2+(len(b.viewEntries)+7)/8)
	binary.BigEndian.PutUint16(packet, 1)
	for i := 0; i < 129; i++ {
		packet[2+i/8] |= 1 << uint(i%8)
	}
	run := func() error { _, err := b.nativeFeedback(nativeChange{b.form}); return err }
	reply := func(command string) string {
		if strings.Contains(command, " ListWatchStatus ") {
			return feedbackReply(command, b.nativeGeneration, packet)
		}
		return ""
	}
	commands, err := layoutScript(t, b, peer, run, reply)
	if err != nil || b.feedbackError != "native selection exceeds limit" || !reflect.DeepEqual(before, b.model.SelectedIDs()) || b.model.Focus != focus || b.replaceSelection {
		t.Fatal("overflow changed admitted model/native selection", err, b.feedbackError)
	}
	if !strings.Contains(strings.Join(commands, "\n"), `SetLabel readiness "Selection unavailable;`) || !strings.Contains(strings.Join(commands, "\n"), "SetLabel status") {
		t.Fatal("overflow was silent", commands)
	}
	// Native remains editable; one fewer visible selection admits all 256
	// combined hidden/visible identities and clears only the feedback error.
	packet[2+128/8] &^= 1
	commands, err = layoutScript(t, b, peer, run, reply)
	if err != nil || b.feedbackError != "" || len(b.model.Selected) != 256 || b.model.HiddenSelectedCount() != 128 || b.replaceSelection {
		t.Fatal("valid selection did not recover", err, b.feedbackError, len(b.model.Selected))
	}
	if !strings.Contains(strings.Join(commands, "\n"), "SetLabel status") || !strings.Contains(strings.Join(commands, "\n"), "SetLabel readiness") {
		t.Fatal("recovery feedback not painted", commands)
	}
}

func TestNativeFeedbackRefusalAndFreshCallbackRecovery(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.instance = feedbackInstance
	before := b.model.SelectedIDs()
	for _, failure := range []string{"stale", "pending", "refused", "malformed"} {
		_, err := layoutScript(t, b, peer, func() error {
			_, err := b.nativeFeedback(nativeChange{b.form})
			return err
		}, func(command string) string {
			if !strings.Contains(command, " ListWatchStatus ") {
				return ""
			}
			reply := feedbackReply(command, 7, []byte{0, 0, 2})
			switch failure {
			case "stale":
				return feedbackReply(command, 6, []byte{0, 0, 2})
			case "pending":
				return strings.Replace(reply, " 7 0 0 ", " 7 8 0 ", 1)
			case "refused":
				return strings.Replace(reply, " 7 0 0 ", " 7 0 1 ", 1)
			default:
				return feedbackReply(command, 7, []byte{0, 0, 128})
			}
		})
		if err != nil || b.feedbackError == "" || !reflect.DeepEqual(before, b.model.SelectedIDs()) || b.replaceSelection {
			t.Fatal("invalid observation changed model or became terminal", failure, err)
		}
	}
	stale := strings.Fields(testControl(b, "properties", "-", []byte{0, 0, 1}))
	stale[2] = "2"
	if _, err := b.action(strings.Join(stale, " ")); err == nil || b.feedbackError == "" {
		t.Fatal("stale callback cleared unavailable feedback")
	}
	if _, err := b.action(testControl(b, "unfilter", "-", []byte{0, 0, 1})); err != nil || b.feedbackError != "" || b.lastControl != 1 {
		t.Fatal("valid fresh native callback did not recover", err)
	}
}

func TestNativeFeedbackLostProducerDeadlineAndTransport(t *testing.T) {
	b, _ := layoutBrowser(t, "list")
	b.renderDeadline = time.Now().Add(10 * time.Millisecond)
	if _, err := b.waitNative("lost", b.nativeGeneration, 0); err == nil || !strings.Contains(err.Error(), "timed out") {
		t.Fatal("lost producer had no bounded timeout", err)
	}
	b.session.Close()
	if current, err := b.nativeFeedback(nativeChange{b.form}); err == nil || current {
		t.Fatal("lost transport became a successful feedback transaction", current, err)
	}
}

func TestNativeFeedbackStartupSubscriptionAndAutomaticPublicationLease(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.instance = feedbackInstance
	b.renderDeadline = time.Now().Add(time.Second)
	notify := ""
	commands, err := layoutScript(t, b, peer, b.render, func(command string) string {
		if strings.Contains(command, ": ListNotify ") {
			notify = strings.Fields(command)[2]
		}
		if strings.HasPrefix(command, "Module FvwmForm ") {
			return layoutReply("SendToModule unused ListStatus picks "+notify+" selection", 1, 0, []byte{0, 1, 1})
		}
		return ""
	})
	if err != nil || b.form != 4 {
		t.Fatal("startup failed", err)
	}
	watch := "*" + b.alias + ": ListWatch " + feedbackInstance + "_f4"
	found := false
	for i, command := range commands {
		if command == watch {
			found = i > 0 && strings.HasSuffix(commands[i-1], ": ListVersioned")
		}
	}
	if !found {
		t.Fatal("watch did not opt in after versioned List", commands)
	}
	watchToken := b.nativeWatchToken()
	commands, err = layoutScript(t, b, peer, b.show, func(command string) string {
		if strings.Contains(command, " ListStatus ") {
			return layoutReply(command, 1, 0, []byte{0, 1, 1})
		}
		return ""
	})
	if err != nil || b.nativeWatchToken() != watchToken {
		t.Fatal("ordinary publication changed subscription lifetime", err)
	}
	for _, command := range commands {
		if strings.Contains(command, "RenewTimeout") || strings.Contains(command, "ListWatchStatus") || strings.Contains(command, ": ListWatch ") {
			t.Fatal("automatic publication renewed idle lease or acknowledged interaction", command)
		}
	}
}

func TestNativeFeedbackReaderRetiresTimedOutReplyBeforeAdmission(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.instance = feedbackInstance
	// Execute the same JSON transport/read-admission path as run(), with no
	// blocking test-side sends into b.native that could conceal queue overflow.
	readerDone := make(chan error, 1)
	go func() {
		readerDone <- b.session.Read(func(record json.RawMessage) error {
			var event struct {
				Echo struct{ Message string } `json:"echo"`
			}
			if err := json.Unmarshal(record, &event); err != nil {
				return err
			}
			return b.routeNativeReply(event.Echo.Message)
		})
	}()
	t.Cleanup(func() {
		b.session.Close()
		<-readerDone
	})
	tokenA, tokenB := feedbackInstance+"_w1", feedbackInstance+"_w2"
	replies := b.nativeWatchReplies.begin(tokenA)
	b.renderDeadline = time.Now().Add(10 * time.Millisecond)
	if _, _, err := b.waitNativeReply(replies, tokenA, 7, 0, 0); err == nil || !strings.Contains(err.Error(), "timed out") {
		t.Fatal("request A did not time out recoverably", err)
	}
	b.nativeWatchReplies.retire(tokenA)
	replies = b.nativeWatchReplies.begin(tokenB)
	b.renderDeadline = time.Now().Add(time.Second)
	lateA := layoutReply("SendToModule unused ListStatus picks "+tokenA+" selection", 7, 0, []byte{0, 0, 1})
	validB := layoutReply("SendToModule unused ListStatus picks "+tokenB+" selection", 7, 0, []byte{0, 1, 2})
	ordinary := layoutReply("SendToModule unused ListStatus picks "+feedbackInstance+"_12_current selection", 7, 0, []byte{0, 0, 1})
	var records strings.Builder
	for _, message := range []string{lateA, validB, ordinary} {
		data, err := json.Marshal(map[string]any{"echo": map[string]string{"message": message}})
		if err != nil {
			t.Fatal(err)
		}
		records.Write(data)
		records.WriteByte('\n')
	}
	if _, err := peer.Write([]byte(records.String())); err != nil {
		t.Fatal("reader failed on consecutive late/current replies", err)
	}
	// This ordinary response is a causal barrier proving the actual reader
	// admitted both earlier records while B's sole reply remained queued.
	select {
	case reply := <-b.native:
		if reply != ordinary {
			t.Fatal("watch reply contaminated the ordinary ACK slot", reply)
		}
	case err := <-readerDone:
		readerDone <- err
		t.Fatal("reader terminated on late/current replies", err)
	case <-time.After(time.Second):
		t.Fatal("reader did not reach ordinary ACK barrier")
	}
	packet, observed, err := b.waitNativeReply(replies, tokenB, 7, 0, 0)
	b.nativeWatchReplies.retire(tokenB)
	if err != nil || observed != 7 || !reflect.DeepEqual(packet, []byte{0, 1, 2}) {
		t.Fatal("late A displaced/lost valid B", packet, observed, err)
	}
	if len(b.native) != 0 || len(replies) != 0 {
		t.Fatal("completion leaked a reply into a later transaction")
	}
}

func TestNativeFeedbackReplyGateRetirementRacesAndOrdinaryIsolation(t *testing.T) {
	b := &browser{instance: feedbackInstance, native: make(chan string, 1), nativeWatchReplies: &nativeWatchReplyGate{}}
	old := feedbackInstance + "_w1"
	current := feedbackInstance + "_w2"
	for i := 0; i < 1000; i++ {
		b.nativeWatchReplies.begin(old)
		start, done := make(chan struct{}), make(chan struct{})
		go func() {
			defer close(done)
			<-start
			b.nativeWatchReplies.offer(old, "late A")
		}()
		close(start)
		// Old admission can race before, during or after retirement/begin.
		// All three orders must leave exactly B available to its consumer.
		b.nativeWatchReplies.retire(old)
		replies := b.nativeWatchReplies.begin(current)
		<-done
		b.nativeWatchReplies.retire(old) // a late retire cannot retire B
		b.nativeWatchReplies.offer(current, "valid B")
		b.nativeWatchReplies.offer(current, "duplicate B")
		select {
		case reply := <-replies:
			if reply != "valid B" {
				t.Fatal("retirement/admission race leaked A or replaced B", reply)
			}
		default:
			t.Fatal("retirement/admission race lost B")
		}
		b.nativeWatchReplies.retire(current)
	}
	// Ordinary publication remains usable while a watch response is queued,
	// and retired/malformed watch tokens never consume its separate slot.
	replies := b.nativeWatchReplies.begin(current)
	ordinary := "FVWM_FORM_LIST " + feedbackInstance + "_12_current 7 0 0 xbad"
	watch := "FVWM_FORM_LIST " + current + " 7 0 0 xbad"
	for _, message := range []string{watch, "FVWM_FORM_LIST " + old + " 7 0 0 xbad", "FVWM_FORM_LIST " + feedbackInstance + "_wgarbage 7 0 0 xbad", ordinary} {
		if err := b.routeNativeReply(message); err != nil {
			t.Fatal("watch traffic overflowed ordinary ACK admission", err)
		}
	}
	if len(b.native) != 1 || (<-b.native) != ordinary || len(replies) != 1 || (<-replies) != watch {
		t.Fatal("ordinary and watch ACK slots were not isolated")
	}
	b.nativeWatchReplies.retire(current)
	if err := b.routeNativeReply(watch); err != nil || len(replies) != 0 || len(b.native) != 0 {
		t.Fatal("completed watch request remained admissible", err)
	}
	// Preserve the ordinary-ACK bounded failure policy.
	if err := b.routeNativeReply(ordinary); err != nil {
		t.Fatal(err)
	}
	if err := b.routeNativeReply(ordinary); err == nil {
		t.Fatal("ordinary ACK overflow became silent")
	}
}

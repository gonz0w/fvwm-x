// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"errors"
	"strconv"
	"strings"
	"sync"
	"time"
)

// The reader validates only immutable instance identity. Form lifetime and the
// acknowledged row mapping are owned by the main loop, never read concurrently.
type nativeChange struct {
	form uint64
}

func parseNativeChange(message, instance string) (nativeChange, bool) {
	const prefix = "FVWM_FORM_CHANGED "
	if len(instance) != 32 {
		return nativeChange{}, false
	}
	for _, c := range instance {
		if !(c >= '0' && c <= '9' || c >= 'a' && c <= 'f') {
			return nativeChange{}, false
		}
	}
	value, ok := strings.CutPrefix(message, prefix+instance+"_f")
	if !ok || len(value) == 0 || len(value) > 16 {
		return nativeChange{}, false
	}
	form, err := strconv.ParseUint(value, 16, 64)
	if err != nil || form == 0 || strconv.FormatUint(form, 16) != value {
		return nativeChange{}, false
	}
	return nativeChange{form: form}, true
}

// Exactly one reader produces and one main loop consumes. Draining a full
// mailbox must preserve the highest queued lifetime: a late retired Form must
// not overwrite the current Form's only latched wakeup.
func offerNativeChange(mailbox chan nativeChange, change nativeChange) {
	select {
	case mailbox <- change:
		return
	default:
	}
	select {
	case previous := <-mailbox:
		if previous.form > change.form {
			change = previous
		}
	default:
	}
	mailbox <- change
}

func (b *browser) nativeWatchToken() string {
	return b.instance + "_f" + strconv.FormatUint(b.form, 16)
}

func (b *browser) nextFeedbackToken() (string, error) {
	if b.feedbackSerial == ^uint64(0) {
		return "", errors.New("native feedback request generation exhausted")
	}
	b.feedbackSerial++
	return b.instance + "_w" + strconv.FormatUint(b.feedbackSerial, 16), nil
}

// Watch responses can arrive after their recoverable deadline. Correlate before
// queue admission, independently of ordinary publication ACKs. The mutex makes
// checking the current request, enqueueing, and retirement/draining atomic with
// respect to the reader, so an old response cannot occupy its successor's slot.
// Only the main loop begins/retires requests; the reader only offers responses.
type nativeWatchReplyGate struct {
	mu      sync.Mutex
	token   string
	replies chan string
}

func (g *nativeWatchReplyGate) begin(token string) <-chan string {
	g.mu.Lock()
	defer g.mu.Unlock()
	if g.replies == nil {
		g.replies = make(chan string, 1)
	}
	g.drain()
	g.token = token
	return g.replies
}

// drain is called only while holding mu.
func (g *nativeWatchReplyGate) drain() {
	select {
	case <-g.replies:
	default:
	}
}

func (g *nativeWatchReplyGate) retire(token string) {
	g.mu.Lock()
	defer g.mu.Unlock()
	if g.token == token {
		g.token = ""
		g.drain()
	}
}

func (g *nativeWatchReplyGate) offer(token, reply string) {
	g.mu.Lock()
	defer g.mu.Unlock()
	if token == "" || token != g.token {
		return
	}
	select {
	case g.replies <- reply:
	default:
		// One exact response is already queued. A duplicate cannot replace it.
	}
}

// This is the reader's actual admission path. The watch namespace is reserved
// even for retired or malformed tokens; none may enter the ordinary ACK slot.
func (b *browser) routeNativeReply(message string) error {
	if !strings.HasPrefix(message, "FVWM_FORM_LIST "+b.instance+"_") {
		return nil
	}
	fields := strings.Fields(message)
	if len(fields) >= 2 && strings.HasPrefix(fields[1], b.instance+"_w") {
		if b.nativeWatchReplies != nil {
			b.nativeWatchReplies.offer(fields[1], message)
		}
		return nil
	}
	select {
	case b.native <- message:
		return nil
	default:
		return errors.New("native collection reply queue is full")
	}
}

func (b *browser) feedbackStatus() string {
	if b.feedbackError != "" {
		return "Selection unavailable: " + b.feedbackError
	}
	return b.status
}

func (b *browser) rememberFeedbackLabel(label nativeLabel) {
	if b.feedbackLabels == nil {
		b.feedbackLabels = make(map[string]string)
	}
	b.feedbackLabels[label.name] = label.value
}

func (b *browser) paintNativeFeedback() error {
	labels := []nativeLabel{{"counts", b.countText()},
		{"readiness", b.readinessText()}, {"status", b.feedbackStatus()}}
	for _, label := range labels {
		if previous, ok := b.feedbackLabels[label.name]; ok && previous == label.value {
			continue
		}
		if err := b.session.Send("SendToModule " + b.alias + " SetLabel " + label.name +
			" \"" + displayText(label.value, 120) + "\""); err != nil {
			return err
		}
		b.rememberFeedbackLabel(label)
	}
	return nil
}

func (b *browser) renewNativeIdle() error {
	return b.session.Send("SendToModule " + b.alias + " RenewTimeout 3000")
}

func (b *browser) snapshotNativeFeedback(token string) ([]byte, error) {
	replies := b.nativeWatchReplies.begin(token)
	defer b.nativeWatchReplies.retire(token)
	if err := b.session.Send("SendToModule " + b.alias + " ListWatchStatus picks " +
		b.nativeWatchToken() + " " + token); err != nil {
		return nil, err
	}
	packet, _, err := b.waitNativeReply(replies, token, b.nativeGeneration, 0, 0)
	return packet, err
}

// Feedback is serialized with publication. A marker carries no row/generation
// authority; even one queued before an update snapshots the current native
// generation and reconciles only its acknowledged mapping. Refused admission
// preserves the model and native selection for a later valid interaction.
func (b *browser) nativeFeedback(change nativeChange) (bool, error) {
	if change.form != b.form || b.alias == "" || b.nativeGeneration == 0 {
		return false, nil
	}
	token, err := b.nextFeedbackToken()
	if err != nil {
		return false, err
	}
	b.renderDeadline = time.Now().Add(workerLimit)
	packet, err := b.snapshotNativeFeedback(token)
	if err == nil {
		err = b.model.SelectNativeView(b.model.Generation, entryIDs(b.viewEntries), packet)
	}
	if err == nil {
		err = b.retireStaleFileRoles()
	}
	b.feedbackError = ""
	if err != nil {
		b.feedbackError = err.Error()
	}
	if err := b.paintNativeFeedback(); err != nil {
		return false, err
	}
	if err := b.renewNativeIdle(); err != nil {
		return false, err
	}
	return true, nil
}

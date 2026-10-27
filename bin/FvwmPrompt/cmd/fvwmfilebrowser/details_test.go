// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"encoding/base64"
	"encoding/binary"
	"fmt"
	"reflect"
	"strconv"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

func TestDetailEncodingCompleteAndBounded(t *testing.T) {
	raw := strings.Repeat("a", 1300) + "$%\\\"&[U+0041]\n\x00é\xff\xc0"
	fields, err := admitDetailFields([]nativeLabel{{"literal", raw}})
	if err != nil {
		t.Fatal(err)
	}
	value := strings.Join(fields[0].parts, "")
	want := strings.Repeat("a", 1300) + "[U+0024][U+0025][U+005C][U+0022][U+0026][U+005B]U+0041[U+005D][U+000A][U+0000][U+00E9][BYTE+FF][BYTE+C0]"
	if value != want || strings.ContainsAny(value, "$%\\\"&\n\x00") {
		t.Fatalf("incomplete/unsafe representation: %q", value)
	}
	var reconstructed strings.Builder
	for remaining := value; len(remaining) > 0; {
		if remaining[0] != '[' {
			reconstructed.WriteByte(remaining[0])
			remaining = remaining[1:]
			continue
		}
		end := strings.IndexByte(remaining, ']')
		token := remaining[1:end]
		if hex, ok := strings.CutPrefix(token, "BYTE+"); ok {
			n, err := strconv.ParseUint(hex, 16, 8)
			if err != nil {
				t.Fatal(err)
			}
			reconstructed.WriteByte(byte(n))
		} else {
			hex, ok := strings.CutPrefix(token, "U+")
			if !ok {
				t.Fatal(token)
			}
			n, err := strconv.ParseUint(hex, 16, 32)
			if err != nil {
				t.Fatal(err)
			}
			reconstructed.WriteRune(rune(n))
		}
		remaining = remaining[end+1:]
	}
	if reconstructed.String() != raw {
		t.Fatal("complete parts cannot reconstruct original filename bytes")
	}
	for _, part := range fields[0].parts {
		if len(part) > detailPartLimit || strings.Count(part, "[") != strings.Count(part, "]") {
			t.Fatal("split encoding token", part)
		}
	}
	if _, err := admitDetailFields([]nativeLabel{{"too-large", strings.Repeat("x", detailSourceLimit+1)}}); err == nil {
		t.Fatal("source limit silently truncated")
	}
	if _, err := admitDetailFields([]nativeLabel{{"expanded", strings.Repeat("\x00", detailSourceLimit)}}); err == nil {
		t.Fatal("expanded limit silently truncated")
	}
	empty, err := admitDetailFields([]nativeLabel{{"empty", ""}})
	if err != nil || !reflect.DeepEqual(empty[0].parts, []string{""}) {
		t.Fatal(empty, err)
	}
}

func detailPacket(rows, focus int) string {
	data := make([]byte, 6+(rows+7)/8)
	binary.BigEndian.PutUint32(data, 1)
	binary.BigEndian.PutUint16(data[4:], uint16(focus))
	return "x" + base64.RawURLEncoding.EncodeToString(data)
}

func TestDetailSectionRequiresExactPacket(t *testing.T) {
	for _, rows := range []int{1, 11, 13} {
		for _, focus := range []int{0, rows - 1} {
			got, err := detailSection(detailPacket(rows, focus), rows)
			if err != nil || got != focus {
				t.Fatal(got, err)
			}
		}
		for _, value := range []string{detailPacket(rows, rows), detailPacket(rows, 65535), detailPacket(rows+8, 1), "x", strings.Replace(detailPacket(rows, 1), "x", "x_", 1)} {
			if _, err := detailSection(value, rows); err == nil {
				t.Fatal("malformed/outside section admitted", value)
			}
		}
	}
}

func detailMessage(b *browser, action string, focus int, ticket uint64) string {
	d := b.details
	return fmt.Sprintf("FVWM_FILES_DETAILS_1 %s %d %d %d %s %s %016x", b.instance, b.form, d.lifetime, d.page, action, detailPacket(len(d.fields), focus), ticket)
}

func TestDetailsPreserveParentAndRejectRetiredCallbacks(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.snapshot.Items = []filemodel.Item{{ID: "b", Name: []byte(strings.Repeat("name", 400))}}
	b.inspected = []string{"b"}
	before := b.providerGeneration()
	generation, serial, form, native := b.model.Generation, b.renderSerial, b.form, b.nativeGeneration
	selected, focus := b.model.SelectedIDs(), b.model.Focus
	commands, err := layoutScript(t, b, peer, func() error { return b.openDetails() }, func(string) string { return "" })
	if err != nil {
		t.Fatal(err)
	}
	if b.details == nil || len(b.details.fields) != 13 {
		t.Fatal("missing complete values", b.details)
	}
	for _, field := range b.details.fields {
		if field.name == "Focused name" && strings.Join(field.parts, "") != strings.Repeat("name", 400) {
			t.Fatal("full raw name lost")
		}
	}
	for _, command := range commands {
		if strings.Contains(command, "same-form") || strings.Contains(command, "ListNotify") || strings.Contains(command, "ListStatus") || strings.Contains(command, "SetInput") {
			t.Fatal("disclosure touched parent/ACK", command)
		}
	}
	for _, want := range []string{": UIScale monitor", ": List sections 40 4", ": ListViewport 18 1", ": ListVersioned", ": InputReadonly value", ": InputHex value 40 x", ": CloseCommand Echo FVWM_FILES_DETAILS_1"} {
		found := false
		for _, command := range commands {
			found = found || strings.Contains(command, want)
		}
		if !found {
			t.Fatal("missing native disclosure control", want, commands)
		}
	}
	initial := detailMessage(b, "read", 11, 1)
	callback, ok := parseDetailCallback(initial, b.instance)
	if !ok {
		t.Fatal(initial)
	}
	_, err = layoutScript(t, b, peer, func() error {
		accepted, err := b.detailAction(callback)
		if !accepted && err == nil {
			return fmt.Errorf("read not accepted")
		}
		return err
	}, func(string) string { return "" })
	if err != nil || b.details.section != 11 {
		t.Fatal(err, b.details)
	}
	for _, stale := range []detailCallback{callback, {parent: b.form + 1, lifetime: b.details.lifetime, page: b.details.page, action: "next", ticket: 2, packet: detailPacket(len(b.details.fields), 11)}} {
		if accepted, err := b.detailAction(stale); accepted || err != nil {
			t.Fatal("stale request affected parent", accepted, err)
		}
	}
	next, _ := parseDetailCallback(detailMessage(b, "next", 11, 1), b.instance)
	_, err = layoutScript(t, b, peer, func() error { _, err := b.detailAction(next); return err }, func(string) string { return "" })
	if err != nil || b.details.part != 1 {
		t.Fatal(err, b.details)
	}
	if b.model.Generation != generation || b.renderSerial != serial || b.form != form || b.nativeGeneration != native || b.providerGeneration() != before || b.model.Focus != focus || !reflect.DeepEqual(b.model.SelectedIDs(), selected) {
		t.Fatal("observational disclosure changed parent")
	}
	_, err = layoutScript(t, b, peer, b.retireDetails, func(string) string { return "" })
	if err != nil || b.details != nil {
		t.Fatal(err)
	}
	if accepted, err := b.detailAction(next); accepted || err != nil {
		t.Fatal("retired callback failed parent", accepted, err)
	}
}

func TestDetailImmutableReaderAdmission(t *testing.T) {
	b := browser{instance: "owner", detailEvents: make(chan detailCallback, 1), acks: make(chan string, 1), native: make(chan string, 1)}
	valid := "FVWM_FILES_DETAILS_1 owner 3 1 2 read " + detailPacket(11, 1) + " 0000000000000001"
	if err := b.routeDetailCallback(valid); err != nil || len(b.detailEvents) != 1 || len(b.acks) != 0 || len(b.native) != 0 {
		t.Fatal("detail entered ordinary queue", err)
	}
	<-b.detailEvents
	for _, bad := range []string{valid + " extra", strings.Replace(valid, " read ", " exec ", 1), strings.Replace(valid, " 3 ", " 0 ", 1), strings.Replace(valid, "0000000000000001", "0000000000000000", 1)} {
		if err := b.routeDetailCallback(bad); err != nil || len(b.detailEvents) != 0 {
			t.Fatal("invalid detail callback admitted", bad, err)
		}
	}
}

func TestDetailOpenAndRefusalAreObservations(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.detailEvents = make(chan detailCallback, 1)
	b.status = strings.Repeat("x", detailSourceLimit+1)
	model := b.model
	form, serial, native, provider := b.form, b.renderSerial, b.nativeGeneration, b.providerGeneration()
	message := fmt.Sprintf("FVWM_FILES_DETAILS_OPEN_1 %s %d %016x", b.instance, b.form, 1)
	if err := b.routeDetailCallback(message); err != nil || len(b.detailEvents) != 1 {
		t.Fatal("open routing", err)
	}
	callback := <-b.detailEvents
	commands, err := layoutScript(t, b, peer, func() error {
		accepted, err := b.acceptDetailOpen(callback)
		if !accepted {
			t.Fatal("current open refused")
		}
		return err
	}, func(string) string { return "" })
	if err != nil || len(b.details.fields) != 1 || b.details.fields[0].name != "Snapshot refused" || !strings.HasPrefix(b.details.fields[0].parts[0], "Details refused:") {
		t.Fatal("oversize snapshot not visibly refused", err)
	}
	if !reflect.DeepEqual(b.model, model) || b.form != form || b.renderSerial != serial || b.nativeGeneration != native || b.providerGeneration() != provider {
		t.Fatal("open reconciled parent state")
	}
	for _, stale := range []detailCallback{callback, {parent: form + 1, ticket: 2, action: "open"}, {parent: form, ticket: 0, action: "open"}} {
		if accepted, err := b.acceptDetailOpen(stale); accepted || err != nil {
			t.Fatal("stale open admitted", accepted, err)
		}
	}
	for _, command := range commands {
		if strings.Contains(command, "same-form") || len(command) > 1000 {
			t.Fatal("parent mutation or oversized command", command)
		}
	}
}

func TestDetailAckHasSeparateCorrelatedAdmission(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.detailAckReplies = &nativeWatchReplyGate{}
	b.acks = make(chan string, 1)
	b.instance = strings.Repeat("a", 32)
	b.detailSerial = ^uint64(0) - 1
	b.detailSendSerial = ^uint64(0) - 2
	b.snapshot.Path = []byte(strings.Repeat("p", 4000))
	commands, err := layoutScript(t, b, peer, b.openDetails, func(command string) string {
		if strings.HasPrefix(command, "Echo FVWM_FILES_DETAILS_SYNC ") {
			tag := strings.TrimPrefix(command, "Echo ")
			b.routeDetailAck(tag + " stale")
			b.routeDetailAck(tag)
		}
		return ""
	})
	if err != nil || len(b.acks) != 0 || len(b.native) != 0 || b.renderSerial != 12 {
		t.Fatal("detail ACK polluted parent", err)
	}
	for _, command := range commands {
		if len(command) > 1000 {
			t.Fatal("worst-lifetime command exceeds core bound", len(command), command)
		}
	}
	b.routeDetailAck("FVWM_FILES_DETAILS_SYNC " + b.instance + " retired")
	if b.detailAckReplies.token != "" || len(b.detailAckReplies.replies) != 0 {
		t.Fatal("retired ack admitted")
	}
}

func TestDetailsRetireBeforeParentReplacement(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	_, err := layoutScript(t, b, peer, b.openDetails, func(string) string { return "" })
	if err != nil {
		t.Fatal(err)
	}
	oldAlias := b.details.alias
	b.renderPath = "/old-folder"
	notify := ""
	commands, err := layoutScript(t, b, peer, b.show, func(command string) string {
		if strings.Contains(command, ": ListNotify ") {
			notify = strings.Fields(command)[2]
		}
		if strings.HasPrefix(command, "Module FvwmForm ") {
			return layoutReply("SendToModule unused ListStatus picks "+notify+" selection", 1, 0, []byte{0, 1, 1})
		}
		return ""
	})
	if err != nil || b.details != nil || len(commands) < 2 || commands[0] != "KillModule FvwmForm "+oldAlias || commands[1] != "DestroyModuleConfig "+oldAlias+":*" {
		t.Fatal("parent replacement retained details", err, commands)
	}
}

func TestDetailsRemainFrozenDuringSameFolderUpdate(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.status = "original snapshot status"
	_, err := layoutScript(t, b, peer, b.openDetails, func(string) string { return "" })
	if err != nil {
		t.Fatal(err)
	}
	frozen := b.details
	b.status = "background status changed"
	commands, err := layoutScript(t, b, peer, b.show, func(command string) string {
		if strings.Contains(command, " ListStatus ") {
			return layoutReply(command, 7, 0, []byte{0, 1, 1})
		}
		return ""
	})
	if err != nil || b.details != frozen || frozen.fields[1].parts[0] != "original snapshot status" {
		t.Fatal("same-folder update rewrote disclosure", err)
	}
	for _, command := range commands {
		if strings.Contains(command, frozen.alias) {
			t.Fatal("same-folder update replaced snapshot", command)
		}
	}
}

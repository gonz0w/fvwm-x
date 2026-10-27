// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"context"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strconv"
	"strings"
	"testing"
	"time"

	"golang.org/x/sys/unix"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopcheck"
)

const fileSource = `{"version":1,"kind":"file","path":"/tmp/a$\"%&\\配置","first":2,"last":4,"include":[{"path":"/tmp/root","first":7,"last":9}],"reason":""}`

func sourceFrame(kind, key uint64, text string) frame {
	body := make([]byte, 24)
	binary.NativeEndian.PutUint64(body, key)
	body = append(body, text...)
	return frame{kind, append(body, 0)}
}
func literalCommands(t *testing.T, commands []string) string {
	t.Helper()
	var result strings.Builder
	for _, command := range commands {
		if _, data, ok := strings.Cut(command, "TextHex x"); ok {
			decoded, err := hex.DecodeString(data)
			if err != nil || !utf8.Valid(decoded) || utf8.RuneCount(decoded) > 72 {
				t.Fatalf("invalid literal row: %s", command)
			}
			result.Write(decoded)
		} else {
			result.WriteString(command)
		}
		result.WriteByte('\n')
	}
	return result.String()
}
func TestConfigSourceStrictSchema(t *testing.T) {
	source, err := decodeConfigSource([]byte(fileSource))
	if err != nil || source.site.first != 2 || source.site.last != 4 || len(source.include) != 1 {
		t.Fatalf("valid source: %+v %v", source, err)
	}
	for _, kind := range []string{"generated", "invocation", "unavailable"} {
		if _, err := decodeConfigSource([]byte(`{"version":1,"kind":"` + kind + `"}`)); err != nil {
			t.Fatal(kind, err)
		}
	}
	for _, bad := range []string{
		`null`, `[]`, `{}`, `{"version":1,"kind":null}`, `{"version":1.0,"kind":"file"}`,
		`{"version":1,"kind":"file","path":"/a","first":1,"last":1,"extra":true}`,
		strings.Replace(fileSource, `"version":1`, `"version":1,"version":1`, 1),
		strings.Replace(fileSource, `"first":2`, `"first":null`, 1),
		strings.Replace(fileSource, `"first":2`, `"first":2.0`, 1),
		strings.Replace(fileSource, `"first":2`, `"first":2e0`, 1),
		strings.Replace(fileSource, `"first":2`, `"first":-1`, 1),
		strings.Replace(fileSource, `"first":2`, `"first":0`, 1),
		strings.Replace(fileSource, `"first":2`, `"first":4294967296`, 1),
		strings.Replace(fileSource, `"last":4`, `"last":1`, 1),
		strings.Replace(fileSource, `"include":[`, `"include":null,"ignored":[`, 1),
		strings.Replace(fileSource, `"reason":""`, `"reason":null`, 1),
		`{"version":1,"kind":"file","path":"/a\n","first":1,"last":1}`,
		`{"version":1,"kind":"file","path":"/a\ud800","first":1,"last":1}`,
		`{"version":1,"kind":"file","path":"/a\udc00","first":1,"last":1}`,
		`{"version":1,"kind":"file","path":"relative","first":1,"last":1}`,
		`{"version":1,"kind":"generated","path":"/a"}`,
		`{"version":1,"kind":"unavailable","include":[{"path":"/a","first":1,"last":1}]}`,
		`{"version":1,"kind":"unavailable","reason":"` + strings.Repeat("a", sourceLimit) + `"}`,
		string([]byte{'{', 0xff, '}'}),
	} {
		if _, err := decodeConfigSource([]byte(bad)); err == nil {
			t.Fatalf("accepted: %s", bad)
		}
	}
	for _, data := range []string{
		`{"version":1,"kind":"file","path":"/a\ud83d\ude00","first":4294967295,"last":4294967295}`,
		`{"version":1,"kind":"file","path":"/a","first":1,"last":1,"include":[]}`,
	} {
		if _, err := decodeConfigSource([]byte(data)); err != nil {
			t.Fatal("valid Unicode/span rejected", err)
		}
	}
	base := `{"version":1,"kind":"unavailable","reason":""}`
	atLimit := strings.Replace(base, `"reason":""`, `"reason":"`+strings.Repeat("x", sourceLimit-len(base))+`"`, 1)
	if len(atLimit) != sourceLimit {
		t.Fatal("bad exact bound fixture")
	}
	if _, err := decodeConfigSource([]byte(atLimit)); err != nil {
		t.Fatal("exact source byte bound rejected", err)
	}
	if _, err := decodeConfigSource([]byte(atLimit + " ")); err == nil {
		t.Fatal("oversize source admitted")
	}
	parent := `{"path":"/a","first":1,"last":1}`
	for _, n := range []int{16, 17} {
		text := `{"version":1,"kind":"file","path":"/a","first":1,"last":1,"include":[` + strings.TrimSuffix(strings.Repeat(parent+",", n), ",") + `]}`
		if _, err := decodeConfigSource([]byte(text)); (err == nil) != (n == 16) {
			t.Fatalf("include bound %d: %v", n, err)
		}
	}
}
func TestConfigSourcePairCannotLeak(t *testing.T) {
	metadata := sourceFrame(configSourceMessage, 5, fileSource)
	config := sourceFrame(configInfo, 5, "*FvwmDesktop: palette invalid")
	for _, middle := range []frame{
		{kind: newDesk}, sourceFrame(configInfo, 6, "same text"), sourceFrame(configInfo, 0, "same text"),
		sourceFrame(configSourceMessage, 5, `null`),
		sourceFrame(configSourceMessage, 5, ""),
		sourceFrame(configSourceMessage, 5, string([]byte{0xff})), sourceFrame(configSourceMessage, 0, fileSource),
		sourceFrame(configSourceMessage, 5, strings.Repeat("a", sourceLimit+1)),
		{kind: configSourceMessage, body: make([]byte, 24)},
	} {
		var pending sourcePair
		pending.consume(metadata)
		pending.consume(middle)
		if pending.consume(config) != nil {
			t.Fatal("stale source reused", middle.kind)
		}
	}
	var pending sourcePair
	pending.consume(metadata)
	if pending.consume(config) == nil || pending.consume(config) != nil {
		t.Fatal("source not consumed exactly once")
	}
	metadata.body[8] = 1
	pending.consume(metadata)
	if pending.consume(config) != nil {
		t.Fatal("reserved field accepted")
	}
}
func TestSourcePairSurvivesEveryFragmentBoundary(t *testing.T) {
	encode := func(f frame) []byte {
		data := append([]byte{}, f.body...)
		for len(data)%8 != 0 {
			data = append(data, 0)
		}
		packet := []byte{}
		for _, word := range []uint64{0xffffffff, f.kind, uint64(len(data)/8 + 4), 0} {
			packet = binary.NativeEndian.AppendUint64(packet, word)
		}
		return append(packet, data...)
	}
	wire := append(encode(sourceFrame(configSourceMessage, 7, fileSource)), encode(sourceFrame(configInfo, 7, "*FvwmDesktop: palette invalid"))...)
	for split := 0; split <= len(wire); split++ {
		var pending sourcePair
		var buffer []byte
		paired := 0
		for _, chunk := range [][]byte{wire[:split], wire[split:]} {
			buffer = append(buffer, chunk...)
			for {
				packet, n, err := nextFrame(buffer)
				if err != nil {
					t.Fatal(err)
				}
				if n == 0 {
					break
				}
				if pending.consume(packet) != nil {
					paired++
				}
				buffer = buffer[n:]
			}
		}
		if paired != 1 || len(buffer) != 0 || pending.source != nil {
			t.Fatal("fragmented pair lost", split)
		}
	}
}
func TestConfigurationErrorsHaveLiteralSourceAndReplay(t *testing.T) {
	d, commands := fixture(t)
	source, err := decodeConfigSource([]byte(fileSource))
	if err != nil {
		t.Fatal(err)
	}
	for _, ready := range []bool{false, true} {
		d.ready = ready
		*commands = nil
		d.configureMessage("*FvwmDesktop: palette invalid", source, applyConfiguration)
		shown := literalCommands(t, *commands)
		if !strings.Contains(shown, `Source: /tmp/a$"%&\配置:2-4`) || !strings.Contains(shown, "Read from /tmp/root:7-9") || !strings.Contains(shown, "Invalid value for palette") {
			t.Fatal(shown)
		}
		*commands = nil
		d.configureMessage("*FvwmDesktop: palette invalid", nil, applyConfiguration)
		shown = literalCommands(t, *commands)
		if !strings.Contains(shown, "Source context unavailable") || strings.Contains(shown, "/tmp/a") {
			t.Fatal("source reused", shown)
		}
	}
	*commands = nil
	d.configureMessage("*FvwmDesktop: palette ink", source, applyConfiguration)
	if len(*commands) != 0 || d.overrides["palette"] != "ink" {
		t.Fatal("valid setting changed behavior")
	}
	data, _ := json.Marshal(map[string]any{"version": 1, "kind": "generated", "reason": "PipeRead output"})
	generated, _ := decodeConfigSource(data)
	if !strings.Contains(generated.text(), "definition context unavailable") || strings.Contains(generated.text(), "Source: /tmp") {
		t.Fatal("generated definition fabricated")
	}
}
func TestNativeReportRetainsPathsSpansAndWholeIncludeChain(t *testing.T) {
	d, commands := fixture(t)
	path := "/tmp/" + strings.Repeat("配置", 80) + `$"%&\`
	d.report = &report{"test", desktopcheck.Result{Errors: 1, Diagnostics: []desktopcheck.Diagnostic{{Location: desktopcheck.Location{Source: desktopcheck.Source{Path: path, Line: 2, EndLine: 4}, IncludedFrom: []desktopcheck.Source{{Path: "/root\tfile", Line: 7, EndLine: 9}, {Path: "/nested", Line: 10, EndLine: 10}}}, Severity: "error", Message: `bad $"%&\`}}}}
	*commands = nil
	if err := d.showReport("test", 0); err != nil {
		t.Fatal(err)
	}
	rows := []string{}
	for _, command := range *commands {
		if _, _, ok := strings.Cut(command, "TextHex x"); ok {
			rows = append(rows, command)
		}
	}
	shown := strings.ReplaceAll(literalCommands(t, rows), "\n", "")
	for _, want := range []string{path + ":2-4", `Read from /root\tfile:7-9`, "Read from /nested:10", `bad $"%&\`} {
		if !strings.Contains(shown, want) {
			t.Fatal("source bytes lost", want, shown)
		}
	}
}

func TestReportPaginationBoundsWrappedRowsAndPreservesAllContext(t *testing.T) {
	for _, count := range []int{0, 1, 4} {
		t.Run(strconv.Itoa(count), func(t *testing.T) {
			d, commands := fixture(t)
			result := desktopcheck.Result{Errors: count}
			var expected strings.Builder
			if count == 0 {
				expected.WriteString("No errors in the supported literal settings subset.")
			}
			for i := 0; i < count; i++ {
				path := fmt.Sprintf("/source-%d/", i) + strings.Repeat("配置", 115) + `$"%&\`
				item := desktopcheck.Diagnostic{Location: desktopcheck.Location{Source: desktopcheck.Source{Path: path, Line: 2, EndLine: 4}}, Severity: "error", Message: fmt.Sprintf("Invalid value for palette %d", i)}
				expected.WriteString(path + ":2-4error: " + item.Message)
				for parent := 0; parent < 7; parent++ {
					path := fmt.Sprintf("/parent-%d-%d/", i, parent) + strings.Repeat("p", 225) + `$"%&\`
					item.IncludedFrom = append(item.IncludedFrom, desktopcheck.Source{Path: path, Line: parent + 1, EndLine: parent + 2})
					fmt.Fprintf(&expected, "Read from %s:%d-%d", path, parent+1, parent+2)
				}
				result.Diagnostics = append(result.Diagnostics, item)
			}
			d.report = &report{"bounded", result}
			pages := reportPages(result)
			if len(pages) < 1 || (count > 0 && len(pages) <= count) {
				t.Fatal("long context did not receive continuation pages", len(pages))
			}
			var actual strings.Builder
			for page := range pages {
				*commands = nil
				if err := d.showReport("bounded", page); err != nil {
					t.Fatal(err)
				}
				published := append([]string{}, (*commands)...)
				rows, callbacks := 0, 0
				for _, command := range published {
					if _, data, ok := strings.Cut(command, "TextHex x"); ok {
						text, err := hex.DecodeString(data)
						if err != nil || !utf8.Valid(text) || utf8.RuneCount(text) > 72 {
							t.Fatal("invalid physical row", command, err)
						}
						actual.Write(text)
						rows++
					}
					if argument, ok := strings.CutPrefix(command, "*SlateConfigCheckForm: Command SendToModule FvwmDesktop check-page "); ok {
						fields := strings.Fields(argument)
						if len(fields) != 2 || fields[0] != "bounded" {
							t.Fatal("invalid callback identity", command)
						}
						target, err := strconv.Atoi(fields[1])
						if err != nil || target < 0 || target >= len(pages) || (target != page-1 && target != page+1) {
							t.Fatal("invalid continuation callback", command)
						}
						if err := d.handle("check-page", argument); err != nil {
							t.Fatal("published callback rejected", err)
						}
						callbacks++
					}
				}
				if rows < 1 || rows > reportRowsPerPage || callbacks != boolInt(page > 0)+boolInt(page+1 < len(pages)) {
					t.Fatal("page exceeded physical row budget or lost navigation", page, rows, callbacks)
				}
			}
			if actual.String() != expected.String() {
				t.Fatal("continuation pages changed, dropped or repeated source text")
			}
			*commands = nil
			for _, page := range []int{-1, len(pages)} {
				if err := d.showReport("bounded", page); err == nil || len(*commands) != 0 {
					t.Fatal("invalid page published")
				}
			}
			if err := d.showReport("stale", 0); err == nil || len(*commands) != 0 {
				t.Fatal("stale callback published")
			}
		})
	}
}

func boolInt(value bool) int {
	if value {
		return 1
	}
	return 0
}

func TestLiveConfigurationValidatesWithoutApplying(t *testing.T) {
	d, commands := fixture(t)
	d.configureMessage("*FvwmDesktop: palette ink", nil, applyConfiguration)
	if err := d.finishConfiguration(); err != nil {
		t.Fatal(err)
	}
	before, err := json.Marshal(d.snapshot(nil, nil))
	if err != nil {
		t.Fatal(err)
	}
	terminal := append([]string(nil), d.terminal...)
	var pending sourcePair
	for _, text := range []string{"*FvwmDesktop: palette grove", "*FvwmDesktop: motion off", "*FvwmDesktop: terminal /unexpected/live/terminal"} {
		*commands = nil
		pending.consume(sourceFrame(configSourceMessage, 99, fileSource))
		source := pending.consume(sourceFrame(configInfo, 99, text))
		d.configureMessage(text, source, validateConfiguration)
		after, _ := json.Marshal(d.snapshot(nil, nil))
		if string(after) != string(before) || !reflect.DeepEqual(d.terminal, terminal) || len(*commands) != 0 || pending.source != nil {
			t.Fatal("valid live configuration changed resident state", text)
		}
	}
	for _, text := range []string{"*FvwmDesktop: palette invalid", "*FvwmDesktop: unknown invalid"} {
		*commands = nil
		pending.consume(sourceFrame(configSourceMessage, 99, fileSource))
		d.configureMessage(text, pending.consume(sourceFrame(configInfo, 99, text)), validateConfiguration)
		shown := literalCommands(t, *commands)
		if !strings.Contains(shown, `Source: /tmp/a$"%&\配置:2-4`) || (!strings.Contains(shown, "Invalid value for palette") && !strings.Contains(shown, "Unknown FvwmDesktop option")) {
			t.Fatal("invalid live configuration lacked sourced notice", shown)
		}
		after, _ := json.Marshal(d.snapshot(nil, nil))
		if string(after) != string(before) || !reflect.DeepEqual(d.terminal, terminal) || pending.source != nil {
			t.Fatal("invalid live configuration changed resident state")
		}
		*commands = nil
		d.configureMessage(text, pending.consume(sourceFrame(configInfo, 99, text)), validateConfiguration)
		if shown := literalCommands(t, *commands); !strings.Contains(shown, "Source context unavailable") || strings.Contains(shown, "/tmp/a") {
			t.Fatal("live diagnostic reused consumed context", shown)
		}
	}
}

func TestModuleSubscribesToLiveConfigurationAndSource(t *testing.T) {
	input, coreWrite, err := os.Pipe()
	if err != nil {
		t.Fatal(err)
	}
	defer input.Close()
	defer coreWrite.Close()
	coreRead, output, err := os.Pipe()
	if err != nil {
		t.Fatal(err)
	}
	defer coreRead.Close()
	defer output.Close()
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	root, err := filepath.Abs("../../../../default-config")
	if err != nil {
		t.Fatal(err)
	}
	done := make(chan error, 1)
	go func() {
		done <- Run(Options{Context: ctx, Root: root, StatePath: filepath.Join(t.TempDir(), "state.json"), System: "Linux", Input: int(input.Fd()), Output: int(output.Fd())})
	}()
	var buffer []byte
	commands := []string{}
	deadline := time.Now().Add(3 * time.Second)
	for len(commands) < 3 && time.Now().Before(deadline) {
		fds := []unix.PollFd{{Fd: int32(coreRead.Fd()), Events: unix.POLLIN}}
		if _, err := unix.Poll(fds, 100); err != nil {
			t.Fatal(err)
		}
		if fds[0].Revents&unix.POLLIN == 0 {
			continue
		}
		data := make([]byte, 4096)
		n, err := unix.Read(int(coreRead.Fd()), data)
		if err != nil {
			t.Fatal(err)
		}
		buffer = append(buffer, data[:n]...)
		for len(buffer) >= 24 {
			n := int(binary.NativeEndian.Uint64(buffer[8:]))
			if n > 4096 {
				t.Fatal("invalid module output size")
			}
			if len(buffer) < n+24 {
				break
			}
			commands = append(commands, string(buffer[16:16+n]))
			buffer = buffer[24+n:]
		}
	}
	cancel()
	select {
	case err := <-done:
		if err != nil {
			t.Fatal(err)
		}
	case <-time.After(time.Second):
		t.Fatal("module did not stop after cancellation")
	}
	if !reflect.DeepEqual(commands, []string{fmt.Sprintf("SET_MASK %d", uint64((1<<22)|(1<<1)|(1<<18)|(1<<19)|(1<<27))), fmt.Sprintf("SET_MASK %d", uint64((1<<31)|(1<<10)|(1<<11))), "Send_ConfigInfo *FvwmDesktop"}) {
		t.Fatal("module omitted live/source subscription", commands)
	}
}

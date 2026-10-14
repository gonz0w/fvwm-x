// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"bytes"
	"context"
	"errors"
	"io"
	"strings"
	"testing"
)

func TestFormat(t *testing.T) {
	m := mode{name: "alpha", description: "Shows café pattern"}
	for _, test := range []struct{ format, want string }{
		{"%n: %D / %d", "alpha: café pattern / Shows café pattern"},
		{"%8n:%-8n", "   alpha:alpha   "},
		{"%*5n:%*-5n", "al...:...ha"},
		{"%*3n:%*99n:%0n", "alpha:alpha:alpha"},
		{"%8*4n:%-8*-4n", "    a...:...a    "},
		{"%q %% %D", "%q %% café pattern"},
	} {
		var out bytes.Buffer
		if err := formatItem(&out, test.format, m); err != nil || out.String() != test.want {
			t.Fatalf("%q: %q, %v; want %q", test.format, out.String(), err, test.want)
		}
	}
	var out bytes.Buffer
	if err := formatItem(&out, `%n`, mode{name: "a\"b\\c\nd"}); err != nil || out.String() != `a\"b\\c d` {
		t.Fatalf("literal external label: %q, %v", out.String(), err)
	}
	// Configured literal escapes are preserved, unlike external substitutions.
	out.Reset()
	if err := formatItem(&out, `literal\" %n`, m); err != nil || out.String() != `literal\" alpha` {
		t.Fatalf("configured literal: %q, %v", out.String(), err)
	}
	if err := formatItem(&out, "%999999999999999999999999n", m); err == nil {
		t.Fatal("overflow width accepted")
	}
}

type failedWriter struct{ calls int }

func (w *failedWriter) Write(p []byte) (int, error) {
	w.calls++
	return 0, errors.New("closed output")
}

func TestOutputFailure(t *testing.T) {
	out := &failedWriter{}
	if err := formatItem(out, "%1000000000n", mode{name: "x"}); err == nil || out.calls != 1 {
		t.Fatalf("large padding failed to stop: %d, %v", out.calls, err)
	}
	if err := menu(out, defaults(), nil); err == nil {
		t.Fatal("menu hid output failure")
	}
}

func TestOptions(t *testing.T) {
	for _, args := range [][]string{
		{"--name=Menu", "-t", "Title", "--it=%n", "--wm", "--s", "--", "-nolock"},
		{"-NAME", "Menu", "--TITLE=Title", "--item", "%n", "--wm-icons", "--special-first", "-", "-nolock"},
	} {
		// The second form deliberately leaves an unrecognized xlock option.
		var out, diagnostic bytes.Buffer
		o, done, status := parse(args, &out, &diagnostic, "tool")
		if strings.Contains(strings.Join(args, " "), " - -nolock") {
			if !done || status != 255 {
				t.Fatal("missing -- was accepted")
			}
			continue
		}
		if done || status != 0 || o.name != "Menu" || o.title != "Title" || o.itemIcon != "menu/lock.xpm" || !o.specialFirst || len(o.params) != 1 || o.params[0] != "-nolock" {
			t.Fatalf("%v: %+v %v %d %q", args, o, done, status, diagnostic.String())
		}
	}
	for _, args := range [][]string{{"--name"}, {"--i"}, {"--wm-icons=yes"}, {"--unknown"}} {
		var out, diagnostic bytes.Buffer
		_, done, status := parse(args, &out, &diagnostic, "tool")
		if !done || status != 255 || !strings.Contains(diagnostic.String(), "Try 'tool --help'") {
			t.Fatalf("%v: %d %q", args, status, diagnostic.String())
		}
	}
	var out, diagnostic bytes.Buffer
	_, done, status := parse([]string{"--title", "Changed", "-h"}, &out, &diagnostic, "tool")
	if !done || status != 0 || !strings.Contains(out.String(), "default is 'Changed'") {
		t.Fatal("help lost parsed options")
	}
}

func TestModes(t *testing.T) {
	entries := modes([]byte("preamble\nwhere mode is one of:\n  alpha Shows Alpha\n  -help option\n  invalid;name text\n  blank Shows Blank\n  random Random\n"))
	if len(entries) != 3 || entries[0].special || !entries[1].special || !entries[2].special {
		t.Fatalf("%+v", entries)
	}
	if len(modes([]byte("  alpha ignored before heading\n"))) != 0 {
		t.Fatal("heading required")
	}
}

// Hide the source's WriterTo so io.Copy exercises its normal reader path.
type readerOnly struct{ io.Reader }

func TestProbeCopyLimit(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	b := probeBuffer{cancel: cancel}
	_, err := io.Copy(&b, readerOnly{strings.NewReader(strings.Repeat("x", probeLimit+1))})
	if err == nil || !b.full || ctx.Err() == nil || b.data.Len() > probeLimit {
		t.Fatal("stream copy bypassed the limit")
	}
}

func TestProbeBuffer(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	b := probeBuffer{cancel: cancel}
	if n, err := b.Write(bytes.Repeat([]byte{'x'}, probeLimit)); n != probeLimit || err != nil {
		t.Fatal(n, err)
	}
	if n, err := b.Write([]byte{'y'}); n != 0 || err == nil || !b.full || ctx.Err() == nil || b.data.Len() != probeLimit {
		t.Fatal("limit failed")
	}
}

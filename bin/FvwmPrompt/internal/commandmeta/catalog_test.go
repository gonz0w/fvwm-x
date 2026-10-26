package commandmeta

import (
	"strings"
	"testing"
)

func TestGeneratedMetadataOwnershipAndLiteralCompletion(t *testing.T) {
	commands := Commands()
	if len(commands) < 100 {
		t.Fatal("missing embedded command inventory")
	}
	first := commands[0]
	commands[0].Name = "mutated"
	if Commands()[0] != first {
		t.Fatal("shared catalog mutation")
	}
	for _, c := range []struct{ line, suffix string }{
		{"Mov", "e "},
		{"  SendToM", "odule "},
		{"SendToModule FvwmDesktop set contr", "ast "},
		{"SendToModule FvwmDesktop set contrast h", "igh "},
		{"SendToModule FvwmDesktop set transparency o", "n "},
		{"SendToModule FvwmDesktop reset group:wall", "paper "},
	} {
		got, _ := Complete(c.line, len([]rune(c.line)))
		found := false
		for _, s := range got {
			found = found || string(s) == c.suffix
		}
		if !found {
			t.Fatalf("%q: %q", c.line, got)
		}
	}
	for _, text := range []string{"Exec sec", "SendToModule Other set cont", "SendToModule FvwmDesktop toggle contrast ", "SendToModule FvwmDesktop set day ", "Read $HOME", "Read `private`", "Move;Quit", "Move\nQuit"} {
		if got, _ := Complete(text, len([]rune(text))); len(got) != 0 {
			t.Fatalf("unsupported grammar %q: %q", text, got)
		}
	}
	if got, _ := Complete("Move", 9); len(got) != 0 {
		t.Fatal(got)
	}
	if got, _ := Complete(strings.Repeat("x", 4097), 4097); len(got) != 0 {
		t.Fatal(got)
	}
	prefix := "SendToModule FvwmDesktop set contrast h"
	got, offset := Complete(prefix+" trailing", len([]rune(prefix)))
	if len(got) != 1 || string(got[0]) != "igh " || offset != 1 {
		t.Fatal(got, offset)
	}
}

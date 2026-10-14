// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"strings"
	"testing"
)

// These vectors are authored from the Desktop Entry Exec/value specifications;
// the Python regex/provider and shell tokenizers are not the reference oracle.
func TestExecLiteralVectors(t *testing.T) {
	context := ExecContext{Name: "Café %U", Icon: "icon with space", Location: "/fixture/app menu.desktop",
		Files: []string{"/fixture/one file", "/fixture/two%U"}, URLs: []string{"file:///fixture/one", "https://example.invalid/two"}}
	cases := []struct {
		name, raw string
		context   ExecContext
		want      []string
	}{
		{"plain", "fixture one two", context, []string{"fixture", "one", "two"}},
		{"empty-and-space", `fixture "two words" ""`, context, []string{"fixture", "two words", ""}},
		{"executable-path", `"/fixture/app with space" arg`, context, []string{"/fixture/app with space", "arg"}},
		{"percent", `fixture %% "100%%"`, context, []string{"fixture", "%", "100%"}},
		{"metadata", "fixture %i %c %k", context, []string{"fixture", "--icon", "icon with space", "Café %U", "/fixture/app menu.desktop"}},
		{"no-metadata", "fixture %i %k", ExecContext{}, []string{"fixture", ""}},
		{"single-file", "fixture %f", context, []string{"fixture", "/fixture/one file"}},
		{"many-files", "fixture %F", context, []string{"fixture", "/fixture/one file", "/fixture/two%U"}},
		{"single-url", "fixture %u", context, []string{"fixture", "file:///fixture/one"}},
		{"many-urls", "fixture %U", context, []string{"fixture", "file:///fixture/one", "https://example.invalid/two"}},
		{"no-files", "fixture %F end", ExecContext{}, []string{"fixture", "end"}},
		{"single-file-embedded", "fixture --file=%f", context, []string{"fixture", "--file=/fixture/one file"}},
		{"deprecated", "fixture %d %D %n %N %v %m end", context, []string{"fixture", "end"}},
		{"embedded-name", "fixture --name=%c", context, []string{"fixture", "--name=Café %U"}},
		{"two-escape-passes", `fixture "a\\\\b" "\\$" "\\""`, context, []string{"fixture", `a\b`, "$", `"`}},
		{"quoted-reserved-literal", `fixture "a&b;*.txt"`, context, []string{"fixture", "a&b;*.txt"}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			got, err := ExpandExec(c.raw, c.context)
			if err != nil || !reflect.DeepEqual(got, c.want) {
				t.Fatal(got, c.want, err)
			}
		})
	}
}

func TestExecRefusesInvalidSource(t *testing.T) {
	cases := map[string]string{
		"empty":             "",
		"empty-executable":  `"" argument`,
		"equals-executable": "fixture=name arg",
		"single-quotes":     "fixture 'two words'",
		"partial-quotes":    `fixture a"b"`,
		"partial-tail":      `fixture "a"b`,
		"open-quote":        `fixture "open`,
		"unknown-code":      "fixture %z",
		"open-code":         "fixture %",
		"multiple-files":    "fixture %f %F",
		"multiple-mixed":    "fixture %f %u",
		"embedded-many":     "fixture --file=%F",
		"embedded-icon":     "fixture --icon=%i",
		"quoted-code":       `fixture "%c"`,
		"executable-code":   "%c arg",
		"unquoted-control":  "fixture\targument",
		"unquoted-reserved": "fixture a&b",
		"quoted-dollar":     `fixture "$value"`,
		"unknown-escape":    `fixture "\q"`,
		"incomplete-escape": `fixture "\\`,
		"non-ascii-source":  "fixture café",
		"nul":               "fixture\x00argument",
		"long-source":       strings.Repeat("a", MaxValueBytes+1),
	}
	for name, raw := range cases {
		t.Run(name, func(t *testing.T) {
			if argv, err := ExpandExec(raw, ExecContext{}); err == nil || argv != nil {
				t.Fatal("accepted invalid Exec", argv, err)
			}
		})
	}
}

func TestExecReplacementBounds(t *testing.T) {
	cases := []struct {
		name, raw string
		context   ExecContext
	}{
		{"nul-name", "fixture %c", ExecContext{Name: "one\x00two"}},
		{"nul-file", "fixture %F", ExecContext{Files: []string{"one\x00two"}}},
		{"long-name", "fixture %c", ExecContext{Name: strings.Repeat("a", MaxValueBytes+1)}},
		{"many-files", "fixture %F", ExecContext{Files: make([]string, MaxItems+1)}},
		{"expanded-word", "fixture prefix%c", ExecContext{Name: strings.Repeat("a", MaxValueBytes)}},
		{"expanded-total", "fixture %F", ExecContext{Files: func() []string {
			values := make([]string, 129)
			for i := range values {
				values[i] = strings.Repeat("a", MaxValueBytes)
			}
			return values
		}()}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			if argv, err := ExpandExec(c.raw, c.context); err == nil || argv != nil {
				t.Fatal("accepted excessive replacement", len(argv), err)
			}
		})
	}
}

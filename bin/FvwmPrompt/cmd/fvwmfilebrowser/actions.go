// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"errors"
	"strings"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

const maxActions = 4

type configuredAction struct{ Label, Program string }
type actionFlags []configuredAction

func (a *actionFlags) String() string { return "" }
func (a *actionFlags) Set(value string) error {
	if len(*a) >= maxActions || len(value) > filemodel.MaxPath+33 {
		return errors.New("at most four actions with bounded label/executable are supported")
	}
	label, program, found := strings.Cut(value, "=")
	if !found || label == "" || strings.TrimSpace(label) == "" || len(label) > 32 ||
		!utf8.ValidString(label) || strings.ContainsAny(label, "\x00\r\n") ||
		program == "" || !utf8.ValidString(program) || len(program) > filemodel.MaxPath || strings.ContainsRune(program, 0) {
		return errors.New("action must be LABEL=EXECUTABLE (label 1..32 bytes), without arguments")
	}
	for _, old := range *a {
		if old.Label == label {
			return errors.New("duplicate action label")
		}
	}
	*a = append(*a, configuredAction{label, program})
	return nil
}

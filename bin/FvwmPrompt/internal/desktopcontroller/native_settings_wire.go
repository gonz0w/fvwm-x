// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"encoding/hex"
	"errors"
	"fmt"
	"strconv"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

const nativeFrameLimit = 2048

type nativeDesired struct {
	motion             bool
	cursorSize         int
	font, icon, cursor string
}

func nativeTuple(v values) nativeDesired {
	return nativeDesired{v["motion"] == true, v["cursor_size"].(int),
		v["application_font"].(string), v["icon_theme"].(string), v["cursor_theme"].(string)}
}

func nativeHex(s string) string {
	if s == "" {
		return "-"
	}
	return hex.EncodeToString([]byte(s))
}

func nativeApply(desktop, run string, seq uint64, v nativeDesired) (string, error) {
	if !identity(desktop, 32) || !identity(run, 32) || seq == 0 {
		return "", errors.New("Invalid native settings identity")
	}
	for key, value := range (values{"motion": v.motion, "cursor_size": v.cursorSize,
		"application_font": v.font, "icon_theme": v.icon, "cursor_theme": v.cursor}) {
		if _, err := desktopsettings.Value(key, value, false); err != nil {
			return "", err
		}
	}
	motion := 0
	if v.motion {
		motion = 1
	}
	line := fmt.Sprintf("FNS1 APPLY %s %s %d %d %d %s %s %s\n", desktop, run, seq,
		motion, v.cursorSize, nativeHex(v.font), nativeHex(v.icon), nativeHex(v.cursor))
	if len(line) > nativeFrameLimit {
		return "", errors.New("Native settings frame too large")
	}
	return line, nil
}

type nativeResult struct {
	desktop, run             string
	seq                      uint64
	xstate, resource, reason string
	xserial                  uint32
}

func nativeUnsigned(s string, bits int) (uint64, error) {
	if len(s) == 0 {
		return 0, errors.New("Invalid native decimal")
	}
	for _, c := range s {
		if c < '0' || c > '9' {
			return 0, errors.New("Invalid native decimal")
		}
	}
	return strconv.ParseUint(s, 10, bits)
}

func nativeResultLine(line string) (nativeResult, error) {
	var r nativeResult
	bad := errors.New("Invalid native settings result")
	if len(line) > nativeFrameLimit || !strings.HasSuffix(line, "\n") {
		return r, bad
	}
	// Literal ASCII spaces and exactly one trailing newline: tabs, CR, extra
	// whitespace and partial EOF never become an actionable result.
	words := strings.Split(strings.TrimSuffix(line, "\n"), " ")
	if len(words) != 9 || words[0] != "FNS1" || words[1] != "RESULT" ||
		!identity(words[2], 32) || !identity(words[3], 32) {
		return r, bad
	}
	seq, err := nativeUnsigned(words[4], 64)
	if err != nil || seq == 0 {
		return r, bad
	}
	serial, err := nativeUnsigned(words[7], 32)
	if err != nil {
		return r, bad
	}
	if !nativeToken(words[5], "ready", "conflict", "lost", "failed", "off") ||
		!nativeToken(words[6], "applied", "inherited", "conflict", "unavailable") ||
		!nativeToken(words[8], "none", "owner-conflict", "invalid-input", "stale-input", "x-error",
			"resource-conflict", "resource-limit", "parent-eof") {
		return r, bad
	}
	return nativeResult{words[2], words[3], seq, words[5], words[6], words[8], uint32(serial)}, nil
}

func nativeToken(value string, allowed ...string) bool {
	for _, token := range allowed {
		if value == token {
			return true
		}
	}
	return false
}

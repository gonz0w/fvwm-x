// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"errors"
	"strings"
)

type ExecContext struct {
	Name, Icon, Location string
	Files, URLs          []string
}

type execToken struct {
	text   string
	quoted bool
}

// execTokens implements Desktop Entry quoting, not shell syntax. Values must
// already have undergone the desktop string escape pass.
func execTokens(value string) ([]execToken, error) {
	var tokens []execToken
	for i := 0; i < len(value); {
		if value[i] == ' ' {
			i++
			continue
		}
		token := execToken{quoted: value[i] == '"'}
		var word strings.Builder
		closed := !token.quoted
		if token.quoted {
			i++
		}
		for i < len(value) {
			c := value[i]
			if token.quoted && c == '"' {
				i++
				closed = true
				break
			}
			if !token.quoted && c == ' ' {
				break
			}
			if token.quoted && c == '\\' {
				i++
				if i == len(value) || !strings.ContainsRune("\"`$\\", rune(value[i])) {
					return nil, errors.New("invalid quoted Exec escape")
				}
				c = value[i]
			} else if token.quoted && strings.ContainsRune("`$", rune(c)) {
				return nil, errors.New("unescaped reserved character in quoted Exec argument")
			} else if !token.quoted && strings.ContainsRune("\t\n\r\"'\\><~|&;$*?#()`", rune(c)) {
				return nil, errors.New("unquoted reserved character in Exec argument")
			}
			if c == 0 || c == 127 || c < 32 && c != '\t' && c != '\n' && c != '\r' {
				return nil, errors.New("control character in Exec argument")
			}
			word.WriteByte(c)
			i++
		}
		if !closed || i < len(value) && value[i] != ' ' {
			return nil, errors.New("incomplete or partial Exec quoting")
		}
		token.text = word.String()
		tokens = append(tokens, token)
	}
	return tokens, nil
}

// ExpandExec produces literal argv. It performs no shell evaluation, process
// launch, implicit URI download or focus/startup-notification action.
func ExpandExec(raw string, context ExecContext) ([]string, error) {
	if len(raw) > MaxValueBytes {
		return nil, errors.New("Exec source exceeds limit")
	}
	for _, c := range raw {
		if c > 127 {
			return nil, errors.New("Exec source must be ASCII")
		}
	}
	decoded, err := decodeValue(raw, false)
	if err != nil {
		return nil, err
	}
	tokens, err := execTokens(decoded[0])
	if err != nil || len(tokens) == 0 || tokens[0].text == "" {
		return nil, errors.New("invalid Exec command line")
	}
	if len(context.Files) > MaxItems || len(context.URLs) > MaxItems {
		return nil, errors.New("too many Exec replacements")
	}
	for _, values := range [][]string{{context.Name, context.Icon, context.Location}, context.Files, context.URLs} {
		for _, value := range values {
			if len(value) > MaxValueBytes || strings.IndexByte(value, 0) >= 0 {
				return nil, errors.New("Exec replacement exceeds limits or contains NUL")
			}
		}
	}
	var argv []string
	fileCodes, total := 0, 0
	add := func(value string) error {
		if len(value) > MaxValueBytes || len(argv) >= MaxItems || total > MaxSourceBytes-len(value) {
			return errors.New("expanded Exec exceeds limits")
		}
		total += len(value)
		argv = append(argv, value)
		return nil
	}
	for tokenIndex, token := range tokens {
		var word strings.Builder
		replaced := false
		keepEmpty := false
		var multiple []string
		for i := 0; i < len(token.text); i++ {
			c := token.text[i]
			if c != '%' {
				word.WriteByte(c)
				continue
			}
			i++
			if i == len(token.text) {
				return nil, errors.New("incomplete Exec field code")
			}
			code := token.text[i]
			if code != '%' && (token.quoted || tokenIndex == 0) {
				return nil, errors.New("Exec field code in quoted argument or executable")
			}
			if strings.ContainsRune("fFuU", rune(code)) {
				fileCodes++
				if fileCodes > 1 {
					return nil, errors.New("multiple Exec file field codes")
				}
			}
			if strings.ContainsRune("FUi", rune(code)) && token.text != "%"+string(code) {
				return nil, errors.New("multi-argument Exec field code is not standalone")
			}
			switch code {
			case '%':
				word.WriteByte('%')
			case 'f', 'u':
				values := context.Files
				if code == 'u' {
					values = context.URLs
				}
				if len(values) != 0 {
					word.WriteString(values[0])
				}
				replaced = true
			case 'F':
				multiple, replaced = context.Files, true
			case 'U':
				multiple, replaced = context.URLs, true
			case 'i':
				if context.Icon != "" {
					multiple = []string{"--icon", context.Icon}
				}
				replaced = true
			case 'c':
				word.WriteString(context.Name)
				replaced, keepEmpty = true, true
			case 'k':
				word.WriteString(context.Location)
				replaced, keepEmpty = true, true
			case 'd', 'D', 'n', 'N', 'v', 'm':
				replaced = true
			default:
				return nil, errors.New("unknown Exec field code")
			}
			if word.Len() > MaxValueBytes {
				return nil, errors.New("Exec argument exceeds limits")
			}
		}
		if multiple != nil {
			for _, value := range multiple {
				if err := add(value); err != nil {
					return nil, err
				}
			}
		} else if word.Len() != 0 || !replaced || keepEmpty {
			if err := add(word.String()); err != nil {
				return nil, err
			}
		}
	}
	if len(argv) == 0 || argv[0] == "" || strings.Contains(argv[0], "=") {
		return nil, errors.New("invalid Exec executable")
	}
	return argv, nil
}

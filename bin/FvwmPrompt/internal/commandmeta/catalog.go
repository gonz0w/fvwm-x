// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package commandmeta shares generated core names and registered setting choices.
// Completion is a literal supported subset, not a configuration parser.
package commandmeta

import (
	_ "embed"
	"encoding/json"
	"sort"
	"strings"
	"unicode"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

//go:embed commands.json
var catalogJSON []byte

type Command struct {
	Name        string `json:"name"`
	Description string `json:"descr"`
}

// Commands returns operation-owned metadata. An invalid built-in catalog is
// unavailable; the generation/parity contract rejects it before publication.
func Commands() []Command {
	var commands []Command
	if json.Unmarshal(catalogJSON, &commands) != nil {
		return nil
	}
	return commands
}

// Complete returns suffixes and the current word length for readline. It does
// not evaluate expansion, parse arbitrary arguments, inspect history or execute.
func Complete(line string, pos int) ([][]rune, int) {
	runes := []rune(line)
	if pos < 0 || pos > len(runes) || len(runes) > 4096 {
		return nil, 0
	}
	prefix := string(runes[:pos])
	if strings.ContainsAny(prefix, "\x00\r\n\"'`$;\\") {
		return nil, 0
	}
	words := strings.Fields(prefix)
	if len(words) == 0 || pos > 0 && unicode.IsSpace(runes[pos-1]) {
		words = append(words, "")
	}
	word := words[len(words)-1]
	var candidates []string
	if len(words) == 1 {
		for _, command := range Commands() {
			candidates = append(candidates, command.Name)
		}
	} else if strings.EqualFold(words[0], "SendToModule") {
		switch len(words) {
		case 2:
			candidates = []string{"FvwmDesktop"}
		case 3:
			if words[1] == "FvwmDesktop" {
				candidates = []string{"set", "toggle", "reset"}
			}
		case 4:
			if words[1] == "FvwmDesktop" {
				for key, setting := range desktopsettings.Registry() {
					if words[2] == "set" || words[2] == "reset" ||
						words[2] == "toggle" && setting.Type == "boolean" {
						candidates = append(candidates, key)
					}
					if words[2] == "reset" {
						candidates = append(candidates, "group:"+setting.Group)
					}
				}
				if words[2] == "reset" {
					candidates = append(candidates, "all")
				}
			}
		case 5:
			if words[1] == "FvwmDesktop" && words[2] == "set" {
				if setting, ok := desktopsettings.Registry()[words[3]]; ok {
					candidates = append(candidates, setting.Choices...)
					if setting.Type == "boolean" {
						candidates = []string{"on", "off"}
					}
				}
			}
		}
	}
	sort.Strings(candidates)
	seen := map[string]bool{}
	var result [][]rune
	for _, candidate := range candidates {
		if !seen[candidate] && strings.HasPrefix(strings.ToLower(candidate), strings.ToLower(word)) {
			seen[candidate] = true
			result = append(result, []rune(string([]rune(candidate)[len([]rune(word)):])+" "))
		}
	}
	return result, len([]rune(word))
}

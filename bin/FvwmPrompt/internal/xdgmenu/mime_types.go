// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"errors"
	"sort"
	"strings"
)

const MaxMIMETypeBytes = 255
const MaxMIMETypeSourceBytes = 1024 * 1024
const MaxMIMETypeAliases = 4096
const MaxMIMETypeEdges = 8192
const MaxMIMETypeReachable = 128
const MaxMIMETypeDepth = 32

// NormalizeMIMEType accepts one concrete ASCII media type, without parameters,
// whitespace or wildcards. RFC 6838 section 4.2 bounds each restricted name.
func NormalizeMIMEType(value string) (string, error) {
	if len(value) > MaxMIMETypeBytes {
		return "", errors.New("MIME type exceeds length limit")
	}
	top, sub, found := strings.Cut(value, "/")
	for _, part := range []string{top, sub} {
		if !found || len(part) == 0 || len(part) > 127 {
			return "", errors.New("invalid MIME type")
		}
		for i, c := range []byte(part) {
			alnum := c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9'
			if !alnum && (i == 0 || !strings.ContainsRune("!#$&-^_.+", rune(c))) {
				return "", errors.New("invalid MIME type character")
			}
		}
	}
	return strings.ToLower(value), nil
}

// MIMETypeSource is one data root's installed text metadata. Sources are in XDG
// precedence order. Aliases use the first owner; subclass edges are combined.
type MIMETypeSource struct {
	Aliases, Subclasses []byte
}

// MIMETypeGraph contains normalized effective aliases and an acyclic parent
// graph. It performs no filename matching, sniffing or database generation.
type MIMETypeGraph struct {
	aliases map[string]string
	parents map[string][]string
}

func mimeTypeRows(data []byte, visit func(string, string) error) error {
	if len(data) > MaxMIMETypeSourceBytes {
		return errors.New("MIME metadata exceeds source limit")
	}
	for _, c := range data {
		if c >= 127 || c < 32 && c != '\n' && c != '\r' && c != '\t' {
			return errors.New("invalid MIME metadata text")
		}
	}
	for remaining := string(data); remaining != ""; {
		line, rest, _ := strings.Cut(remaining, "\n")
		remaining = rest
		line = strings.Trim(line, " \t\r")
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		fields := strings.Fields(line)
		if len(fields) != 2 {
			return errors.New("invalid MIME metadata row")
		}
		child, err := NormalizeMIMEType(fields[0])
		if err != nil {
			return err
		}
		parent, err := NormalizeMIMEType(fields[1])
		if err != nil {
			return err
		}
		if err := visit(child, parent); err != nil {
			return err
		}
	}
	return nil
}

func NewMIMETypeGraph(sources []MIMETypeSource) (MIMETypeGraph, error) {
	graph := MIMETypeGraph{aliases: make(map[string]string), parents: make(map[string][]string)}
	fail := func(err error) (MIMETypeGraph, error) { return MIMETypeGraph{}, err }
	if len(sources) > MaxDirectoryRoleRoots {
		return fail(errors.New("too many MIME metadata roots"))
	}
	remaining := MaxDirectoryRolePolicyBytes
	for _, source := range sources {
		for _, data := range [][]byte{source.Aliases, source.Subclasses} {
			if len(data) > MaxMIMETypeSourceBytes || len(data) > remaining {
				return fail(errors.New("MIME metadata exceeds byte limits"))
			}
			remaining -= len(data)
		}
	}
	aliases, edges := 0, 0
	for _, source := range sources {
		local := make(map[string]string)
		err := mimeTypeRows(source.Aliases, func(alias, canonical string) error {
			aliases++
			if aliases > MaxMIMETypeAliases {
				return errors.New("too many MIME aliases")
			}
			if old, ok := local[alias]; ok && old != canonical {
				return errors.New("conflicting MIME aliases in one source")
			}
			local[alias] = canonical
			if _, ok := graph.aliases[alias]; !ok {
				graph.aliases[alias] = canonical
			}
			return nil
		})
		if err != nil {
			return fail(err)
		}
	}
	// Resolve every effective alias, including unreachable ones, before using
	// the graph. A malformed installed graph cannot yield a partial answer.
	resolved := make(map[string]string)
	for alias := range graph.aliases {
		seen := make(map[string]bool)
		value := alias
		for {
			if seen[value] || len(seen) >= MaxMIMETypeDepth {
				return fail(errors.New("cyclic or over-depth MIME aliases"))
			}
			seen[value] = true
			next, ok := graph.aliases[value]
			if !ok {
				break
			}
			value = next
		}
		resolved[alias] = value
	}
	graph.aliases = resolved
	nodes := make(map[string]bool)
	for _, value := range resolved {
		nodes[value] = true
	}
	for _, source := range sources {
		err := mimeTypeRows(source.Subclasses, func(child, parent string) error {
			edges++
			if edges > MaxMIMETypeEdges {
				return errors.New("too many MIME subclass edges")
			}
			child, _ = graph.Canonical(child)
			parent, _ = graph.Canonical(parent)
			if child == parent {
				return errors.New("self-referential MIME subclass")
			}
			graph.parents[child] = append(graph.parents[child], parent)
			nodes[child], nodes[parent] = true, true
			return nil
		})
		if err != nil {
			return fail(err)
		}
	}
	// Shared MIME-info section 2.11 supplies these implicit relationships.
	// Their targets are also canonicalized; inode types have no binary parent.
	for _, value := range []string{"text/plain", "application/octet-stream"} {
		canonical, _ := graph.Canonical(value)
		nodes[canonical] = true
	}
	for node := range nodes {
		graph.parents[node] = graph.parentTypes(node)
	}
	state, depths := make(map[string]uint8), make(map[string]int)
	var check func(string, int) (int, error)
	check = func(node string, depth int) (int, error) {
		if depth >= MaxMIMETypeDepth || state[node] == 1 {
			return 0, errors.New("cyclic or over-depth MIME subclasses")
		}
		if state[node] == 2 {
			if depth+depths[node] > MaxMIMETypeDepth {
				return 0, errors.New("over-depth MIME subclasses")
			}
			return depths[node], nil
		}
		state[node] = 1
		longest := 1
		for _, parent := range graph.parents[node] {
			d, err := check(parent, depth+1)
			if err != nil {
				return 0, err
			}
			longest = max(longest, d+1)
		}
		state[node], depths[node] = 2, longest
		return longest, nil
	}
	for node := range nodes {
		if _, err := check(node, 0); err != nil {
			return fail(err)
		}
	}
	return graph, nil
}

func (g MIMETypeGraph) Canonical(mime string) (string, error) {
	value, err := NormalizeMIMEType(mime)
	if err != nil {
		return "", err
	}
	if canonical, ok := g.aliases[value]; ok {
		value = canonical
	}
	return value, nil
}

func (g MIMETypeGraph) parentTypes(mime string) []string {
	result := append([]string(nil), g.parents[mime]...)
	if strings.HasPrefix(mime, "text/") {
		parent, _ := g.Canonical("text/plain")
		if parent != mime {
			result = append(result, parent)
		}
	}
	if !strings.HasPrefix(mime, "inode/") {
		parent, _ := g.Canonical("application/octet-stream")
		if parent != mime {
			result = append(result, parent)
		}
	}
	sort.Strings(result)
	unique := result[:0]
	for _, value := range result {
		if len(unique) == 0 || unique[len(unique)-1] != value {
			unique = append(unique, value)
		}
	}
	return unique
}

// Types returns subtype-before-all-ancestors order. Lexical ordering of
// incomparable types is an explicit local policy, independent of map/row order.
func (g MIMETypeGraph) Types(mime string) ([]string, error) {
	canonical, err := g.Canonical(mime)
	if err != nil {
		return nil, err
	}
	parents := make(map[string][]string)
	depths := make(map[string]int)
	var collect func(string, int) error
	collect = func(node string, depth int) error {
		if depth >= MaxMIMETypeDepth {
			return errors.New("MIME hierarchy exceeds depth limit")
		}
		if prior, ok := depths[node]; ok && prior >= depth {
			return nil
		}
		depths[node] = depth
		if _, ok := parents[node]; !ok {
			if len(parents) >= MaxMIMETypeReachable {
				return errors.New("too many reachable MIME types")
			}
			parents[node] = g.parentTypes(node)
		}
		for _, parent := range parents[node] {
			if err := collect(parent, depth+1); err != nil {
				return err
			}
		}
		return nil
	}
	if err := collect(canonical, 0); err != nil {
		return nil, err
	}
	children := make(map[string]int)
	for _, values := range parents {
		for _, parent := range values {
			children[parent]++
		}
	}
	var ready, result []string
	for node := range parents {
		if children[node] == 0 {
			ready = append(ready, node)
		}
	}
	for len(ready) != 0 {
		sort.Strings(ready)
		node := ready[0]
		ready = ready[1:]
		result = append(result, node)
		for _, parent := range parents[node] {
			children[parent]--
			if children[parent] == 0 {
				ready = append(ready, parent)
			}
		}
	}
	if len(result) != len(parents) {
		return nil, errors.New("cyclic MIME hierarchy")
	}
	return result, nil
}

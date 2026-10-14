// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"bytes"
	"encoding/xml"
	"errors"
	"fmt"
	"io"
	"strings"
)

// Node preserves element order, layout attributes and unknown extension
// elements for the later merger. The caller owns the returned tree.
type Node struct {
	Tag      string
	Text     string
	Attr     map[string]string
	Children []*Node
}

type xmlFrame struct {
	node *Node
	text strings.Builder
}

// ParseMenuXML decodes only the supplied bytes. The standard decoder neither
// fetches external DTDs nor expands declared external entities. A conventional
// menu DOCTYPE is accepted as inert metadata; unknown entities are refused.
func ParseMenuXML(data []byte) (*Node, error) {
	if len(data) > MaxSourceBytes {
		return nil, errors.New("menu XML exceeds source limit")
	}
	decoder := xml.NewDecoder(bytes.NewReader(data))
	var root *Node
	var stack []*xmlFrame
	nodes := 0
	for {
		token, err := decoder.Token()
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, fmt.Errorf("invalid menu XML: %w", err)
		}
		switch value := token.(type) {
		case xml.StartElement:
			nodes++
			if nodes > MaxItems || len(stack) >= MaxDepth || value.Name.Space != "" || len(value.Name.Local) > MaxValueBytes {
				return nil, errors.New("menu XML exceeds node/depth limits or uses a namespace")
			}
			node := &Node{Tag: value.Name.Local, Attr: make(map[string]string)}
			if len(value.Attr) > MaxItems {
				return nil, errors.New("menu XML exceeds attribute limit")
			}
			for _, attr := range value.Attr {
				if len(attr.Name.Local) > MaxValueBytes || len(attr.Value) > MaxValueBytes {
					return nil, errors.New("menu XML attribute exceeds value limit")
				}
				if _, exists := node.Attr[attr.Name.Local]; exists || attr.Name.Space != "" {
					return nil, errors.New("duplicate or namespaced menu XML attribute")
				}
				node.Attr[attr.Name.Local] = attr.Value
			}
			if len(stack) == 0 {
				if root != nil || node.Tag != "Menu" {
					return nil, errors.New("menu XML requires one Menu root")
				}
				root = node
			} else {
				parent := stack[len(stack)-1].node
				parent.Children = append(parent.Children, node)
			}
			stack = append(stack, &xmlFrame{node: node})
		case xml.EndElement:
			frame := stack[len(stack)-1]
			frame.node.Text = frame.text.String()
			stack = stack[:len(stack)-1]
		case xml.CharData:
			if len(stack) == 0 {
				if strings.Trim(string(value), " \t\r\n") != "" {
					return nil, errors.New("text outside menu XML root")
				}
			} else {
				frame := stack[len(stack)-1]
				if frame.text.Len() > MaxValueBytes-len(value) {
					return nil, errors.New("menu XML value exceeds limit")
				}
				frame.text.Write(value)
			}
		}
	}
	if root == nil || len(stack) != 0 {
		return nil, errors.New("missing or incomplete menu XML root")
	}
	return root, nil
}

// MatchRule evaluates the Menu specification's boolean inclusion language.
// A separate caller processes ordered Include/Exclude operations and passes
// the desktop-file identity relative to the owning AppDir.
func MatchRule(rule *Node, desktopID string, categories []string) (bool, error) {
	if len(desktopID) > MaxValueBytes || len(categories) > MaxItems {
		return false, errors.New("menu rule inputs exceed limits")
	}
	remaining := MaxItems
	return matchRule(rule, desktopID, categories, 0, &remaining)
}

func matchRule(rule *Node, id string, categories []string, depth int, remaining *int) (bool, error) {
	if rule == nil || depth >= MaxDepth || *remaining <= 0 {
		return false, errors.New("invalid or excessive menu rule tree")
	}
	*remaining -= 1
	if (rule.Tag == "All" || rule.Tag == "Filename" || rule.Tag == "Category") && len(rule.Children) != 0 {
		return false, errors.New("menu rule leaf has child elements")
	}
	text := strings.Trim(rule.Text, " \t\r\n")
	if len(rule.Text) > MaxValueBytes || (rule.Tag == "Filename" || rule.Tag == "Category") && text == "" {
		return false, errors.New("menu rule leaf has no value")
	}
	switch rule.Tag {
	case "All":
		return true, nil
	case "Filename":
		return text == id, nil
	case "Category":
		for _, category := range categories {
			if *remaining <= 0 {
				return false, errors.New("menu rule evaluation exceeds work limit")
			}
			*remaining -= 1
			if text == category {
				return true, nil
			}
		}
		return false, nil
	case "Include", "Exclude", "Or", "Not", "And":
		matched := rule.Tag == "And"
		for _, child := range rule.Children {
			value, err := matchRule(child, id, categories, depth+1, remaining)
			if err != nil {
				return false, err
			}
			if rule.Tag == "And" {
				matched = matched && value
			} else {
				matched = matched || value
			}
		}
		if rule.Tag == "Not" {
			matched = !matched
		}
		return matched, nil
	default:
		return false, fmt.Errorf("unsupported menu rule %s", rule.Tag)
	}
}

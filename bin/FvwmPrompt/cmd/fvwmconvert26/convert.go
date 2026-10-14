// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"errors"
	"fmt"
	"io"
	"regexp"
	"strings"
)

// These expressions deliberately retain the legacy heuristic's dispatch
// order and byte-oriented whitespace. This is not an FVWM command parser.
var rules = []struct {
	pattern *regexp.Regexp
	kind    string
}{
	{rx(`^style`), "style"},
	{rx(`^\s*\*fvwmtheme:?`), "theme"},
	{rx(`^\s*modulepath\s*`), "modulepath"},
	{rx(`^\s*windowshade(?:steps|animate).*`), "shade"},
	{rx(`^\s*windowfont.*`), "font"},
	{rx(`^\s*iconfont.*`), "iconfont"},
	{rx(`^\s*module(?:synchronous)?.*?fvwmtheme$`), "theme"},
	{rx(`^\s*edgeresistance\s*\d+\s*\d+`), "edge"},
	{rx(`^\s*key|mouse`), "binding"},
	{rx(`^\s*snap(?:attraction|grid)`), "snap"},
	{rx(`^\s*addtofunc\s+initfunction`), "init"},
	{rx(`^\s*addtofunc\s+startfunction.*`), "start"},
	{rx(`^\s*addtofunc\s+restartfunction`), "restart"},
	{rx(`^\s*addtofunc\s+\w+.*`), "function"},
	{rx(`^\s*pixmappath|iconpath\s+\w+.*`), "path"},
	{rx(`^\s*colorlimit|(?:gnome(?:button|showdesks))\s+\w+.*`), "obsolete"},
	{rx(`^\s*desk\s*\w+.*`), "desk"},
	{rx(`^\s*hilightcolorset\s*`), "hilight"},
	{rx(`^\s*hidegeometrywindow\s*`), "geometry"},
	{rx(`^\s*\+\s*"?[ichmd]\s*"?\s+.*`), "continuation"},
	{rx(`^\s*read\s*[/\w]+`), "read"},
}

func rx(pattern string) *regexp.Regexp {
	// Perl's dollar anchor also matches before one terminal newline.
	if strings.HasSuffix(pattern, "$") {
		pattern = strings.TrimSuffix(pattern, "$") + `(?:\n)?$`
	}
	return regexp.MustCompile("(?i)" + pattern)
}

var (
	spaces       = regexp.MustCompile(`\s+`)
	spaceCapture = regexp.MustCompile(`(\s+)`)
	styleParts   = rx(`^(style\s+"??[\w+*?]"??)(.*)$`)
	styleComma   = regexp.MustCompile(`\s*,\s*`)
	negated      = regexp.MustCompile(`No(.*)`)
	pPosition    = rx(`nopposition`)
	digits       = regexp.MustCompile(`(\d+)p?`)
	edgeArgs     = rx(`edgeresistance\s*(\d+)\s*(\d+)`)
	plusPrefix   = rx(`^\s*\+`)
	plusAction   = rx(`(\+\s*"?(?:i|c|d|h|m)"?\s*)`)
	// The legacy embedded qr expression is case-sensitive even within /i.
	conditionOld    = regexp.MustCompile(`(all|current|direction|next|none|prev|pick|thiswindow|windowid)\s*\[(.*?)\]`)
	conditionNew    = regexp.MustCompile(`(all|current|direction|next|none|prev|pick|thiswindow|windowid)\s*(\(.*?\))(.*)`)
	conditionSep    = regexp.MustCompile(`[^,](\s+)[^,]`)
	functionHead    = rx(`^addtofunc\s+(?:start|init|restart)function.*`)
	functionInit    = rx(`addtofunc\s+initfunction\s*`)
	functionStart   = rx(`addtofunc\s+startfunction\s*`)
	functionRestart = rx(`addtofunc\s+restartfunction\s*`)
	themePrefix     = rx(`^\*fvwmtheme\s*:?`)
	themeModule     = rx(`modulesynchronous.*?fvwmtheme`)
	pathPrefix      = rx(`^\s*(?:pixmappath|iconpath)`)
	deskPrefix      = rx(`^\s*desk\s+`)
	windowFont      = rx(`windowfont`)
	iconFont        = rx(`iconfont`)
	hilight         = rx(`hilightcolorset`)
	geometry        = rx(`hidegeometrywindow`)
)

func replaceFirst(r *regexp.Regexp, text, replacement string) string {
	where := r.FindStringIndex(text)
	if where == nil {
		return text
	}
	return text[:where[0]] + replacement + text[where[1]:]
}

// Perl's split limit counts fields, excluding captured separators. Preserve
// the initial empty field and drop trailing empty fields for unlimited splits.
func splitFields(r *regexp.Regexp, text string, limit int) []string {
	if text == "" {
		return nil
	}
	parts := []string{}
	position, fields := 0, 0
	matches := -1
	if limit > 0 {
		matches = limit - 1
	}
	for _, match := range r.FindAllStringSubmatchIndex(text, matches) {
		if limit > 0 && fields == limit-1 {
			break
		}
		parts = append(parts, text[position:match[0]])
		fields++
		for i := 2; i < len(match); i += 2 {
			if match[i] >= 0 {
				parts = append(parts, text[match[i]:match[i+1]])
			} else {
				parts = append(parts, "")
			}
		}
		position = match[1]
	}
	parts = append(parts, text[position:])
	if limit == 0 {
		for len(parts) > 0 && parts[len(parts)-1] == "" {
			parts = parts[:len(parts)-1]
		}
	}
	return parts
}

func conditional(line string) string {
	line = strings.Replace(line, "[*]", "", 1)
	if match := conditionOld.FindStringSubmatchIndex(line); match != nil {
		line = line[:match[0]] + "(" + line[match[2]:match[3]] + ")" + line[match[1]:]
	}
	if match := conditionNew.FindStringSubmatch(line); match != nil {
		line = match[1] + " " + strings.Join(splitFields(conditionSep, match[2], 0), ", ") + " " + match[3]
	}
	return line
}

func conditionalParts(parts []string) string {
	if len(parts) != 0 {
		parts[len(parts)-1] = conditional(parts[len(parts)-1])
	}
	return strings.Join(parts, "")
}

type converter struct {
	text      strings.Builder
	init      strings.Builder
	start     strings.Builder
	lineCount int
	hasInit   bool
	hasStart  bool
	context   string
	reads     []readReference
	err       error
}

type readReference struct {
	name string
	line string
}

func (c *converter) function(line, kind string) {
	c.context = kind
	head := functionStart
	if kind == "init" {
		head = functionInit
	} else if kind == "restart" {
		head = functionRestart
	}
	line = replaceFirst(head, line, "")
	if kind == "restart" && line == "" {
		return
	}
	line = replaceFirst(plusPrefix, line, "")
	if kind != "restart" && line == "" {
		return
	}
	limit := 2
	if kind == "init" {
		limit = 3
	}
	parts := splitFields(spaces, line, limit)
	if kind == "init" && len(parts) <= 2 {
		parts = append([]string{""}, parts...)
	}
	for len(parts) < 2 {
		parts = append(parts, "")
	}
	parts[1] = strings.ReplaceAll(parts[1], `"`, "")
	if kind == "init" {
		parts[1] += " Test (Init) "
	}
	parts[len(parts)-1] = conditional(parts[len(parts)-1])
	text := strings.Join(parts, " ")
	if kind == "init" {
		c.init.WriteString("+ " + text + "\n")
		c.hasInit = true
	} else {
		c.start.WriteString("+ " + text + "\n")
		c.hasStart = true
	}
}

func (c *converter) dispatch(line string) {
	kind := ""
	for _, rule := range rules {
		if rule.pattern.MatchString(line) {
			kind = rule.kind
			break
		}
	}
	switch kind {
	case "style":
		match := styleParts.FindStringSubmatch(line)
		line = ""
		if match != nil {
			parts := splitFields(styleComma, match[2], 0)
			for i, part := range parts {
				if !pPosition.MatchString(part) {
					if m := negated.FindStringSubmatch(part); m != nil {
						part = replaceFirst(negated, part, "!"+m[1])
					}
				}
				parts[i] = replaceFirst(pPosition, part, "!UsePPosition")
			}
			line = match[1] + strings.Join(parts, ", ")
		}
	case "theme":
		line = replaceFirst(themePrefix, line, "")
		if themeModule.MatchString(line) {
			line = ""
		}
	case "modulepath":
		line = "# Commented out by fvwm-convert-2.6:  " + line
	case "shade":
		if m := digits.FindStringSubmatch(line); m != nil {
			line = "Style * WindowShadeSteps " + m[1]
		}
	case "font":
		line = replaceFirst(windowFont, line, "Style * Font")
	case "iconfont":
		line = replaceFirst(iconFont, line, "Style * IconFont")
	case "edge":
		m := edgeArgs.FindStringSubmatch(line)
		line = fmt.Sprintf("\nEdgeResistance %s\nStyle * EdgeMoveResistance %s", m[1], m[2])
	case "binding":
		line = conditionalParts(splitFields(spaceCapture, line, 5))
	case "snap":
		line = "Style * " + line
	case "init", "start", "restart":
		c.function(line, kind)
		return
	case "function":
		if !functionHead.MatchString(line) {
			c.context = ""
		}
		line = conditionalParts(splitFields(spaceCapture, line, 4))
	case "path":
		line = replaceFirst(pathPrefix, line, "ImagePath")
	case "obsolete":
		line = "# Obsolete, does nothing: " + line
	case "desk":
		line = replaceFirst(deskPrefix, line, "GotoDesk")
	case "hilight":
		line = replaceFirst(hilight, line, "Style * HilightColorset")
	case "geometry":
		line = replaceFirst(geometry, line, "GeometryWindow hide")
	case "continuation":
		if c.context != "" {
			c.function(line, c.context)
			return
		}
		line = conditionalParts(splitFields(plusAction, line, 2))
	case "read":
		parts := splitFields(spaces, line, 3)
		if len(parts) > 1 && parts[1] != "" {
			if len(c.reads) >= readLimit {
				c.err = errors.New("Read references exceed 4096")
				return
			}
			c.reads = append(c.reads, readReference{parts[1], line})
		}
	}
	parts := strings.Split(line, `\`)
	for len(parts) > 0 && parts[len(parts)-1] == "" {
		parts = parts[:len(parts)-1]
	}
	if c.lineCount != 0 {
		c.text.WriteByte('\n')
	}
	c.text.WriteString(strings.Join(parts, "\\\n"))
	c.lineCount++
}

func (c *converter) writeOutput(out io.Writer) error {
	if _, err := io.WriteString(out, c.text.String()); err != nil {
		return err
	}
	if c.hasInit || c.hasStart {
		for _, block := range []string{"\n\nDestroyFunc StartFunction\nAddToFunc StartFunction\n", c.init.String(), c.start.String()} {
			if _, err := io.WriteString(out, block); err != nil {
				return err
			}
		}
	}
	return nil
}

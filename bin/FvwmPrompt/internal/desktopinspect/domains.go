// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopinspect

import (
	"encoding/json"
	"errors"
	"fmt"
	"sort"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

func (c *collector) theme(s *Section) {
	s.State = "observed"
	s.Generation = "multiple-domains"
	s.Reason = "Published core definitions plus provider policy; final paint unknown; domains are not an atomic snapshot"
	contrast := ""
	var settings map[string]map[string]json.RawMessage
	if json.Unmarshal(c.req.Settings, &settings) == nil {
		contrast = stringField(settings["contrast"], "effective")
	}
	projection, err := desktopsettings.ProjectTheme(c.req.Colors, c.req.Appearance, contrast)
	if err != nil {
		c.problem(s, "Provider color-role projection unavailable")
	} else {
		for _, role := range projection.Roles {
			c.row(s, "Policy role "+role.Name, fmt.Sprintf("colorset %d; fg=%s; bg=%s; appearance=%s", role.Colorset, role.Foreground, role.Background, c.req.Appearance))
		}
	}
	appearanceGeneration := ""
	appearance, err := c.call("appearance", "", 0, "", false)
	if err != nil {
		c.problem(s, "appearance: "+err.Error())
	} else {
		appearanceGeneration = stringField(appearance, "generation")
		windowEffective, windowOK := boolField(appearance, "window_effective_state")
		if !windowOK || windowEffective || stringField(appearance, "rendered_completion") != "unknown" {
			c.problem(s, "Appearance completion metadata unavailable")
		}
		if !token(appearanceGeneration) {
			c.problem(s, "Invalid appearance generation")
			appearanceGeneration = ""
		} else {
			c.row(s, "Appearance generation", appearanceGeneration)
		}
	}
	colorsGeneration := ""
	count := 0
	c.pages(s, "colorsets", "", "colorsets", &colorsGeneration, func(item map[string]json.RawMessage) error {
		if count >= 32 {
			s.Truncated = true
			return errors.New("colorset row budget reached")
		}
		count++
		id, ok := intField(item, "id")
		state := stringField(item, "update_state")
		pixels, pok := objectField(item, "pixels")
		background, bok := objectField(item, "background")
		if !ok || id < 0 || !pok || !bok || !member(state, []string{"pending", "refused", "idle"}) {
			return errors.New("invalid colorset definition")
		}
		fg, bg := stringField(pixels, "foreground"), stringField(pixels, "background")
		if len(fg) != 16 || len(bg) != 16 || strings.Trim(fg+bg, "0123456789abcdefABCDEF") != "" {
			return errors.New("invalid published pixel")
		}
		transparency := stringField(background, "transparency")
		if !member(transparency, []string{"none", "parent", "root"}) {
			return errors.New("invalid colorset background")
		}
		c.row(s, fmt.Sprintf("Observed colorset %d", id), fmt.Sprintf("%s; fg pixel=%s; bg pixel=%s; background=%s", state, fg, bg, transparency))
		return nil
	})
	if colorsGeneration != "" {
		c.row(s, "Colorset generation", colorsGeneration)
	}
	count = 0
	c.pages(s, "menu-styles", "", "menu_styles", &appearanceGeneration, func(item map[string]json.RawMessage) error {
		if count >= 32 {
			s.Truncated = true
			return errors.New("menu style row budget reached")
		}
		count++
		name, nameOK := boundedStringField(item, "name", 256, false)
		opacity, ok := intField(item, "opacity_percent")
		border, bok := intField(item, "border_width")
		sets, sok := objectField(item, "colorsets")
		if !nameOK || name == "" || !ok || opacity < 0 || opacity > 100 || !bok || border < 0 || !sok {
			return errors.New("invalid menu style")
		}
		menu, mok := intField(sets, "menu")
		active, aok := intField(sets, "active")
		if !mok || !aok {
			return errors.New("invalid menu colorsets")
		}
		c.row(s, "Observed menu "+name, fmt.Sprintf("colorset=%d; active=%d; opacity=%d%%; border=%d configuration pixels", menu, active, opacity, border))
		return nil
	})
	count = 0
	c.pages(s, "decorations", "", "decorations", &appearanceGeneration, func(item map[string]json.RawMessage) error {
		if count >= 32 {
			s.Truncated = true
			return errors.New("decoration row budget reached")
		}
		count++
		name, nameOK := boundedStringField(item, "name", 256, true)
		if !nameOK {
			return errors.New("invalid decoration name")
		}
		if name == "" {
			name = "default"
		}
		height, ok := intField(item, "title_height")
		minimum, mok := intField(item, "minimum_title_height")
		justification := stringField(item, "title_justification")
		if !ok || !mok || height < 0 || minimum < 0 || !member(justification, []string{"left", "center", "right", "unknown"}) {
			return errors.New("invalid decoration definition")
		}
		c.row(s, "Observed decoration "+name, fmt.Sprintf("title=%d; minimum=%d configuration pixels; justify=%s", height, minimum, justification))
		return nil
	})
	if len(s.Rows) == 0 {
		s.State = "unavailable"
	}
}
func masks(value int, mapping map[string]json.RawMessage) string {
	parts := []string{}
	keys := []string{}
	for k := range mapping {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	for _, key := range keys {
		if !identity(key) {
			continue
		}
		mask, ok := intField(mapping, key)
		// Composite aliases obscure a small mask; list exact components and
		// retain its numeric value for unknown or overlapping bits.
		if ok && mask > 0 && (mask&(mask-1)) == 0 && value&mask != 0 {
			parts = append(parts, key)
		}
	}
	return fmt.Sprintf("%d(%s)", value, strings.Join(parts, "+"))
}
func (c *collector) bindings(s *Section) {
	s.State = "observed"
	s.Reason = "Observed binding scopes; action arguments and window matches hidden; sampled separately"
	generation := ""
	contexts, modifiers := map[string]json.RawMessage{}, map[string]json.RawMessage{}
	policy, err := c.call("bindings", "policy", 0, "", false)
	if err != nil {
		c.problem(s, "binding policy: "+err.Error())
	} else {
		generation = stringField(policy, "generation")
		var cok, mok bool
		contexts, cok = objectField(policy, "contexts")
		modifiers, mok = objectField(policy, "modifiers")
		if !token(generation) || !cok || !mok || len(contexts) > 32 || len(modifiers) > 16 {
			generation = ""
			c.problem(s, "Malformed binding policy")
		} else {
			for _, key := range []string{"ignored_modifier_mask", "xkb_ignored_modifier_mask", "used_modifier_mask"} {
				if n, ok := intField(policy, key); ok && n >= 0 {
					c.row(s, "policy "+key, masks(n, modifiers))
				} else {
					c.problem(s, "Malformed binding mask policy")
				}
			}
		}
	}
	for _, scope := range []string{"global", "menu", "menu-fallback"} {
		count := 0
		c.pages(s, "bindings", scope, "bindings", &generation, func(item map[string]json.RawMessage) error {
			if count >= 32 {
				s.Truncated = true
				return errors.New("binding scope row budget reached")
			}
			count++
			kind := stringField(item, "type")
			code, cok := intField(item, "code")
			context, xok := intField(item, "context_mask")
			modifier, mok := intField(item, "modifier_mask")
			if !member(kind, []string{"Key", "PointerKey", "MousePress", "MouseRelease"}) || !cok || code < 0 || !xok || context < 0 || !mok || modifier < 0 {
				return errors.New("invalid binding entry")
			}
			key, keyOK := boundedStringField(item, "key", 256, true)
			action, actionOK := boundedStringField(item, "action", 1024, false)
			windowMatch, windowOK := boundedStringField(item, "window", 1024, true)
			if !keyOK || !actionOK || !windowOK {
				return errors.New("invalid binding text fields")
			}
			if key == "" {
				key = fmt.Sprint(code)
			}
			window := ""
			if windowMatch != "" {
				window = "; window match hidden"
			}
			c.row(s, scope+" "+kind+" "+key, fmt.Sprintf("context=%s; modifiers=%s; %s%s", masks(context, contexts), masks(modifier, modifiers), c.action(action), window))
			return nil
		})
	}
	placement, err := c.call("bindings", "placement", 0, generation, false)
	if err != nil {
		c.problem(s, "placement: "+err.Error())
	} else if got := stringField(placement, "generation"); !token(got) || generation != "" && got != generation {
		c.problem(s, "Placement generation changed")
	} else {
		if generation == "" {
			generation = got
		}
		for _, key := range []string{"button_count", "drag_finish_mask", "interactive_finish_mask"} {
			if value, ok := intField(placement, key); ok && value >= 0 {
				c.row(s, "placement "+key, fmt.Sprint(value))
			} else {
				c.problem(s, "Malformed placement policy")
			}
		}
		keys, keysOK := boolField(placement, "key_bindings")
		modifiers, modifiersOK := boolField(placement, "modifier_bindings")
		if !keysOK || !modifiersOK || keys || modifiers {
			c.problem(s, "Malformed placement binding limits")
		} else {
			c.row(s, "placement limits", "No key or modifier bindings in placement policy")
		}
	}
	s.Generation = generation
}
func rectangle(data json.RawMessage) (string, bool) {
	if string(data) == "null" {
		return "unknown (cached work area unavailable)", true
	}
	var m map[string]json.RawMessage
	if json.Unmarshal(data, &m) != nil || m == nil {
		return "", false
	}
	x, xok := intField(m, "x")
	y, yok := intField(m, "y")
	w, wok := intField(m, "width")
	h, hok := intField(m, "height")
	if !xok || !yok || !wok || !hok || w < 0 || h < 0 {
		return "", false
	}
	return fmt.Sprintf("x=%d y=%d width=%d height=%d actual pixels", x, y, w, h), true
}
func (c *collector) layout(s *Section) {
	s.State = "observed"
	s.Generation = "multiple-domains"
	s.Reason = "Actual-pixel topology/workspaces sampled separately; cached work areas; widget bounds and per-window attribution unavailable"
	topology, err := c.call("topology", "", 0, "", false)
	if err != nil {
		c.problem(s, "topology: "+err.Error())
	} else {
		facts, ok := objectField(topology, "topology")
		monitors, mok := listField(facts, "monitors")
		epoch := stringField(facts, "epoch")
		if !ok || !mok || len(monitors) > 64 || len(epoch) != 16 || strings.Trim(epoch, "0123456789abcdef") != "" || stringField(topology, "units") != "actual-pixels" {
			c.problem(s, "Malformed committed topology")
		} else {
			c.row(s, "Topology epoch", epoch)
			for _, monitor := range monitors {
				name := stringField(monitor, "name")
				area, aok := rectangle(monitor["geometry"])
				primary, pok := boolField(monitor, "primary")
				if name == "" || !aok || !pok {
					c.problem(s, "Malformed monitor geometry")
					continue
				}
				c.row(s, "Monitor "+name, area+fmt.Sprintf("; primary=%t", primary))
			}
		}
	}
	workspaces, err := c.call("workspaces", "", 0, "", false)
	if err != nil {
		c.problem(s, "workspaces: "+err.Error())
	} else {
		monitors, ok := listField(workspaces, "monitors")
		mode := stringField(workspaces, "desktop_mode")
		if !ok || len(monitors) > 64 || stringField(workspaces, "units") != "actual-pixels" || !member(mode, []string{"global", "per-monitor", "shared"}) {
			c.problem(s, "Malformed workspace inventory")
		} else {
			c.row(s, "Workspace mode", mode+"; work areas cached")
			for _, monitor := range monitors {
				name := stringField(monitor, "name")
				desk, dok := intField(monitor, "desk")
				scale, sok := intField(monitor, "scale_quarters")
				pages, pok := objectField(monitor, "pages")
				width, wok := intField(pages, "width")
				height, hok := intField(pages, "height")
				if name == "" || !dok || !sok || scale < 1 || !pok || !wok || !hok || width < 1 || height < 1 {
					c.problem(s, "Malformed workspace geometry")
					continue
				}
				c.row(s, "Workspace "+name, fmt.Sprintf("desk=%d; pages=%dx%d; scale=%d/4", desk, width, height, scale))
				for _, key := range []string{"viewport", "work_area", "dynamic_work_area"} {
					area, ok := rectangle(monitor[key])
					if ok {
						c.row(s, name+" "+key, area)
					} else {
						c.problem(s, "Malformed workspace rectangle")
					}
				}
			}
		}
	}
	desktop, err := c.call("desktop", "", 0, "", false)
	if err != nil {
		c.problem(s, "desktop: "+err.Error())
	} else {
		desk, ok := intField(desktop, "desk")
		if !ok || stringField(desktop, "units") != "actual-pixels" {
			c.problem(s, "Malformed current desktop")
		} else {
			c.row(s, "Current desktop", fmt.Sprintf("desk=%d", desk))
			for _, key := range []string{"viewport", "work_area"} {
				area, ok := rectangle(desktop[key])
				if ok {
					c.row(s, "Current "+key, area)
				} else {
					c.problem(s, "Malformed current desktop rectangle")
				}
			}
		}
	}
}

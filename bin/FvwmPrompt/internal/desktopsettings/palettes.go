// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopsettings

import "fmt"

// Palettes is an operation-owned, complete set of the existing named colors.
// The source format remains the shipped palette/phase/role JSON object.
type Palettes map[string]map[string]map[string]string

// DecodePalettes reuses the bounded, duplicate-aware settings JSON decoder.
// Validate every phase before any consumer can replace its working colors.
func DecodePalettes(data []byte) (Palettes, error) {
	decoded, err := DecodeJSON(data)
	if err != nil {
		return nil, err
	}
	object, ok := decoded.(map[string]any)
	if !ok || len(object) != 4 {
		return nil, fmt.Errorf("Expected exactly slate, dune, grove and ink palettes")
	}
	result := Palettes{}
	for _, name := range []string{"slate", "dune", "grove", "ink"} {
		phases, ok := object[name].(map[string]any)
		if !ok || len(phases) != 2 {
			return nil, fmt.Errorf("%s: expected exactly dark and light phases", name)
		}
		result[name] = map[string]map[string]string{}
		for _, phase := range []string{"dark", "light"} {
			roles, ok := phases[phase].(map[string]any)
			if !ok || len(roles) != 8 {
				return nil, fmt.Errorf("%s.%s: expected eight color roles", name, phase)
			}
			colors := map[string]string{}
			for _, role := range []string{"background", "surface", "raised", "text", "muted", "border", "accent", "on_accent"} {
				color, ok := roles[role].(string)
				if !ok || len(color) != 7 || color[0] != '#' {
					return nil, fmt.Errorf("%s.%s.%s: expected #RRGGBB", name, phase, role)
				}
				for _, c := range color[1:] {
					if !(c >= '0' && c <= '9' || c >= 'a' && c <= 'f' || c >= 'A' && c <= 'F') {
						return nil, fmt.Errorf("%s.%s.%s: expected #RRGGBB", name, phase, role)
					}
				}
				colors[role] = color
			}
			result[name][phase] = colors
		}
	}
	return result, nil
}

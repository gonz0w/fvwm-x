// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopworker

import (
	"fmt"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
)

// PaletteResult carries acquisition failures as bounded data, so the resident
// owner can show the source diagnostic without worker stderr or a second read.
type PaletteResult struct {
	Palettes desktopsettings.Palettes `json:"palettes"`
	Error    string                   `json:"error"`
}

func readPalettes(path string, read func(string) ([]byte, error)) PaletteResult {
	data, err := read(path)
	var palettes desktopsettings.Palettes
	if err == nil {
		palettes, err = desktopsettings.DecodePalettes(data)
	}
	if err != nil {
		// Parser messages can contain source keys. Bound the worker reply before
		// serialization; the controller renders diagnostic text literally.
		message := fmt.Sprintf("%s: %s", path, err)
		if len(message) > 4096 {
			message = "Palette source diagnostic exceeds the display limit"
		}
		return PaletteResult{Error: message}
	}
	return PaletteResult{Palettes: palettes}
}

// LoadPalettes is also used at startup. readBounded opens nonblocking and checks
// the opened file is regular before reading; a FIFO cannot hang this read.
func LoadPalettes(path string) (desktopsettings.Palettes, error) {
	result := readPalettes(path, readBounded)
	if result.Error != "" {
		return nil, fmt.Errorf("%s", result.Error)
	}
	return result.Palettes, nil
}

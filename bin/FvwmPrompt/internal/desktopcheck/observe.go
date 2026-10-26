// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcheck

// Observation is a successful registered setting literal, in traversal order.
// Last never asserts the setting's exact runtime source.
type Observation struct {
	Location
	Key   string `json:"key"`
	Value any    `json:"value"`
	Order int    `json:"order"`
	Last  bool   `json:"last"`
}

type ObservationResult struct {
	Report       Result        `json:"report"`
	Observations []Observation `json:"observations"`
	Truncated    bool          `json:"truncated"`
}

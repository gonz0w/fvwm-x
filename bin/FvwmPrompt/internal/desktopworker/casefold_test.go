// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopworker

import "testing"

func TestFullCasefoldRetainsNonSimpleMappings(t *testing.T) {
	for _, pair := range [][2]string{
		{"Straße", "strasse"}, {"STRASSE", "strasse"}, {"Σςσ", "σσσ"},
		{"İ", "i\u0307"}, {"ſ", "s"}, {"µ", "μ"}, {"ﬃ", "ffi"},
		{"\u13f8\u13f0", "\u13f0\u13f0"}, {"ﬁle", "file"}, {"ÅNGSTRÖM", "ångström"},
	} {
		if got := Casefold(pair[0]); got != pair[1] {
			t.Errorf("%q: got %q want %q", pair[0], got, pair[1])
		}
	}
}

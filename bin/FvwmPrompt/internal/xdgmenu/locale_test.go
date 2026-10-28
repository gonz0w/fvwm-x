// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"testing"
)

func TestLocalePreferencesPreserveProviderEnvironmentOrder(t *testing.T) {
	for _, test := range []struct {
		name string
		env  map[string]string
		want []string
	}{
		{"empty", nil, nil},
		{"lang", map[string]string{"LANG": "de_DE.UTF-8"}, []string{"de_DE.UTF-8"}},
		{"messages wins", map[string]string{"LC_MESSAGES": "fr_FR.UTF-8", "LANG": "de_DE.UTF-8"}, []string{"fr_FR.UTF-8"}},
		{"all wins", map[string]string{"LC_ALL": "da_DK.UTF-8", "LC_MESSAGES": "fr_FR.UTF-8", "LANG": "de_DE.UTF-8"}, []string{"da_DK.UTF-8"}},
		{"language first", map[string]string{"LANGUAGE": "fr_CA:fr", "LANG": "de_DE.UTF-8"}, []string{"fr_CA", "fr", "de_DE.UTF-8"}},
		{"language alone", map[string]string{"LANGUAGE": "fr:da"}, []string{"fr", "da"}},
		{"C suppresses language", map[string]string{"LC_ALL": "C", "LANGUAGE": "fr", "LANG": "de_DE.UTF-8"}, nil},
		{"C encoding suppresses language", map[string]string{"LC_MESSAGES": "C.UTF-8", "LANGUAGE": "fr"}, nil},
		{"POSIX suppresses language", map[string]string{"LANG": "POSIX", "LANGUAGE": "fr"}, nil},
	} {
		t.Run(test.name, func(t *testing.T) {
			before := make(map[string]string, len(test.env))
			for key, value := range test.env {
				before[key] = value
			}
			got := LocalePreferences(test.env)
			if !reflect.DeepEqual(got, test.want) {
				t.Fatal(got, test.want)
			}
			for key, value := range before {
				if test.env[key] != value {
					t.Fatal("locale environment mutated")
				}
			}
		})
	}
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"reflect"
	"testing"
)

func TestEffectiveDisplayLastOptionAndLiteralValues(t *testing.T) {
	for _, tc := range []struct {
		args []string
		want string
	}{
		{nil, ":inherited"},
		{[]string{"-d", ":first", "-display", ":second", "--display", ":last"}, ":last"},
		{[]string{"-d", ":first", "--display", ""}, ":inherited"},
		{[]string{"-f", "-d", "-cmd", "--display", "--cmd", "-display", "-d", ":target"}, ":target"},
		{[]string{"-d", "--display"}, "--display"},
	} {
		before := append([]string(nil), tc.args...)
		got, err := effectiveDisplay(tc.args, ":inherited")
		if err != nil || got != tc.want || !reflect.DeepEqual(tc.args, before) {
			t.Fatalf("args=%q got=%q want=%q err=%v", tc.args, got, tc.want, err)
		}
	}
}

func TestEffectiveDisplayConsumesEveryCoreOptionValue(t *testing.T) {
	for _, option := range []string{"-f", "-c", "-cmd", "--cmd", "-i", "-clientid", "--clientid", "-clientId", "--clientId", "-F", "-restore", "--restore", "-I", "-visualid", "--visualid", "-visualId", "--visualId", "-C", "-visual", "--visual", "-l", "-color-limit", "--color-limit", "-o", "--output-file"} {
		t.Run(option, func(t *testing.T) {
			got, err := effectiveDisplay([]string{option, "-d", "--display", ":target"}, ":inherited")
			if err != nil || got != ":target" {
				t.Fatalf("%q %v", got, err)
			}
			if _, err := effectiveDisplay([]string{option}, ":inherited"); err == nil {
				t.Fatal("missing value admitted")
			}
		})
	}
	flags := []string{"-debug_stack_ring", "--debug-stack-ring", "-r", "-replace", "--replace", "-L", "-strict-color-limit", "--strict-color-limit", "-A", "-allocate-palette", "--allocate-palette", "-S", "-static-palette", "--static-palette", "-P", "-visual-palette", "--visual-palette", "-v", "--verbose", "-d", ":target"}
	if got, err := effectiveDisplay(flags, ":inherited"); err != nil || got != ":target" {
		t.Fatalf("%q %v", got, err)
	}
}

func TestEffectiveDisplayRefusesUncertainOrNonStartupInvocation(t *testing.T) {
	for _, args := range [][]string{{"-d"}, {"--display"}, {"--future-option", "-d", ":target"}, {"--display=:target"}, {"argument"}, {"--help"}, {"--version"}} {
		if _, err := effectiveDisplay(args, ":inherited"); err == nil {
			t.Fatalf("accepted %q", args)
		}
	}
	env := []string{"DISPLAY=:old", "XAUTHORITY=/private/auth", "DISPLAY=:duplicate", "KEEP=value"}
	want := []string{"XAUTHORITY=/private/auth", "KEEP=value", "DISPLAY=:target"}
	if got := displayEnvironment(env, ":target"); !reflect.DeepEqual(got, want) {
		t.Fatalf("%q", got)
	}
}

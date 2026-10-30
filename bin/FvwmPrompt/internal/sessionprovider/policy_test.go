// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionprovider

import (
	"errors"
	"reflect"
	"testing"
)

func tools(paths map[string]string) func(string) (string, error) {
	return func(name string) (string, error) {
		if path, ok := paths[name]; ok {
			return path, nil
		}
		return "", errors.New("not installed")
	}
}

func TestNativeFixedPlans(t *testing.T) {
	for _, system := range []string{"Linux", "FreeBSD", "OpenBSD"} {
		for _, action := range []string{"poweroff", "reboot"} {
			for _, systemctl := range []bool{false, true} {
				paths := map[string]string{"shutdown": "/sbin/shutdown"}
				if systemctl {
					paths["systemctl"] = "/usr/bin/systemctl"
				}
				flag := "-r"
				if action == "poweroff" {
					flag = "-p"
				}
				want := []string{"/sbin/shutdown", flag, "now"}
				if system == "Linux" && systemctl {
					want = []string{"/usr/bin/systemctl", action}
				}
				got, err := NativePlan(system, action, tools(paths))
				if err != nil || !reflect.DeepEqual(got, want) {
					t.Fatalf("%s %s: %q %v", system, action, got, err)
				}
			}
		}
	}
}

func TestUnsafeAndMissingPlansRefuse(t *testing.T) {
	for _, path := range []string{"", "shutdown", "./shutdown", "/sbin/../sbin/shutdown", "/sbin/shutdown\x00", "/sbin/shutdown\n"} {
		if _, err := NativePlan("FreeBSD", "poweroff", tools(map[string]string{"shutdown": path})); err == nil {
			t.Fatalf("accepted path %q", path)
		}
	}
	for _, pair := range [][2]string{{"Other", "poweroff"}, {"Linux", "suspend"}, {"Linux", "poweroff; id"}} {
		if _, err := NativePlan(pair[0], pair[1], func(string) (string, error) {
			t.Fatal("unsupported request reached discovery")
			return "", nil
		}); err == nil {
			t.Fatal("unsupported request accepted")
		}
	}
	if _, err := NativePlan("Linux", "reboot", tools(nil)); err == nil {
		t.Fatal("missing tools accepted")
	}
	if _, err := NativePlan("Linux", "reboot", nil); err == nil {
		t.Fatal("missing discovery accepted")
	}
}

func TestLockerDiscoveryIsLiteralAndSafe(t *testing.T) {
	for _, item := range []struct {
		paths map[string]string
		want  []string
	}{
		{map[string]string{"xlock": "/usr/bin/xlock", "slock": "/usr/bin/slock"}, []string{"/usr/bin/xlock"}},
		{map[string]string{"slock": "/usr/local/bin/slock"}, []string{"/usr/local/bin/slock"}},
		{map[string]string{"xlock": "./xlock", "slock": "/usr/bin/slock"}, []string{"/usr/bin/slock"}},
	} {
		got, err := LockerPlan(tools(item.paths))
		if err != nil || !reflect.DeepEqual(got, item.want) {
			t.Fatalf("%q %v", got, err)
		}
	}
	if _, err := LockerPlan(tools(map[string]string{"xlock": "./xlock"})); err == nil {
		t.Fatal("unsafe locker accepted")
	}
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"reflect"
	"testing"
)

func TestOptions(t *testing.T) {
	cases := []struct {
		args []string
		want options
	}{
		{[]string{"-dci1", "Echo", "ignored"}, options{stdin: true, info: true, detailed: true, commands: []string{"Echo", "ignored"}}},
		{[]string{"-FmSr", "-w10", "-f", "space path", "Echo", "-h"}, options{socket: "space path", commands: []string{"Echo", "-h"}}},
		{[]string{"--socket=space path", "--inf", "0", "--", "-h"}, options{socket: "space path", commands: []string{"-h"}}},
		{[]string{"--version"}, options{action: "version"}},
		{[]string{"--verbose"}, options{detailed: true}},
	}
	for _, c := range cases {
		got, err := parse(c.args)
		if err != nil || !reflect.DeepEqual(got, c.want) {
			t.Errorf("%q: %+v, %v; want %+v", c.args, got, err, c.want)
		}
	}
	for _, args := range [][]string{{"--v"}, {"-i"}, {"--help=yes"}, {"-z"}, {"--no-such-option"}, {"--socket"}} {
		if _, err := parse(args); err == nil {
			t.Errorf("accepted %q", args)
		}
	}
}

func TestDiscovery(t *testing.T) {
	cases := []struct {
		env            map[string]string
		explicit, want string
	}{
		{map[string]string{"FVWMMFL_SOCKET": "/env"}, "/explicit", "/explicit"},
		{map[string]string{"FVWMMFL_SOCKET": "/env"}, "", "/env"},
		{map[string]string{"DISPLAY": ":0.1", "FVWMMFL_SOCKET_PATH": "/private"}, "", "/private/fvwm_mfl__0_1.sock"},
		{map[string]string{"DISPLAY": "host:2", "FVWMMFL_SOCKET_PATH": "$FVWM_USERDIR/mfl", "FVWM_USERDIR": "/config"}, "", "/config/mfl/fvwm_mfl_host_2.sock"},
		{map[string]string{"FVWMMFL_SOCKET_PATH": "mfl", "FVWM_USERDIR": "/config"}, "", "/config/mfl/fvwm_mfl_default.sock"},
		{map[string]string{"XDG_RUNTIME_DIR": "/run/user/42", "TMPDIR": "/tmp-other"}, "", "/run/user/42/fvwmmfl/fvwm_mfl_default.sock"},
		{map[string]string{"TMPDIR": "/tmp-other"}, "", "/tmp-other/fvwmmfl-42/fvwm_mfl_default.sock"},
		{map[string]string{}, "", "/tmp/fvwmmfl-42/fvwm_mfl_default.sock"},
	}
	for _, c := range cases {
		if got := socketPath(c.explicit, func(k string) string { return c.env[k] }, 42); got != c.want {
			t.Errorf("%v: %s; want %s", c.env, got, c.want)
		}
	}
}

func TestInformationalExit(t *testing.T) {
	for _, flag := range []string{"--help", "-v", "--version"} {
		var out, diagnostic bytes.Buffer
		status := run([]string{flag}, &bytes.Buffer{}, &out, &diagnostic, func(string) string { return "" })
		if status != 0 || out.Len() == 0 || diagnostic.Len() != 0 {
			t.Fatalf("%s: %d %q %q", flag, status, out.String(), diagnostic.String())
		}
	}
}

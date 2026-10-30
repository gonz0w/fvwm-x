// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopworker

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"strings"
	"syscall"
	"testing"
	"time"
)

func TestRequestOwnership(t *testing.T) {
	for _, data := range []string{
		`null`, `[]`, `{}`, `{"version":true}`, `{"version":1,"operation":"status","extra":1}`,
		`{"version":1,"operation":"status","Operation":"power"}`, `{"version":1,"version":1,"operation":"status"}`,
		`{"version":1,"operation":"status"} {}`, `{"version":1,"operation":"status","directories":null}`,
		`{"version":1,"operation":"status","system":"Other"}`, `{"version":1,"operation":"status","action":"up"}`,
		`{"version":1,"operation":"power","action":"logout"}`, `{"version":1,"operation":"volume","action":"up; id"}`,
		`{"version":1,"operation":"apps","directories":["bad\u0000path"]}`, strings.Repeat(" ", RequestLimit+1),
	} {
		if _, err := Decode([]byte(data)); err == nil {
			t.Errorf("accepted %q", data)
		}
	}
	for _, data := range []string{`{"version":1,"operation":"status","system":"Linux"}`, `{"version":1,"operation":"apps","directories":[]}`, `{"version":1,"operation":"volume","system":"OpenBSD","action":"mute"}`} {
		if _, err := Decode([]byte(data)); err != nil {
			t.Error(err)
		}
	}
}

func TestLauncherLiteralArguments(t *testing.T) {
	got, err := launcherExec(`app "hello world" %F %i %c %k %%`, "A; touch /tmp/never", "/tmp/a.desktop", "an-icon")
	want := []string{"app", "hello world", "--icon", "an-icon", "A; touch /tmp/never", "/tmp/a.desktop", "%"}
	if err != nil || !reflect.DeepEqual(got, want) {
		t.Fatalf("%q %v", got, err)
	}
	for _, pair := range [][2]string{{`app "literal; value"`, "literal; value"}, {`app ""`, ""}, {`app --file=%f`, "--file="}} {
		got, err := launcherExec(pair[0], "", "", "")
		if err != nil || len(got) != 2 || got[1] != pair[1] {
			t.Errorf("%q: %q %v", pair[0], got, err)
		}
	}
	for _, raw := range []string{`app %f %F`, `app %Z`, `%f`, `app "broken`, `app \`, "app \x00", "app " + strings.Repeat("x", CommandLimit)} {
		if _, err := launcherExec(raw, "", "", ""); err == nil {
			t.Errorf("accepted %q", raw)
		}
	}
	if _, err := launcherExec("app "+strings.Repeat("%c ", 2000), strings.Repeat("X", 4096), "", ""); err == nil {
		t.Fatal("unbounded substitutions accepted")
	}
}

func TestApplicationPrecedenceAndLiteralRecords(t *testing.T) {
	root := t.TempDir()
	user, system := filepath.Join(root, "user"), filepath.Join(root, "system")
	os.Mkdir(user, 0700)
	os.Mkdir(system, 0700)
	write := func(dir, name, data string) {
		t.Helper()
		if err := os.WriteFile(filepath.Join(dir, name), []byte("[Desktop Entry]\nType=Application\n"+data), 0600); err != nil {
			t.Fatal(err)
		}
	}
	write(system, "app.desktop", "Name=System\nExec=/bin/sh\n")
	write(user, "app.desktop", "Hidden=true\n")
	write(system, "safe.desktop", "Name=Quoted;Name\nExec=/bin/sh \"literal; argument\"\n")
	write(system, "bad.desktop", "Name=Bad\nExec=/bin/sh %Z\n")
	write(system, "try.desktop", "Name=Missing\nTryExec=absent-program\nExec=/bin/sh\n")
	write(system, "other.desktop", "Name=Other\nOnlyShowIn=Other;\nExec=/bin/sh\n")
	write(system, "duplicate.desktop", "Name=First\nName=Last\nExec=/bin/sh\n")
	t.Setenv("XDG_CURRENT_DESKTOP", "FVWM")
	result, err := applications(context.Background(), []string{user, system}, NativeEnvironment())
	if err != nil || len(result) != 1 {
		t.Fatalf("%v %v", result, err)
	}
	if !reflect.DeepEqual(result[0], Application{Name: "Quoted;Name", Argv: []string{"/bin/sh", "literal; argument"}}) {
		t.Fatalf("%+v", result)
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, err := applications(ctx, []string{user}, NativeEnvironment()); !errors.Is(err, context.Canceled) {
		t.Fatal(err)
	}
}

func TestApplicationBoundsAndNoDirectoryLinks(t *testing.T) {
	root := t.TempDir()
	other := t.TempDir()
	os.WriteFile(filepath.Join(other, "hidden.desktop"), []byte("[Desktop Entry]\nType=Application\nName=Outside\nExec=/bin/sh\n"), 0600)
	os.Symlink(other, filepath.Join(root, "linked"))
	os.WriteFile(filepath.Join(root, "large.desktop"), []byte(strings.Repeat("x", CommandLimit+1)), 0600)
	got, err := applications(context.Background(), []string{root}, NativeEnvironment())
	if err != nil || len(got) != 0 {
		t.Fatalf("%v %v", got, err)
	}
	if data, err := readBounded(filepath.Join(root, "large.desktop")); err == nil || data != nil {
		t.Fatal("oversized source read")
	}
	for i := 0; i < 520; i++ {
		os.WriteFile(filepath.Join(root, fmt.Sprintf("%04d.desktop", i)), []byte(fmt.Sprintf("[Desktop Entry]\nType=Application\nName=App%04d\nExec=/bin/sh\n", i)), 0600)
	}
	got, err = applications(context.Background(), []string{root}, NativeEnvironment())
	if err != nil || len(got) != 512 {
		t.Fatalf("%d %v", len(got), err)
	}
}

func TestStatusBackendFormats(t *testing.T) {
	for _, system := range []string{"FreeBSD", "OpenBSD"} {
		t.Run(system, func(t *testing.T) {
			counters := "38 0 80 1 11386"
			expected := [2]int64{11505, 11386}
			if system == "OpenBSD" {
				counters = "180,0,147,13,6,3954"
				expected = [2]int64{4300, 3954}
			}
			values := map[string]string{"kern.cp_time": counters, "hw.physmem": "1073741824", "hw.pagesize": "4096", "vm.stats.vm.v_free_count": "1024", "hw.acpi.battery.life": "75"}
			env := NativeEnvironment()
			env.Run = func(_ context.Context, argv []string, _ time.Duration) (string, error) {
				if argv[0] == "sysctl" {
					return values[argv[2]], nil
				}
				if argv[0] == "apm" {
					if argv[1] == "-b" {
						return "3", nil
					}
					return "75", nil
				}
				return map[string]string{"vmstat": "  1024 pages free", "ifconfig": "lo0: flags=UP\n\tstatus: active\nem0: flags=UP\n\tstatus: active\n"}[argv[0]], nil
			}
			got := collectStatus(context.Background(), system, env)
			if got.CPU == nil || *got.CPU != expected || got.Memory == nil || *got.Memory != 1069547520 || got.Battery == nil || *got.Battery != 75 || got.Network != "em0" {
				t.Fatalf("%+v", got)
			}
		})
	}
	env := NativeEnvironment()
	env.Run = func(_ context.Context, argv []string, _ time.Duration) (string, error) {
		if reflect.DeepEqual(argv, []string{"apm", "-b"}) {
			return "4", nil
		}
		return "0", nil
	}
	if collectStatus(context.Background(), "OpenBSD", env).Battery != nil {
		t.Fatal("absent battery accepted")
	}
	env.Run = func(context.Context, []string, time.Duration) (string, error) { return "", errors.New("unavailable") }
	got := collectStatus(context.Background(), "FreeBSD", env)
	if got.CPU != nil || got.Memory != nil || got.Battery != nil || got.Network != "Disconnected" {
		t.Fatalf("%+v", got)
	}
}

func TestLinuxCountersAndOverflow(t *testing.T) {
	env := NativeEnvironment()
	env.Read = func(path string) ([]byte, error) {
		return []byte(map[string]string{"/proc/stat": "cpu 1 2 3 40 4 5 6 7 99\n", "/proc/meminfo": "MemTotal: 100 kB\nMemAvailable: 25 kB\n", "/sys/class/net/eth0/operstate": "up", "/sys/class/power_supply/BAT0/type": "Battery", "/sys/class/power_supply/BAT0/capacity": "75"}[path]), nil
	}
	env.Entries = func(path string) ([]string, error) {
		if path == "/sys/class/net" {
			return []string{"lo", "eth0"}, nil
		}
		return []string{"BAT0"}, nil
	}
	got := collectStatus(context.Background(), "Linux", env)
	if got.CPU == nil || *got.CPU != [2]int64{68, 44} || got.Memory == nil || *got.Memory != 76800 || got.Battery == nil || *got.Battery != 75 || got.Network != "eth0" {
		t.Fatalf("%+v", got)
	}
	if usedMemory("1", "9223372036854775807", "4096") != nil || cpu([]string{"9223372036854775807", "1"}, 1) != nil || battery("101") != nil {
		t.Fatal("overflow/domain accepted")
	}
}

func TestAudioAndPowerAreFixedArgv(t *testing.T) {
	for _, system := range []string{"Linux", "FreeBSD", "OpenBSD"} {
		for _, action := range []string{"up", "down", "mute"} {
			t.Run(system+action, func(t *testing.T) {
				env := NativeEnvironment()
				env.LookPath = func(name string) (string, error) { return name, nil }
				var calls [][]string
				env.Run = func(_ context.Context, argv []string, _ time.Duration) (string, error) {
					calls = append(calls, argv)
					if len(argv) > 2 && argv[1] == "-n" {
						return "1", nil
					}
					return "changed", nil
				}
				if volume(context.Background(), system, action, env) != "changed" {
					t.Fatal("no volume result")
				}
				var want []string
				switch system {
				case "Linux":
					want = []string{"amixer", "sset", "Master", map[string]string{"up": "5%+", "down": "5%-", "mute": "toggle"}[action]}
				case "FreeBSD":
					want = []string{"mixer", map[string]string{"up": "vol.volume=+5%", "down": "vol.volume=-5%", "mute": "vol.mute=toggle"}[action]}
				case "OpenBSD":
					want = []string{"sndioctl", map[string]string{"up": "output.level=+0.05", "down": "output.level=-0.05", "mute": "output.mute=0"}[action]}
				}
				if !reflect.DeepEqual(calls[len(calls)-1], want) {
					t.Fatalf("%q != %q", calls, want)
				}
			})
		}
	}
	for _, system := range []string{"Linux", "FreeBSD", "OpenBSD"} {
		for _, action := range []string{"poweroff", "reboot"} {
			env := NativeEnvironment()
			env.LookPath = func(name string) (string, error) { return "/owned/" + name, nil }
			env.Run = func(_ context.Context, argv []string, deadline time.Duration) (string, error) {
				want := []string{"/owned/systemctl", action}
				if system != "Linux" {
					flag := "-r"
					if action == "poweroff" {
						flag = "-p"
					}
					want = []string{"/owned/shutdown", flag, "now"}
				}
				if !reflect.DeepEqual(argv, want) || deadline != 3*time.Second {
					t.Fatalf("%q", argv)
				}
				return "", nil
			}
			if power(context.Background(), system, action, env) != "Power request sent." {
				t.Fatal("power result")
			}
			env.Run = func(context.Context, []string, time.Duration) (string, error) { return "", &exec.ExitError{} }
			if !strings.Contains(power(context.Background(), system, action, env), "unconfirmed") {
				t.Fatal("command exit falsely established denial or completion")
			}
		}
	}
}

func TestFailedBackendOwnershipAndInvalidDirectRequest(t *testing.T) {
	env := NativeEnvironment()
	calls := 0
	env.Run = func(context.Context, []string, time.Duration) (string, error) {
		calls++
		return "", context.DeadlineExceeded
	}
	if _, err := Run(context.Background(), Request{Version: 1, Operation: "status", System: "FreeBSD"}, env); !errors.Is(err, ErrAbandonedJob) {
		t.Fatal(err)
	}
	calls = 0
	if _, err := Run(context.Background(), Request{Version: 1, Operation: "power", System: "Linux", Action: "invalid"}, env); err == nil || calls != 0 {
		t.Fatal("invalid typed request executed backend")
	}
}

func TestPowerFailureNeverReplays(t *testing.T) {
	for _, failure := range []error{&exec.ExitError{}, context.DeadlineExceeded, errors.New("transport lost")} {
		env := NativeEnvironment()
		env.LookPath = func(name string) (string, error) { return "/owned/" + name, nil }
		calls := 0
		env.Run = func(_ context.Context, argv []string, _ time.Duration) (string, error) {
			calls++
			if !reflect.DeepEqual(argv, []string{"/owned/systemctl", "poweroff"}) {
				t.Fatalf("unexpected fallback: %q", argv)
			}
			return "", failure
		}
		if got := power(context.Background(), "Linux", "poweroff", env); !strings.Contains(got, "unconfirmed") || calls != 1 {
			t.Fatalf("%s: %d calls", got, calls)
		}
	}
	if got := power(context.Background(), "Linux", "suspend", Environment{}); got != "Power service is unavailable." {
		t.Fatal("unsupported action reached execution")
	}
}

func TestRetiredSettingsRPCIsRefused(t *testing.T) {
	for _, payload := range []string{
		`{"version":1,"operation":"settings","settings":{"operation":"registry"}}`,
		`{"version":1,"operation":"settings"}`,
		`{"version":1,"operation":"power","action":"reboot","settings":{}}`,
	} {
		if _, err := Decode([]byte(payload)); err == nil {
			t.Fatal("retired private request accepted: " + payload)
		}
	}
	if _, err := Run(context.Background(), Request{Version: 1, Operation: "settings"}, Environment{}); err == nil {
		t.Fatal("direct caller reached retired policy")
	}
}

func TestRequestCountRefusesBeforeValueScan(t *testing.T) {
	directories := make([]string, 65)
	directories[0] = "\x00"
	request := Request{Version: 1, Operation: "apps", Directories: directories}
	if _, err := Run(context.Background(), request, Environment{}); err == nil || err.Error() != "too many application directories" {
		t.Fatalf("count must be refused before element inspection: %v", err)
	}
}

func TestConfigurationCheckCannotUseBackends(t *testing.T) {
	root := t.TempDir()
	path := filepath.Join(root, "config")
	if err := os.WriteFile(path, []byte("Exec touch marker\n*FvwmDesktop: motion on\n"), 0600); err != nil {
		t.Fatal(err)
	}
	payload, _ := json.Marshal(map[string]string{"path": path, "userdir": root, "datadir": root, "cwd": root})
	request := Request{Version: 1, Operation: "check", Check: payload}
	value, err := Run(context.Background(), request, Environment{})
	if err != nil || value == nil {
		t.Fatalf("%v %v", value, err)
	}
	for _, field := range []string{"action", "directories", "settings"} {
		outer := map[string]any{"version": 1, "operation": "check", "check": json.RawMessage(payload)}
		outer[field] = map[string]any{"action": "up", "directories": []string{"/tmp"}, "settings": map[string]any{}}[field]
		data, _ := json.Marshal(outer)
		if _, err := Decode(data); err == nil {
			t.Fatalf("accepted unrelated %s", field)
		}
	}
	request.Operation = "apps"
	if _, err := Run(context.Background(), request, Environment{}); err == nil {
		t.Fatal("check payload admitted by another operation")
	}
}

func TestFileMenuIsBoundedAndLiteral(t *testing.T) {
	root := t.TempDir()
	request := Request{Version: 1, Operation: "files", Path: root}
	value, err := Run(context.Background(), request, Environment{})
	if err != nil || len(value.([]FileMenuEntry)) != 0 {
		t.Fatalf("empty folder: %v %v", value, err)
	}
	for _, name := range []string{"z-file", "A-file", "literal; command"} {
		if err := os.WriteFile(filepath.Join(root, name), nil, 0600); err != nil {
			t.Fatal(err)
		}
	}
	if err := os.Mkdir(filepath.Join(root, "directory"), 0700); err != nil {
		t.Fatal(err)
	}
	value, err = Run(context.Background(), request, Environment{})
	if err != nil {
		t.Fatal(err)
	}
	records := value.([]FileMenuEntry)
	if len(records) != 4 || !records[0].Directory || records[1].Name != "A-file" || records[2].Name != "literal; command" {
		t.Fatalf("%+v", records)
	}
	request.Path = filepath.Join(root, "fifo")
	if err := syscall.Mkfifo(request.Path, 0600); err != nil {
		t.Fatal(err)
	}
	if _, err := Run(context.Background(), request, Environment{}); err == nil {
		t.Fatal("FIFO accepted as folder")
	}
	request.Operation = "apps"
	if _, err := Run(context.Background(), request, Environment{}); err == nil {
		t.Fatal("unrelated operation accepted file path")
	}
}

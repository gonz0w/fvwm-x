// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"reflect"
	"strings"
	"testing"
)

func autostartDesktop(t *testing.T, extra string) *Desktop {
	t.Helper()
	d, err := ParseDesktop([]byte("[Desktop Entry]\nType=Application\nName=Fixture\nExec=fixture-app\n" + extra))
	if err != nil {
		t.Fatal(err)
	}
	return d
}

func TestAutostartPolicy(t *testing.T) {
	for _, test := range []struct {
		name, extra    string
		start, invalid bool
	}{
		{"ordinary", "", true, false},
		{"menu-hidden", "NoDisplay=true\n", true, false},
		{"ignore-menu-key", "NoDisplay=irrelevant\n", true, false},
		{"hidden", "Hidden=true\n", false, false},
		{"invalid-hidden", "Hidden=yes\n", false, true},
		{"only-alias", "OnlyShowIn=FVWM;\n", true, false},
		{"only-primary", "OnlyShowIn=FVWM3;\n", true, false},
		{"case-sensitive", "OnlyShowIn=fvwm;\n", false, false},
		{"only-empty", "OnlyShowIn=\n", false, false},
		{"not-empty", "NotShowIn=\n", true, false},
		{"excluded", "NotShowIn=Other;FVWM;\n", false, false},
		{"both-empty", "OnlyShowIn=\nNotShowIn=\n", false, true},
		{"one-empty", "OnlyShowIn=FVWM;\nNotShowIn=\n", false, true},
		{"try-absolute", "TryExec=/bin/fixture\n", true, false},
		{"try-bare", "TryExec=fixture\n", true, false},
		{"try-missing", "TryExec=absent\n", false, false},
		{"try-relative", "TryExec=bin/fixture\n", false, true},
		{"try-empty", "TryExec=\n", true, false},
		{"terminal", "Terminal=true\n", false, false},
		{"bad-terminal", "Terminal=yes\n", false, true},
		{"bus-fallback", "DBusActivatable=true\n", true, false},
		{"bad-bus", "DBusActivatable=yes\n", false, true},
		{"directory", "Path=/work\n", true, false},
		{"absolute-directory-spelling", "Path=/work/\n", true, false},
		{"relative-directory", "Path=work\n", false, true},
		{"missing-directory", "Path=/missing\n", false, false},
	} {
		t.Run(test.name, func(t *testing.T) {
			opts := AutostartOptions{Desktops: []string{"FVWM3", "FVWM"},
				Executable: func(name string) bool { return name != "absent" },
				Directory:  func(path string) bool { return path == "/work" || path == "/work/" }}
			launch, reason, err := PlanAutostart(autostartDesktop(t, test.extra), "fixture.desktop", "/config/autostart/fixture.desktop", opts)
			if (err != nil) != test.invalid || (len(launch.Argv) != 0) != test.start {
				t.Fatalf("launch=%+v reason=%q err=%v", launch, reason, err)
			}
			if !test.start && err == nil && reason == "" {
				t.Fatal("skip without a reason")
			}
			if test.name == "bus-fallback" && !strings.Contains(reason, "fallback") {
				t.Fatal("missing portable Exec fallback diagnostic", reason)
			}
		})
	}
}

func TestAutostartExecAndRequiredKeys(t *testing.T) {
	d := autostartDesktop(t, "Name[fr]=Nom\nIcon=fixture-icon\n")
	d.Groups["Desktop Entry"]["Exec"] = `fixture-app "literal \\$HOME;\\$(touch nope)" %c %k %i %F %%`
	launch, _, err := PlanAutostart(d, "fixture.desktop", "/config/autostart/fixture.desktop", AutostartOptions{Locales: []string{"fr_FR.UTF-8"}})
	want := []string{"fixture-app", "literal $HOME;$(touch nope)", "Nom", "/config/autostart/fixture.desktop", "--icon", "fixture-icon", "%"}
	if err != nil || !reflect.DeepEqual(launch.Argv, want) {
		t.Fatal(launch, err)
	}
	for _, test := range []struct{ key, value string }{
		{"Type", "Link"}, {"Name", ""}, {"Exec", ""},
		{"Exec", "fixture-app %q"}, {"Exec", "fixture-app " + strings.Repeat("a", MaxValueBytes)},
	} {
		d := autostartDesktop(t, "")
		d.Groups["Desktop Entry"][test.key] = test.value
		launch, _, _ := PlanAutostart(d, "fixture.desktop", "/config/autostart/fixture.desktop", AutostartOptions{})
		if len(launch.Argv) != 0 {
			t.Fatal("accepted unusable entry", test.key, launch)
		}
	}
	for _, bus := range []bool{false, true} {
		d := autostartDesktop(t, "")
		delete(d.Groups["Desktop Entry"], "Exec")
		if bus {
			d.Groups["Desktop Entry"]["DBusActivatable"] = "true"
		}
		launch, reason, err := PlanAutostart(d, "fixture.desktop", "/config/autostart/fixture.desktop", AutostartOptions{})
		if len(launch.Argv) != 0 || err != nil || reason == "" {
			t.Fatal(launch, reason, err)
		}
	}
	// Hidden-only user overrides need not provide launch metadata.
	d, err = ParseDesktop([]byte("[Desktop Entry]\nHidden=true\n"))
	if err != nil {
		t.Fatal(err)
	}
	if launch, reason, err := PlanAutostart(d, "fixture.desktop", "/config/autostart/fixture.desktop", AutostartOptions{}); err != nil || len(launch.Argv) != 0 || reason == "" {
		t.Fatal(launch, reason, err)
	}
}

func TestAutostartScanValidation(t *testing.T) {
	valid := func() AutostartScan {
		return AutostartScan{Launches: []AutostartLaunch{{Filename: "a.desktop", Path: "/config/autostart/a.desktop", Name: "A", Argv: []string{"/bin/app", "literal;value"}}}}
	}
	if err := ValidateAutostartScan(valid()); err != nil {
		t.Fatal(err)
	}
	for _, change := range []func(*AutostartScan){
		func(s *AutostartScan) { s.Launches = append(s.Launches, s.Launches[0]) },
		func(s *AutostartScan) { s.Launches[0].Path = "relative" },
		func(s *AutostartScan) { s.Launches[0].Directory = "relative" },
		func(s *AutostartScan) { s.Launches[0].Argv = nil },
		func(s *AutostartScan) { s.Launches[0].Argv = []string{"app"} },
		func(s *AutostartScan) { s.Launches[0].Argv = []string{"/bin/app", "bad\x00"} },
		func(s *AutostartScan) { s.Diagnostics = []string{strings.Repeat("x", MaxAutostartDiagnosticBytes+1)} },
		func(s *AutostartScan) { s.Diagnostics = make([]string, MaxAutostartDiagnostics+1) },
		func(s *AutostartScan) { s.Launches = make([]AutostartLaunch, MaxAutostartEntries+1) },
		func(s *AutostartScan) {
			s.Launches[0].Argv = []string{"/bin/app", strings.Repeat("x", MaxAutostartOutputBytes)}
		},
		func(s *AutostartScan) {
			// JSON escaping can overflow the wire cap while string bytes fit.
			for i := 0; i < 8; i++ {
				s.Launches[0].Argv = append(s.Launches[0].Argv, strings.Repeat("\"", 32768))
			}
		},
	} {
		scan := valid()
		change(&scan)
		if err := ValidateAutostartScan(scan); err == nil {
			t.Fatal("accepted invalid worker output", scan)
		}
	}
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopworker

import (
	"context"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func xdgApplicationEnvironment(t *testing.T) Environment {
	t.Helper()
	for key, value := range map[string]string{"LC_ALL": "", "LC_MESSAGES": "", "LANG": "en_US.UTF-8", "LANGUAGE": "", "XDG_CURRENT_DESKTOP": "FVWM"} {
		t.Setenv(key, value)
	}
	env := NativeEnvironment()
	env.LookPath = func(name string) (string, error) {
		if name == "app" || name == "action" || name == "try app" {
			return name, nil
		}
		return "", errors.New("fixture executable absent")
	}
	return env
}

func writeXDGApplication(t *testing.T, directory, name, body string) string {
	t.Helper()
	path := filepath.Join(directory, name)
	if err := os.MkdirAll(filepath.Dir(path), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, []byte("[Desktop Entry]\nType=Application\n"+body), 0600); err != nil {
		t.Fatal(err)
	}
	return path
}

func TestDesktopApplicationsUseExactSharedExecGrammar(t *testing.T) {
	for _, test := range []struct {
		raw  string
		want []string
	}{
		{`app --name=%c --source=%k %d %D %n %N %v %m %F`, []string{"app", "--name=Localized %U", "--source=/tmp/source.desktop"}},
		{`app "a\\\\b" "\\$" "\\"" "" %%`, []string{"app", `a\b`, "$", `"`, "", "%"}},
	} {
		got, err := launcherExec(test.raw, "Localized %U", "/tmp/source.desktop", "")
		if err != nil || !reflect.DeepEqual(got, test.want) {
			t.Fatalf("literal XDG Exec %q: got %q, want %q, error %v", test.raw, got, test.want, err)
		}
	}
	for _, raw := range []string{`app 'single quote'`, `app partial"quote"`, `app "%c"`, `app %f %U`, `app café`, `app "\q"`, `app "$HOME"`, `app \$HOME`, `app %Z`} {
		if got, err := launcherExec(raw, "Name", "/tmp/source.desktop", "icon"); err == nil || got != nil {
			t.Fatalf("invalid XDG Exec accepted: %q => %q, %v", raw, got, err)
		}
	}
}

func TestDesktopApplicationsKeepLocalArgvAndByteCaps(t *testing.T) {
	for _, raw := range []string{"app " + strings.Repeat("x ", 4096), "app " + strings.Repeat("%c ", 33)} {
		if got, err := launcherExec(raw, strings.Repeat("x", 2048), "", ""); err == nil || got != nil {
			t.Fatal("local launcher output bound lost", len(got), err)
		}
	}
	got, err := launcherExec("app %c", strings.Repeat("x", CommandLimit-3), "", "")
	if err != nil || len(got) != 2 || len(got[0])+len(got[1]) != CommandLimit {
		t.Fatal("exact local byte boundary changed", len(got), err)
	}
}

func TestDesktopApplicationsAbsoluteDefaultsAndExplicitRoots(t *testing.T) {
	env := xdgApplicationEnvironment(t)
	root := t.TempDir()
	previous, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	if err := os.Chdir(root); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		if err := os.Chdir(previous); err != nil {
			t.Error(err)
		}
	})
	home, system := filepath.Join(root, "home"), filepath.Join(root, "system")
	t.Setenv("HOME", home)
	t.Setenv("XDG_DATA_HOME", "relative-user")
	t.Setenv("XDG_DATA_DIRS", "relative-system:"+system+":"+system+"/.:")
	user := filepath.Join(home, ".local", "share", "applications")
	writeXDGApplication(t, user, "same.desktop", "Name=User first\nExec=app\n")
	writeXDGApplication(t, filepath.Join(system, "applications"), "same.desktop", "Name=System duplicate\nExec=app\n")
	writeXDGApplication(t, filepath.Join(system, "applications"), "other.desktop", "Name=System other\nExec=app\n")
	writeXDGApplication(t, "relative-user/applications", "relative.desktop", "Name=Relative user\nExec=app\n")
	writeXDGApplication(t, "relative-system/applications", "relative.desktop", "Name=Relative system\nExec=app\n")
	got, err := applications(context.Background(), nil, env)
	if err != nil || len(got) != 2 || got[0].Name != "System other" || got[1].Name != "User first" {
		t.Fatal("relative environment roots or default precedence changed", got, err)
	}
	// Explicit request roots keep their existing literal/relative semantics.
	got, err = applications(context.Background(), []string{"relative-user/applications"}, env)
	if err != nil || len(got) != 1 || got[0].Name != "Relative user" {
		t.Fatal("explicit root was rewritten through XDG defaults", got, err)
	}
}

func TestDesktopApplicationsTypedLocalizedMetadataAndDeclaredActions(t *testing.T) {
	env := xdgApplicationEnvironment(t)
	t.Setenv("LANGUAGE", "fr_CA:fr")
	root, cwd := t.TempDir(), filepath.Join(t.TempDir(), "work space")
	if err := os.Mkdir(cwd, 0700); err != nil {
		t.Fatal(err)
	}
	body := "Name=Base\\sName\nName[fr]=Nom\\sBase\nIcon=parent\\sicon\nIcon[fr]=icône\\sparent\nTryExec=try\\sapp\nTerminal=true\nOnlyShowIn=FVWM;\nPath=" + strings.ReplaceAll(cwd, " ", `\s`) + "\nExec=app %i %c %k\nActions=Edit;Bad;Missing;Inherited;Absent;\n" +
		"[Desktop Action Edit]\nName=Edit\nName[fr]=Édition\\sprivée\nIcon=action\\sicon\nIcon[fr]=action\\sfrançaise\nExec=action %i %c %k\n" +
		"[Desktop Action Bad]\nName=Bad\nExec=action \"%c\"\n" +
		"[Desktop Action Inherited]\nName=Inherited\nName[fr]=Héritée\nExec=action %i %c %k\nTerminal=false\nPath=/not-the-parent\n" +
		"[Desktop Action Absent]\nName=Absent executable\nExec=absent\n" +
		"[Desktop Action Unlisted]\nName=Never declared\nExec=action\n"
	path := writeXDGApplication(t, root, "localized.desktop", body)
	got, err := applications(context.Background(), []string{root}, env)
	if err != nil || len(got) != 3 {
		t.Fatalf("localized parent/actions: %+v %v", got, err)
	}
	want := []Application{
		{Name: "Nom Base", Argv: []string{"app", "--icon", "icône parent", "Nom Base", path}, Cwd: &cwd, Terminal: true},
		{Name: "Nom Base — Héritée", Argv: []string{"action", "--icon", "icône parent", "Nom Base", path}, Cwd: &cwd, Terminal: true},
		{Name: "Nom Base — Édition privée", Argv: []string{"action", "--icon", "action française", "Nom Base", path}, Cwd: &cwd, Terminal: true},
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("typed/localized fields or parent action context changed: got %+v want %+v", got, want)
	}
	for _, locale := range []string{"C", "POSIX", "C.UTF-8"} {
		t.Setenv("LC_ALL", locale)
		base, err := applications(context.Background(), []string{root}, env)
		if err != nil || len(base) != 3 || base[0].Name != "Base Name" || base[0].Argv[2] != "parent icon" {
			t.Fatal("C locale did not suppress LANGUAGE", locale, base, err)
		}
	}
}

func TestDesktopApplicationsTypedRefusalsKeepTombstones(t *testing.T) {
	env := xdgApplicationEnvironment(t)
	user, system := t.TempDir(), t.TempDir()
	for name, body := range map[string]string{
		"hidden":     "Name=Hidden\nHidden=true\nExec=app\n",
		"terminal":   "Name=Bad terminal\nTerminal=TRUE\nExec=app\n",
		"display":    "Name=Bad display\nNoDisplay=TRUE\nExec=app\n",
		"name":       "Name[fr]=Missing base name\nExec=app\n",
		"escape":     "Name=Invalid\\qescape\nExec=app\n",
		"visibility": "Name=Conflicting visibility\nOnlyShowIn=FVWM;\nNotShowIn=Other;\nExec=app\n",
	} {
		writeXDGApplication(t, user, name+".desktop", body)
		writeXDGApplication(t, system, name+".desktop", "Name=Must stay shadowed\nExec=app\n")
	}
	writeXDGApplication(t, system, "valid.desktop", "Name=Valid\nExec=app\n")
	got, err := applications(context.Background(), []string{user, system}, env)
	if err != nil || len(got) != 1 || got[0].Name != "Valid" {
		t.Fatal("invalid high-priority IDs revealed lower entries", got, err)
	}
}

func TestDesktopApplicationsActionAndCombinedRecordBounds(t *testing.T) {
	env := xdgApplicationEnvironment(t)
	root := t.TempDir()
	var actions, groups strings.Builder
	for index := 0; index < 12; index++ {
		fmt.Fprintf(&actions, "Action%d;", index)
		fmt.Fprintf(&groups, "[Desktop Action Action%d]\nName=Action %02d\nExec=action\n", index, index)
	}
	body := "Name=App\nExec=app\nActions=" + actions.String() + "\n" + groups.String()
	writeXDGApplication(t, root, "one.desktop", body)
	got, err := applications(context.Background(), []string{root}, env)
	if err != nil || len(got) != 9 {
		t.Fatal("eight-action parent bound lost", len(got), err)
	}
	for _, app := range got {
		if strings.Contains(app.Name, "Action 08") || strings.Contains(app.Name, "Action 09") || strings.Contains(app.Name, "Action 10") || strings.Contains(app.Name, "Action 11") {
			t.Fatal("action outside bounded declared prefix included", app)
		}
	}
	for index := 0; index < 57; index++ {
		writeXDGApplication(t, root, fmt.Sprintf("%03d.desktop", index), body)
	}
	got, err = applications(context.Background(), []string{root}, env)
	if err != nil || len(got) != 512 {
		t.Fatal("actions bypassed combined application record cap", len(got), err)
	}
}

func TestDesktopApplicationsMalformedActionsPreserveValidParent(t *testing.T) {
	for _, body := range []string{
		"Actions=Bad\\q;\n[Desktop Action Bad]\nName=Bad\nExec=action\n",
		"Actions=Bad;\n[Desktop Action Bad]\nName=Bad\\qname\nExec=action\n",
		"Actions=Bad;\n[Desktop Action Bad]\nName=Bad\nIcon=bad\\qicon\nExec=action\n",
	} {
		env := xdgApplicationEnvironment(t)
		root := t.TempDir()
		writeXDGApplication(t, root, "malformed-action.desktop", "Name=Parent\nExec=app\n"+body)
		got, err := applications(context.Background(), []string{root}, env)
		if err != nil || len(got) != 1 || got[0].Name != "Parent" {
			t.Fatal("malformed action erased the valid launcher", got, err)
		}
	}
}

func TestDesktopApplicationsChargeActionOutputBytes(t *testing.T) {
	env := xdgApplicationEnvironment(t)
	root := t.TempDir()
	var actions, groups strings.Builder
	for index := 0; index < 8; index++ {
		fmt.Fprintf(&actions, "Action%d;", index)
		fmt.Fprintf(&groups, "[Desktop Action Action%d]\nName=Action %d\nExec=action\n", index, index)
	}
	writeXDGApplication(t, root, "bounded.desktop", "Name="+strings.Repeat("N", 20000)+"\nExec=app\nActions="+actions.String()+"\n"+groups.String())
	if got, err := applications(context.Background(), []string{root}, env); err == nil || got != nil || !strings.Contains(err.Error(), "application result limit") {
		t.Fatal("action labels bypassed result byte accounting", len(got), err)
	}
}

func TestDesktopApplicationsShareCurrentDesktopVisibility(t *testing.T) {
	env := xdgApplicationEnvironment(t)
	root := t.TempDir()
	writeXDGApplication(t, root, "other.desktop", "Name=Other\nOnlyShowIn=Other;\nExec=app\n")
	writeXDGApplication(t, root, "fvwm.desktop", "Name=FVWM\nOnlyShowIn=FVWM;\nExec=app\n")
	for value, want := range map[string][]string{"Other:Third": {"Other"}, "": {"FVWM"}, "Other:FVWM": {"FVWM", "Other"}} {
		t.Setenv("XDG_CURRENT_DESKTOP", value)
		got, err := applications(context.Background(), []string{root}, env)
		var names []string
		for _, app := range got {
			names = append(names, app.Name)
		}
		if err != nil || !reflect.DeepEqual(names, want) {
			t.Fatal(value, names, want, err)
		}
	}
}

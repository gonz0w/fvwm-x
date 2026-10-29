// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"context"
	"errors"
	"io"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"syscall"
	"testing"
	"time"
)

func TestRoleProviderPermissionInputs(t *testing.T) {
	if os.Geteuid() == 0 {
		if os.Getenv("FVWM_ROLE_PERMISSION_CHILD") == "1" {
			t.Fatal("permission oracle still has root identity")
		}
		// The root native runner uses only a child credential. Copying its
		// executable avoids inheriting inaccessible private Go cache parents.
		directory, err := os.MkdirTemp("/tmp", "fvwm-role-permission-")
		if err != nil {
			t.Fatal(err)
		}
		defer os.RemoveAll(directory)
		if err := os.Chmod(directory, 0755); err != nil {
			t.Fatal(err)
		}
		source, err := os.Open(os.Args[0])
		if err != nil {
			t.Fatal(err)
		}
		defer source.Close()
		binary := filepath.Join(directory, "role-permission-test")
		target, err := os.OpenFile(binary, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0755)
		if err != nil {
			t.Fatal(err)
		}
		_, copyErr := io.Copy(target, source)
		closeErr := target.Close()
		if copyErr != nil || closeErr != nil {
			t.Fatal(copyErr, closeErr)
		}
		if err := os.Chmod(binary, 0755); err != nil {
			t.Fatal(err)
		}
		ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
		defer cancel()
		command := exec.CommandContext(ctx, binary, "-test.run=^TestRoleProviderPermissionInputs$", "-test.v", "-test.timeout=20s")
		command.Dir = "/tmp"
		command.Env = append(os.Environ(), "FVWM_ROLE_PERMISSION_CHILD=1", "TMPDIR=/tmp", "GOTMPDIR=/tmp")
		command.SysProcAttr = &syscall.SysProcAttr{Credential: &syscall.Credential{
			Uid: 65534, Gid: 65534, Groups: []uint32{},
		}}
		output, err := command.CombinedOutput()
		if err != nil {
			t.Fatalf("unprivileged permission oracle: %v\n%s", err, output)
		}
		t.Logf("unprivileged child permission oracle passed:\n%s", output)
		return
	}
	for _, kind := range []string{"directory preferences", "file preferences", "aliases", "subclasses"} {
		t.Run(kind, func(t *testing.T) {
			o, config, user, system := directoryRoleFixture(t)
			mime, declared := "text/plain", "text/plain"
			if kind == "directory preferences" {
				mime, declared = "inode/directory", "inode/directory"
			}
			var inaccessible string
			switch kind {
			case "directory preferences", "file preferences":
				inaccessible = filepath.Join(config, "fvwm-mimeapps.list")
				writeFixture(t, filepath.Join(config, "mimeapps.list"), "[Default Applications]\n"+mime+"=good.desktop;\n")
			case "aliases":
				inaccessible = filepath.Join(filepath.Dir(user), "mime", "aliases")
				writeFixture(t, filepath.Join(filepath.Dir(system), "mime", "aliases"), "application/x-old text/plain\n")
				mime = "application/x-old"
			case "subclasses":
				inaccessible = filepath.Join(filepath.Dir(user), "mime", "subclasses")
				writeFixture(t, filepath.Join(filepath.Dir(system), "mime", "subclasses"), "application/x-child text/plain\n")
				mime = "application/x-child"
			}
			applicationEntry(t, user, "good.desktop", declared, "")
			writeFixture(t, inaccessible, "not parsed because this file is inaccessible\n")
			if err := os.Chmod(inaccessible, 0000); err != nil {
				t.Fatal(err)
			}
			defer os.Chmod(inaccessible, 0600)
			if _, err := os.ReadFile(inaccessible); !errors.Is(err, fs.ErrPermission) {
				t.Fatal("fixture did not establish a real permission refusal", os.Geteuid(), err)
			}
			var diagnostics []RoleDiagnostic
			o.Diagnostic = func(value RoleDiagnostic) { diagnostics = append(diagnostics, value) }
			ids, preferred, err := providerRoleIDs(t, o, mime)
			if err != nil || preferred != "good.desktop" || !reflect.DeepEqual(ids, []string{"good.desktop"}) {
				t.Fatal("inaccessible optional input blocked valid later provider", ids, preferred, err)
			}
			if len(diagnostics) != 1 || diagnostics[0].Code != RoleDiagnosticPermission || diagnostics[0].Path != inaccessible || diagnostics[0].Count != 1 {
				t.Fatal("permission refusal lost its diagnostic", diagnostics)
			}
			t.Logf("euid=%d real fs.ErrPermission confirmed; valid provider retained", os.Geteuid())
		})
	}
}

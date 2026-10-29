// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"context"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"syscall"
	"testing"
	"unicode/utf8"
)

func providerRoleIDs(t *testing.T, options DirectoryRoleOptions, mime string) ([]string, string, error) {
	t.Helper()
	if mime == "inode/directory" {
		result, err := New(nil).DirectoryRoles(options)
		ids := []string{}
		for _, handler := range result.Handlers {
			ids = append(ids, handler.ID)
		}
		return ids, result.DefaultID, err
	}
	result, err := New(nil).ApplicationRoles(mime, options)
	return applicationIDs(result), result.DefaultID, err
}

// MIME Applications section 4 tries successive desktop IDs until it finds a
// valid associated application. A malformed effective owner is ineligible; it
// must neither abort unrelated choices nor reveal a lower same-ID replacement.
func TestRoleProviderInvalidPreferredFallsBack(t *testing.T) {
	for _, mime := range []string{"inode/directory", "text/plain"} {
		for _, broken := range []struct {
			name, exec, extra string
		}{
			{name: "Exec grammar", exec: "app %Z"},
			{name: "typed boolean", exec: "app %u", extra: "Terminal=maybe\n"},
			{name: "duplicate key", exec: "app %u", extra: "Name=Duplicate\n"},
		} {
			t.Run(mime+"/"+broken.name, func(t *testing.T) {
				o, config, user, system := directoryRoleFixture(t)
				writeFixture(t, filepath.Join(user, "broken.desktop"), "[Desktop Entry]\nType=Application\nName=Broken\nExec="+broken.exec+"\nMimeType="+mime+";\n"+broken.extra)
				applicationEntry(t, system, "broken.desktop", mime, "")
				applicationEntry(t, user, "good.desktop", mime, "")
				writeFixture(t, filepath.Join(config, "mimeapps.list"), "[Default Applications]\n"+mime+"=broken.desktop;good.desktop;\n")
				ids, preferred, err := providerRoleIDs(t, o, mime)
				if err != nil || preferred != "good.desktop" || !reflect.DeepEqual(ids, []string{"good.desktop"}) {
					t.Fatal("invalid preferred owner blocked a valid different ID or exposed its lower replacement", ids, preferred, err)
				}
			})
		}
	}
}

func TestRoleProviderUnrelatedInvalidEntryIsIsolated(t *testing.T) {
	for _, mime := range []string{"inode/directory", "image/png"} {
		t.Run(mime, func(t *testing.T) {
			o, config, user, _ := directoryRoleFixture(t)
			applicationEntry(t, user, "good.desktop", mime, "")
			writeFixture(t, filepath.Join(user, "unrelated.desktop"), "[Desktop Entry]\nType=Application\nName=Unrelated\nExec=app %Z\nMimeType=application/x-unrelated;\n")
			writeFixture(t, filepath.Join(config, "mimeapps.list"), "[Default Applications]\n"+mime+"=good.desktop;\n")
			ids, preferred, err := providerRoleIDs(t, o, mime)
			if err != nil || preferred != "good.desktop" || !reflect.DeepEqual(ids, []string{"good.desktop"}) {
				t.Fatal("unrelated malformed entry broke role lookup", ids, preferred, err)
			}
		})
	}
}

func TestRoleProviderDiagnosticsAreBoundedAndOperationLocal(t *testing.T) {
	for _, mime := range []string{"inode/directory", "text/plain"} {
		t.Run(mime, func(t *testing.T) {
			o, _, user, _ := directoryRoleFixture(t)
			const invalidCount = 25
			for i := 0; i < invalidCount; i++ {
				writeFixture(t, filepath.Join(user, fmt.Sprintf("bad-%02d.desktop", i)), "[Desktop Entry]\nType=Application\nName=Bad\nExec=app %Z\nMimeType="+mime+";\n")
			}
			applicationEntry(t, user, "good.desktop", mime, "")
			var diagnostics []RoleDiagnostic
			o.Diagnostic = func(value RoleDiagnostic) { diagnostics = append(diagnostics, value) }
			for repeat := 0; repeat < 2; repeat++ {
				diagnostics = nil
				ids, preferred, err := providerRoleIDs(t, o, mime)
				if err != nil || preferred != "good.desktop" || !reflect.DeepEqual(ids, []string{"good.desktop"}) {
					t.Fatal("rejected entries blocked valid choice", ids, preferred, err)
				}
				if len(diagnostics) != 17 {
					t.Fatal("lookup must emit16 details and one omission summary", diagnostics)
				}
				for _, value := range diagnostics[:16] {
					if value.Code != RoleDiagnosticInvalidEntry || value.Count != 1 || value.Path == "" || value.Message == "" {
						t.Fatal("missing rejected-entry detail", value)
					}
				}
				if last := diagnostics[16]; last.Code != RoleDiagnosticSuppressed || last.Count != invalidCount-16 || last.Path != "" {
					t.Fatal("omission count is not operation-local", last)
				}
			}
		})
	}
}

func TestRoleDiagnosticTextIsBoundedAndQuoted(t *testing.T) {
	value := RoleDiagnostic{Code: RoleDiagnosticInvalidEntry,
		Path:    strings.Repeat("\u00e9", 400) + "\n\x00",
		Message: "bad\n\x00\"" + strings.Repeat("\u00e9", 300), Count: 1}
	line := value.String()
	if !utf8.ValidString(line) || strings.ContainsAny(line, "\n\r\x00") ||
		len(line) > 6*(512+256)+128 || !strings.Contains(line, `\n`) || !strings.Contains(line, `\x00`) {
		t.Fatal("diagnostic escaped its bounded single-line representation", len(line), line)
	}
}

func TestRoleProviderOptionalPermissionPolicyKeepsFatalControls(t *testing.T) {
	for _, mime := range []string{"inode/directory", "text/plain"} {
		t.Run(mime, func(t *testing.T) {
			o, config, user, _ := directoryRoleFixture(t)
			applicationEntry(t, user, "good.desktop", mime, "")
			writeFixture(t, filepath.Join(config, "mimeapps.list"), "[Default Applications]\n"+mime+"=../bad.desktop;\n")
			if ids, _, err := providerRoleIDs(t, o, mime); err == nil || len(ids) != 0 {
				t.Fatal("malformed preferences became an optional input", ids, err)
			}
			ctx, cancel := context.WithCancel(context.Background())
			cancel()
			source := New(ctx)
			if mime == "inode/directory" {
				if _, err := source.DirectoryRoles(o); !errors.Is(err, context.Canceled) {
					t.Fatal("cancellation was swallowed", err)
				}
			} else if _, err := source.ApplicationRoles(mime, o); !errors.Is(err, context.Canceled) {
				t.Fatal("cancellation was swallowed", err)
			}
		})
	}
}

func TestRoleProviderDiagnosticCannotHideCancellation(t *testing.T) {
	o, _, user, _ := directoryRoleFixture(t)
	writeFixture(t, filepath.Join(user, "bad.desktop"), "[Desktop Entry]\nType=Application\nName=Bad\nExec=app %Z\nMimeType=inode/directory;\n")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	o.Diagnostic = func(RoleDiagnostic) { cancel() }
	if result, err := New(ctx).DirectoryRoles(o); !errors.Is(err, context.Canceled) || len(result.Handlers) != 0 {
		t.Fatal("a rejected last entry converted cancellation into success", result, err)
	}
}

func TestRoleProviderEntryAcquisitionFailuresRemainFatal(t *testing.T) {
	for _, mime := range []string{"inode/directory", "text/plain"} {
		for _, failure := range []string{"source cap", "working directory loop"} {
			t.Run(mime+"/"+failure, func(t *testing.T) {
				o, _, user, _ := directoryRoleFixture(t)
				applicationEntry(t, user, "good.desktop", mime, "")
				var expected error
				if failure == "source cap" {
					writeFixture(t, filepath.Join(user, "bad.desktop"), strings.Repeat("#", MaxDirectoryRoleSourceBytes+1))
				} else {
					loop := filepath.Join(o.Home, "loop")
					if err := os.Symlink("loop", loop); err != nil {
						t.Fatal(err)
					}
					applicationEntry(t, user, "bad.desktop", mime, "Path="+loop+"\n")
					expected = syscall.ELOOP
				}
				ids, _, err := providerRoleIDs(t, o, mime)
				if err == nil || len(ids) != 0 || expected != nil && !errors.Is(err, expected) {
					t.Fatal("acquisition failure became an ineligible entry", ids, err)
				}
			})
		}
	}
}

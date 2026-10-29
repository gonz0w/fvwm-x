// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import "testing"

func TestRoleStartupMetadataIsCapturedAndRevalidated(t *testing.T) {
	o, _, user, _ := directoryRoleFixture(t)
	roleEntry(t, user, "app.desktop", "StartupNotify=true\nStartupWMClass=RoleClass\n")
	first, err := New(nil).DirectoryRoles(o)
	if err != nil || len(first.Handlers) != 1 || first.Handlers[0].Startup == nil || first.Handlers[0].Startup.Name != "Dossier" || first.Handlers[0].Startup.ApplicationID != "app.desktop" {
		t.Fatal(first, err)
	}
	roleEntry(t, user, "app.desktop", "StartupNotify=false\nStartupWMClass=RoleClass\n")
	fresh, err := New(nil).DirectoryRoles(o)
	if err != nil || fresh.Handlers[0].Fingerprint == first.Handlers[0].Fingerprint || fresh.Handlers[0].Startup.Notify == nil || *fresh.Handlers[0].Startup.Notify {
		t.Fatal("startup metadata changed without invalidating captured choice", fresh, err)
	}
}

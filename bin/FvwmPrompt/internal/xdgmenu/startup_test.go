// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgmenu

import (
	"strings"
	"testing"
)

func TestStartupMetadataTriStateAndBoundedFallback(t *testing.T) {
	for _, test := range []struct {
		body             string
		enabled, failure bool
	}{
		{"", false, false},
		{"StartupNotify=true\n", true, false},
		{"StartupNotify=false\nStartupWMClass=LegacyApp\n", false, false},
		{"StartupWMClass=LegacyApp\n", true, false},
		{"StartupWMClass=bad\\nclass\n", false, false},
		{"StartupNotify=true\nStartupWMClass=bad\\nclass\n", true, false},
		{"StartupNotify=TRUE\n", false, true},
	} {
		entry := parsedDesktop(t, "Name=App\nExec=app\n"+test.body)
		metadata, err := ParseStartupMetadata(entry, "Nom localisé", "icône", "app.desktop")
		if (err != nil) != test.failure || StartupEnabled(metadata) != test.enabled {
			t.Fatal(test, metadata, err)
		}
		if test.enabled && (metadata.Name != "Nom localisé" || metadata.Icon != "icône" || metadata.ApplicationID != "app.desktop") {
			t.Fatal("localized metadata was lost", metadata)
		}
	}
	entry := parsedDesktop(t, "Name=App\nStartupNotify=true\nExec=app\n")
	for _, name := range []string{strings.Repeat("é", 128), "invalid\xff", ""} {
		metadata, err := ParseStartupMetadata(entry, name, "icon", "app.desktop")
		if err != nil || StartupEnabled(metadata) {
			t.Fatal("unannounceable metadata must retain ordinary launch", metadata, err)
		}
	}
}

func TestMenuStartupMetadataIncludesParentIdentityForActions(t *testing.T) {
	entry := parsedDesktop(t, "Name=Parent\nExec=app\nStartupNotify=true\nStartupWMClass=ParentClass\nActions=One;\n[Desktop Action One]\nName=Action\nExec=action\nStartupNotify=false\nStartupWMClass=Wrong\n")
	got := presentationFixture(t, `<Include><All/></Include>`, map[string]*Desktop{"parent.desktop": entry}, PresentationOptions{})
	if len(got.Items) != 2 {
		t.Fatal(got)
	}
	for _, item := range got.Items {
		if !StartupEnabled(item.Startup) || item.Startup.ApplicationID != "parent.desktop" || item.Startup.WMClass != "ParentClass" || item.Startup.Name != item.Label {
			t.Fatal("action overrode parent startup policy", item)
		}
	}
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
package iconrender

import (
	"bytes"
	"image/color"
	"os"
	"path/filepath"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

func TestFreshAcquisitionSeesPreviouslyMissingTheme(t *testing.T) {
	dir := t.TempDir()
	request := xdgmenu.IconRequest{Theme: "fvwm-lifecycle-fixture",
		Name: "fvwm-lifecycle-missing", Size: 16, ScaleQuarters: 4}
	old := newTestRenderer(t, dir, []string{"png"})
	if got, err := old.Resolve(request); err != nil || got != "" {
		t.Fatal("initial missing theme", got, err)
	}
	root := filepath.Join(dir, ".icons", request.Theme)
	writeFixture(t, filepath.Join(root, "index.theme"), []byte(
		"[Icon Theme]\nName=Fixture\nDirectories=16x16/apps\n"+
			"[16x16/apps]\nSize=16\nType=Fixed\n"))
	writeFixture(t, filepath.Join(root, "16x16/apps", request.Name+".png"),
		fixturePNG(t, 16, color.NRGBA{R: 255, A: 255}))
	if got, err := old.Resolve(request); err != nil || got != "" {
		t.Fatal("cached missing result changed within acquisition", got, err)
	}
	fresh := newTestRenderer(t, dir, []string{"png"})
	if got, err := fresh.Resolve(request); err != nil || got == "" {
		t.Fatal("fresh acquisition retained missing theme", got, err)
	}
}

func TestFreshAcquisitionSeesChangedInheritanceAndRetainsExports(t *testing.T) {
	dir := t.TempDir()
	root := filepath.Join(dir, ".icons")
	index := func(parent string) []byte {
		return []byte("[Icon Theme]\nName=Fixture\nInherits=" + parent +
			"\nDirectories=16x16/apps\n[16x16/apps]\nSize=16\nType=Fixed\n")
	}
	writeFixture(t, filepath.Join(root, "child/index.theme"), index("first"))
	for name, pixel := range map[string]color.NRGBA{
		"first": {R: 255, A: 255}, "second": {B: 255, A: 255}} {
		writeFixture(t, filepath.Join(root, name, "index.theme"), index(""))
		writeFixture(t, filepath.Join(root, name, "16x16/apps/fvwm-lifecycle.png"),
			fixturePNG(t, 16, pixel))
	}
	request := xdgmenu.IconRequest{Theme: "child", Name: "fvwm-lifecycle",
		Size: 16, ScaleQuarters: 4}
	old := newTestRenderer(t, dir, []string{"png"})
	before, err := old.Resolve(request)
	if err != nil || before == "" {
		t.Fatal("initial inherited icon", before, err)
	}
	retained, err := os.ReadFile(before)
	if err != nil {
		t.Fatal(err)
	}
	writeFixture(t, filepath.Join(root, "child/index.theme"), index("second"))
	if got, err := old.Resolve(request); err != nil || got != before {
		t.Fatal("cached request changed within acquisition", got, err)
	}
	fresh := newTestRenderer(t, dir, []string{"png"})
	after, err := fresh.Resolve(request)
	if err != nil || after == "" || after == before {
		t.Fatal("fresh acquisition retained old inheritance", after, err)
	}
	if data, err := os.ReadFile(before); err != nil || !bytes.Equal(data, retained) {
		t.Fatal("regeneration changed referenced export", err)
	}
	if data, err := os.ReadFile(after); err != nil || bytes.Equal(data, retained) {
		t.Fatal("fresh content did not change", err)
	}
}

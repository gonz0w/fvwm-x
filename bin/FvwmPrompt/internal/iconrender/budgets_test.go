// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
package iconrender

import (
	"image/color"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
)

func TestConsumerBudgetsAndPredecodeGeometry(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "large.png")
	writeFixture(t, path, fixturePNG(t, 257, color.NRGBA{R: 255, A: 255}))
	source, err := xdgsource.NewLimited(nil, 8*1024*1024, 2048)
	if err != nil {
		t.Fatal(err)
	}
	renderer, err := New(source, xdgmenu.SearchPaths{}, dir, Config{CacheDir: filepath.Join(dir, "output"), Extensions: []string{"png"}, OutputBytes: 2 * 1024 * 1024, SourcePixels: 256 * 256})
	if err != nil {
		t.Fatal(err)
	}
	if _, err = renderer.Resolve(xdgmenu.IconRequest{Name: path, Size: 24, ScaleQuarters: 4}); err == nil || !strings.Contains(err.Error(), "geometry") {
		t.Fatal("decoded oversized source", err)
	}
	for _, config := range []Config{{CacheDir: dir, CacheBytes: -1}, {CacheDir: dir, OutputBytes: xdgsource.MaxJobBytes + 1}, {CacheDir: dir, SourcePixels: -1}, {CacheDir: dir, ExtraRoots: []string{"relative"}}, {CacheDir: dir, ExtraRoots: []string{"/tmp/../other"}}, {CacheDir: dir, ExtraRoots: []string{"/tmp/\xff"}}} {
		if _, err := New(xdgsource.New(nil), xdgmenu.SearchPaths{}, dir, config); err == nil {
			t.Fatal("accepted invalid consumer configuration", config)
		}
	}
	legacy := newTestRenderer(t, dir, []string{"png"})
	if legacy.sourcePixels != 64*1024*1024 || legacy.outputBytes != xdgsource.MaxJobBytes {
		t.Fatal("legacy defaults changed")
	}
}

func TestSmallerPublicationBudgetAndXPMAdmission(t *testing.T) {
	dir := t.TempDir()
	pngPath := filepath.Join(dir, "normal.png")
	writeFixture(t, pngPath, fixturePNG(t, 24, color.NRGBA{R: 255, A: 255}))
	renderer, err := New(xdgsource.New(nil), xdgmenu.SearchPaths{}, dir, Config{CacheDir: filepath.Join(dir, "out"), Extensions: []string{"png"}, OutputBytes: 1, SourcePixels: 256 * 256})
	if err != nil {
		t.Fatal(err)
	}
	if _, err := renderer.Resolve(xdgmenu.IconRequest{Name: pngPath, Size: 24, ScaleQuarters: 4}); err == nil || !strings.Contains(err.Error(), "publication") {
		t.Fatal(err)
	}
	xpmPath := filepath.Join(dir, "huge.xpm")
	writeFixture(t, xpmPath, []byte("/* XPM */\nstatic char *x[] = {\"32768 32768 1 1\"};\n"))
	renderer, err = New(xdgsource.New(nil), xdgmenu.SearchPaths{}, dir, Config{CacheDir: filepath.Join(dir, "out"), Extensions: []string{"xpm"}, SourcePixels: 256 * 256})
	if err != nil {
		t.Fatal(err)
	}
	if _, err := renderer.Resolve(xdgmenu.IconRequest{Name: xpmPath, Size: 24, ScaleQuarters: 4}); err == nil || !strings.Contains(err.Error(), "geometry") {
		t.Fatal(err)
	}
}

func TestLimitedXPMBoundsColorAndCharacterWork(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "input.xpm")
	for _, header := range []string{"1 1 4294967295 1", "1 1 4294967296 1", "1 1 1 4294967295", "1 1 0 1", "1 1 1 0", "1 1 1 9", "1 1 2 1"} {
		writeFixture(t, path, []byte("/* XPM */\nstatic char *x[] = {\""+header+"\"};\n"))
		renderer, err := New(xdgsource.New(nil), xdgmenu.SearchPaths{}, dir, Config{CacheDir: filepath.Join(dir, "out"), Extensions: []string{"xpm"}, SourcePixels: 256 * 256})
		if err != nil {
			t.Fatal(err)
		}
		if _, err := renderer.Resolve(xdgmenu.IconRequest{Name: path, Size: 24, ScaleQuarters: 4}); err == nil {
			t.Fatal("unbounded native XPM metadata accepted", header)
		}
		if _, err := os.Stat(renderer.dir); !os.IsNotExist(err) {
			t.Fatal("rejected XPM metadata published an artifact", err)
		}
	}
}

func TestLimitedXPMFindsActualHeaderAfterComments(t *testing.T) {
	for _, source := range []string{
		"/* XPM */\n/* fake \"300 300 1 1\" { } */\nstatic char *x[] = {\"1 1 1 1\",\". c #fff\",\".\"};\n",
		"/* XPM */\nstatic /* \"fake\" */ const char *x[] /* dimensions */ = /* before brace */ { /* before data \"300 300\" */ \"1 1 1 1\",\". c #fff\",\".\"};\n",
	} {
		header, err := limitedXPMHeader([]byte(source))
		if err != nil || string(header) != "1 1 1 1" {
			t.Fatal("did not select actual first data string", string(header), err)
		}
		dir := t.TempDir()
		path := filepath.Join(dir, "comments.xpm")
		writeFixture(t, path, []byte(source))
		renderer, err := New(xdgsource.New(nil), xdgmenu.SearchPaths{}, dir, Config{CacheDir: filepath.Join(dir, "out"), Extensions: []string{"xpm"}, SourcePixels: 256 * 256})
		if err != nil {
			t.Fatal(err)
		}
		resolved, err := renderer.Resolve(xdgmenu.IconRequest{Name: path, Size: 24, ScaleQuarters: 4})
		if err != nil || resolved == "" {
			t.Fatal("refused valid commented XPM", resolved, err)
		}
		published, err := os.ReadFile(resolved)
		if err != nil || string(published) != source {
			t.Fatal("publication changed admitted XPM bytes", err)
		}
	}
}

func TestLimitedXPMRejectsCommentedFakeSmallHeader(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "fake-small.xpm")
	writeFixture(t, path, []byte("/* XPM */\n/* \"1 1 1 1\" */\nstatic char *x[] = {\"300 300 1 1\"};\n"))
	renderer, err := New(xdgsource.New(nil), xdgmenu.SearchPaths{}, dir, Config{CacheDir: filepath.Join(dir, "out"), Extensions: []string{"xpm"}, SourcePixels: 256 * 256})
	if err != nil {
		t.Fatal(err)
	}
	if _, err := renderer.Resolve(xdgmenu.IconRequest{Name: path, Size: 24, ScaleQuarters: 4}); err == nil || !strings.Contains(err.Error(), "geometry") {
		t.Fatal("comment disguised oversized actual data header", err)
	}
	if _, err := os.Stat(renderer.dir); !os.IsNotExist(err) {
		t.Fatal("rejected oversized XPM reached native publication", err)
	}
}

func TestLimitedXPMRefusesAmbiguousHeaderSyntax(t *testing.T) {
	for name, source := range map[string]string{
		"unterminated-comment": "/* XPM */ /* \"1 1 1 1\"",
		"nested-comment":       "/* XPM */ /* nested /* comment */ static char *x[] = {\"1 1 1 1\"};",
		"line-comment":         "/* XPM */ // \"1 1 1 1\"\nstatic char *x[] = {\"300 300 1 1\"};",
		"escaped-digit":        "/* XPM */ static char *x[] = {\"1 1 1 \\x31\"};",
		"escaped-quote":        "/* XPM */ static char *x[] = {\"1 1 1 1\\\"\"};",
		"unterminated-string":  "/* XPM */ static char *x[] = {\"1 1 1 1",
		"concatenated-string":  "/* XPM */ static char *x[] = {\"1 1 1 1\" \"300 300\"};",
		"character-prefix":     "/* XPM */ static char *x[] = {'\"', \"300 300 1 1\"};",
		"preprocessor-prefix":  "/* XPM */\n#define fake \"1 1 1 1\"\nstatic char *x[] = {\"300 300 1 1\"};",
		"nul-comment":          "/* XPM */ /* \x00 */ static char *x[] = {\"1 1 1 1\"};",
		"comment-after-header": "/* XPM */ static char *x[] = {\"1 1 1 1\" /* unfinished",
	} {
		t.Run(name, func(t *testing.T) {
			if _, err := limitedXPMHeader([]byte(source)); err == nil {
				t.Fatal("ambiguous limited XPM header admitted")
			}
		})
	}
	for _, length := range []int{128, 129} {
		header := "1 1 1 1" + strings.Repeat(" ", length-len("1 1 1 1"))
		source := []byte("/* XPM */ static char *x[] = {\"" + header + "\"};")
		_, err := limitedXPMHeader(source)
		if (err == nil) != (length == 128) {
			t.Fatal("changed geometry string byte bound", length, err)
		}
	}
}

func TestLimitedXPMCharacterBoundaryAndUnlimitedPath(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "eight.xpm")
	writeFixture(t, path, []byte("/* XPM */ static char *x[] = {\"1 1 1 8\",\"........ c #fff\",\"........\"};\n"))
	renderer, err := New(xdgsource.New(nil), xdgmenu.SearchPaths{}, dir, Config{CacheDir: filepath.Join(dir, "out"), Extensions: []string{"xpm"}, SourcePixels: 256 * 256})
	if err != nil {
		t.Fatal(err)
	}
	if resolved, err := renderer.Resolve(xdgmenu.IconRequest{Name: path, Size: 24, ScaleQuarters: 4}); err != nil || resolved == "" {
		t.Fatal("refused bounded eight-character XPM", resolved, err)
	}
	// Legacy consumers still publish unchanged bytes for their native loader;
	// the limited lexical admission policy must not become a shared default.
	path = filepath.Join(dir, "legacy.xpm")
	writeFixture(t, path, []byte("/* XPM */ static char *x[] = {\"1 1 1 \\x31\"};\n"))
	legacy := newTestRenderer(t, dir, []string{"xpm"})
	if resolved, err := legacy.Resolve(xdgmenu.IconRequest{Name: path, Size: 24, ScaleQuarters: 4}); err != nil || resolved == "" {
		t.Fatal("limited admission changed unlimited publication", resolved, err)
	}
}

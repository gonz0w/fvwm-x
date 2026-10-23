package filemodel

import (
	"bytes"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

func TestLocalTargetURIDecodesExactlyOnce(t *testing.T) {
	for _, check := range []struct{ input, path string }{
		{"/tmp/literal%FF?#", "/tmp/literal%FF?#"},
		{"file:///tmp/a%20b%23%3F%25", "/tmp/a b#?%"},
		{"file://LOCALHOST/tmp/%FF", "/tmp/\xff"},
		{"file:/tmp/%252F", "/tmp/%2F"},
		{"FILE:///tmp/a", "/tmp/a"},
	} {
		path, err := ParseTarget(check.input)
		if err != nil || !bytes.Equal(path, []byte(check.path)) {
			t.Fatal(check.input, path, err)
		}
	}
	for _, input := range []string{"relative", "http://localhost/path", "file://remote/path", "file://localhoſt/path", "file://user@localhost/path", "file://localhost:80/path", "file:relative", "file:///tmp/%ZZ", "file:///tmp/%00", "file:///tmp/?", "file:///tmp/#", "file://localhost", "/tmp/\x00", "/" + strings.Repeat("a", MaxPath), strings.Repeat("x", MaxTarget+1)} {
		if _, err := ParseTarget(input); err == nil {
			t.Fatal("accepted", input)
		}
	}
}

func TestLocatePublishesAllTargetsOrNone(t *testing.T) {
	root := t.TempDir()
	for _, name := range []string{"a", "z", ".hidden", "literal ' \" $[x];\n"} {
		if err := os.WriteFile(filepath.Join(root, name), nil, 0600); err != nil {
			t.Fatal(err)
		}
	}
	a, z := []byte(filepath.Join(root, "a")), []byte(filepath.Join(root, "z"))
	snapshot, ids, err := Locate([][]byte{z, []byte(filepath.Join(root, ".hidden")), a}, false)
	if err != nil || !snapshot.Hidden || len(ids) != 3 {
		t.Fatal(snapshot, ids, err)
	}
	for i, name := range []string{"z", ".hidden", "a"} {
		found := false
		for _, item := range snapshot.Items {
			if item.ID == ids[i] && string(item.Name) == name {
				found = true
			}
		}
		if !found {
			t.Fatal("requested order/identity lost", name)
		}
	}
	for _, targets := range [][][]byte{nil, {a, a}, {a, []byte(filepath.Join(root, "missing"))}, {a, []byte(filepath.Join(root, "other", "file"))}, {[]byte(root + "/../a")}, {[]byte("/")}, make([][]byte, 257)} {
		s, selected, err := Locate(targets, false)
		if err == nil || s != nil || selected != nil {
			t.Fatal("partial or invalid reveal", s, selected, err)
		}
	}
	if runtime.GOOS != "darwin" {
		raw := []byte(filepath.Join(root, "raw-"))
		raw = append(raw, 0xff)
		if err := os.WriteFile(string(raw), nil, 0600); err != nil {
			t.Fatal(err)
		}
		s, selected, err := Locate([][]byte{raw}, false)
		if err != nil || len(selected) != 1 {
			t.Fatal(err)
		}
		found := false
		for _, item := range s.Items {
			if item.ID == selected[0] && bytes.Equal(item.Name, []byte{'r', 'a', 'w', '-', 0xff}) {
				found = true
			}
		}
		if !found {
			t.Fatal("non-UTF8 target normalized")
		}
	}
}

func TestLocateObservesFinalSymlinkWithoutOpening(t *testing.T) {
	root := t.TempDir()
	target := filepath.Join(root, "link")
	if err := os.Symlink(filepath.Join(root, "absent"), target); err != nil {
		t.Fatal(err)
	}
	s, ids, err := Locate([][]byte{[]byte(target)}, false)
	if err != nil || len(ids) != 1 || s.Items[0].Kind != "symlink" {
		t.Fatal(s, ids, err)
	}
	if _, _, err := s.Validate(ids[0]); err == nil {
		t.Fatal("observation made symlink actionable")
	}
}

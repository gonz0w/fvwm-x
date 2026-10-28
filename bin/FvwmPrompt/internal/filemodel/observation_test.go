package filemodel

import (
	"bytes"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"syscall"
	"testing"
)

func TestSingleTargetDoesNotScanSiblingsAndSupportsRoot(t *testing.T) {
	dir := t.TempDir()
	for i := 0; i < 4097; i++ {
		if err := os.WriteFile(filepath.Join(dir, fmt.Sprint(i)), nil, 0600); err != nil {
			t.Fatal(err)
		}
	}
	for _, path := range []string{"/", filepath.Join(dir, "0")} {
		o, err := ObserveTarget([]byte(path))
		if err != nil || o.Status != "present" {
			t.Fatal(o, err)
		}
		if got, _, err := o.Validate(); err != nil || got != path {
			t.Fatal(got, err)
		}
	}
}

func TestSingleTargetLiteralMissingSymlinkAndStale(t *testing.T) {
	dir := t.TempDir()
	name := "literal\xff ' $[x];\n"
	if runtime.GOOS == "darwin" {
		name = "literal ' $[x];\n"
	}
	path := []byte(filepath.Join(dir, name))
	if err := os.WriteFile(string(path), []byte("old"), 0600); err != nil {
		t.Fatal(err)
	}
	o, err := ObserveTarget(path)
	if err != nil || !bytes.Equal(o.Path, path) || o.Status != "present" {
		t.Fatal(o, err)
	}
	if err := os.WriteFile(string(path), []byte("changed"), 0600); err != nil {
		t.Fatal(err)
	}
	if _, _, err := o.Validate(); err == nil {
		t.Fatal("stale observation acted")
	}
	missing := filepath.Join(dir, "absent")
	o, err = ObserveTarget([]byte(missing))
	if err != nil || o.Status != "missing" {
		t.Fatal(o, err)
	}
	link := filepath.Join(dir, "link")
	if err := os.Symlink(missing, link); err != nil {
		t.Fatal(err)
	}
	o, err = ObserveTarget([]byte(link))
	if err != nil || o.Kind != "symlink" || o.Status != "present" {
		t.Fatal(o, err)
	}
	if _, _, err := o.Validate(); err == nil {
		t.Fatal("symlink actionable")
	}
	if err := os.Remove(link); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(dir, link); err != nil {
		t.Fatal(err)
	}
	o, err = ObserveTarget([]byte(link + "/"))
	if err != nil || o.Kind != "symlink" {
		t.Fatal("trailing slash followed final symlink", o, err)
	}
	if _, _, err := o.Validate(); err == nil {
		t.Fatal("directory symlink actionable")
	}
	fifo := filepath.Join(dir, "fifo")
	if err := syscall.Mkfifo(fifo, 0600); err != nil {
		t.Fatal(err)
	}
	o, err = ObserveTarget([]byte(fifo))
	if err != nil || o.Kind != "special" {
		t.Fatal(o, err)
	}
	if _, _, err := o.Validate(); err == nil {
		t.Fatal("special file actionable")
	}
	o, err = ObserveTarget([]byte(string(path) + "/child"))
	if err != nil || o.Status != "unavailable" {
		t.Fatal(o, err)
	}
	if _, err := ObserveTarget([]byte("relative")); err == nil {
		t.Fatal("relative accepted")
	}
}

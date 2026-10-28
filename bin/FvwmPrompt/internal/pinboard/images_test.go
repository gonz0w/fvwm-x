package pinboard

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestImageRetirementIsOwnedBoundedAndValidatesBeforeUnlink(t *testing.T) {
	owner := strings.Repeat("a", 32)
	cache := filepath.Join(t.TempDir(), "fvwm-pinboard-"+owner, "g-1")
	if err := PrepareImages(cache, owner); err != nil {
		t.Fatal(err)
	}
	image := filepath.Join(cache, ".fvwm-xdg-"+strings.Repeat("b", 64)+".png")
	if err := os.WriteFile(image, []byte("image"), 0600); err != nil {
		t.Fatal(err)
	}
	foreign := filepath.Join(cache, "user-document")
	if err := os.WriteFile(foreign, []byte("keep"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := RetireImages(cache, owner); err == nil {
		t.Fatal("foreign file admitted")
	}
	if _, err := os.Stat(image); err != nil {
		t.Fatal("partial directory retirement", err)
	}
	if err := os.Remove(foreign); err != nil {
		t.Fatal(err)
	}
	if err := RetireImages(cache, strings.Repeat("c", 32)); err == nil {
		t.Fatal("foreign owner retired images")
	}
	file, err := os.OpenFile(image, os.O_RDWR, 0600)
	if err != nil {
		t.Fatal(err)
	}
	if err := file.Truncate(MaxImageBytes + 1); err != nil {
		t.Fatal(err)
	}
	file.Close()
	if err := CheckImages(cache, owner); err == nil {
		t.Fatal("oversized generation admitted")
	}
	if err := RetireImages(cache, owner); err != nil {
		t.Fatal("obsolete oversized generation cannot retire", err)
	}
	if _, err := os.Stat(cache); !os.IsNotExist(err) {
		t.Fatal("image generation remains", err)
	}
}

func TestImageGenerationRefusesSymlinkDirectory(t *testing.T) {
	owner := strings.Repeat("a", 32)
	root := filepath.Join(t.TempDir(), "fvwm-pinboard-"+owner)
	if err := os.Mkdir(root, 0700); err != nil {
		t.Fatal(err)
	}
	real := t.TempDir()
	if err := os.Chmod(real, 0700); err != nil {
		t.Fatal(err)
	}
	cache := filepath.Join(root, "g-1")
	if err := os.Symlink(real, cache); err != nil {
		t.Fatal(err)
	}
	if err := PrepareImages(cache, owner); err == nil {
		t.Fatal("symlink image generation admitted")
	}
	if err := RetireImages(cache, owner); err == nil {
		t.Fatal("symlink image generation retired")
	}
}

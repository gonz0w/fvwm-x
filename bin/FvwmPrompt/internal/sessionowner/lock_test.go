// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"os"
	"path/filepath"
	"testing"

	"golang.org/x/sys/unix"
)

func TestDisplayLockIsExclusivePersistentAndPerDisplay(t *testing.T) {
	parent := t.TempDir()
	info, err := os.Lstat(parent)
	if err != nil {
		t.Fatal(err)
	}
	var status unix.Stat_t
	if err := unix.Lstat(parent, &status); err != nil {
		t.Fatal(err)
	}
	t.Logf("TempDir path=%q mode=%#o uid=%d euid=%d", parent, info.Mode().Perm(), status.Uid, os.Geteuid())
	// TempDir's numbered child follows the process umask. The runtime-directory
	// contract requires an explicitly private, owned leaf, regardless of umask.
	directory := filepath.Join(parent, "runtime")
	if err := os.Mkdir(directory, 0700); err != nil {
		t.Fatal(err)
	}
	env := []string{"DISPLAY=:19", "XDG_RUNTIME_DIR=" + directory}
	first, err := acquireLock(env)
	if err != nil {
		t.Fatal(err)
	}
	if second, err := acquireLock(env); err == nil {
		second.Close()
		t.Fatal("duplicate owner admitted")
	}
	other, err := acquireLock([]string{"DISPLAY=:20", "XDG_RUNTIME_DIR=" + directory})
	if err != nil {
		t.Fatal(err)
	}
	other.Close()
	before, err := os.ReadDir(directory)
	if err != nil {
		t.Fatal(err)
	}
	first.Close()
	again, err := acquireLock(env)
	if err != nil {
		t.Fatal(err)
	}
	again.Close()
	after, err := os.ReadDir(directory)
	if err != nil {
		t.Fatal(err)
	}
	if len(before) != 2 || len(after) != 2 || before[0].Name() != after[0].Name() {
		t.Fatalf("lock inode names changed: %v %v", before, after)
	}
}

func TestFirstLoginCreatesOnlyPrivateCacheLeaves(t *testing.T) {
	home := t.TempDir()
	lock, err := acquireLock([]string{"DISPLAY=:19", "HOME=" + home})
	if err != nil {
		t.Fatal(err)
	}
	lock.Close()
	for _, leaf := range []string{".cache", ".cache/fvwm3", ".cache/fvwm3/session-owner"} {
		info, err := os.Lstat(filepath.Join(home, leaf))
		if err != nil || !info.IsDir() || info.Mode().Perm() != 0700 {
			t.Fatalf("%s: %v %v", leaf, info, err)
		}
	}
}

func TestOwnerStorageRefusesUnsafeLeaves(t *testing.T) {
	for _, mode := range []string{"public", "symlink", "lock-symlink", "lock-hardlink"} {
		t.Run(mode, func(t *testing.T) {
			home := t.TempDir()
			runtime := filepath.Join(home, "runtime")
			if err := os.Mkdir(runtime, 0700); err != nil {
				t.Fatal(err)
			}
			env := []string{"DISPLAY=:19", "HOME=/no-such-session-test-home", "XDG_RUNTIME_DIR=" + runtime}
			if mode == "public" {
				if err := os.Chmod(runtime, 0777); err != nil {
					t.Fatal(err)
				}
			}
			if mode == "symlink" {
				if err := os.Rename(runtime, runtime+"-real"); err != nil {
					t.Fatal(err)
				}
				if err := os.Symlink(runtime+"-real", runtime); err != nil {
					t.Fatal(err)
				}
			}
			if mode == "lock-symlink" || mode == "lock-hardlink" {
				lock, err := acquireLock(env)
				if err != nil {
					t.Fatal(err)
				}
				lock.Close()
				entries, _ := os.ReadDir(runtime)
				path := filepath.Join(runtime, entries[0].Name())
				if mode == "lock-symlink" {
					if err := os.Rename(path, path+"-real"); err != nil {
						t.Fatal(err)
					}
					if err := os.Symlink(path+"-real", path); err != nil {
						t.Fatal(err)
					}
				} else {
					if err := os.Link(path, path+"-alias"); err != nil {
						t.Fatal(err)
					}
				}
			}
			if lock, err := acquireLock(env); err == nil {
				lock.Close()
				t.Fatal("unsafe storage admitted")
			}
		})
	}
}

func TestUnsafeCacheLeafDoesNotChangeItsPermissions(t *testing.T) {
	home := t.TempDir()
	cache := filepath.Join(home, ".cache")
	if err := os.Mkdir(cache, 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.Mkdir(filepath.Join(cache, "fvwm3"), 0700); err != nil {
		t.Fatal(err)
	}
	leaf := filepath.Join(cache, "fvwm3", "session-owner")
	if err := os.Mkdir(leaf, 0755); err != nil {
		t.Fatal(err)
	}
	if lock, err := acquireLock([]string{"HOME=" + home, "DISPLAY=:19"}); err == nil {
		lock.Close()
		t.Fatal("public private leaf admitted")
	}
	info, err := os.Stat(leaf)
	if err != nil || info.Mode().Perm() != 0755 {
		t.Fatalf("changed existing permissions: %v %v", info, err)
	}
}

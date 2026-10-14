// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package xdgsource

import (
	"os"
	"path/filepath"
	"testing"
)

func TestPrivateRecords(t *testing.T) {
	t.Run("atomic-create-replace-and-mode", func(t *testing.T) {
		path := filepath.Join(t.TempDir(), "record")
		if err := PublishPrivate(path, []byte("first")); err != nil {
			t.Fatal(err)
		}
		if err := PublishPrivate(path, []byte("second")); err != nil {
			t.Fatal(err)
		}
		data, err := ReadPrivate(path)
		if err != nil || string(data) != "second" {
			t.Fatal(data, err)
		}
		info, _ := os.Stat(path)
		if info.Mode().Perm() != 0600 {
			t.Fatal(info.Mode())
		}
	})
	t.Run("legacy-readable-settings-upgrade", func(t *testing.T) {
		path := filepath.Join(t.TempDir(), "settings")
		writeFixture(t, path, "old")
		if err := os.Chmod(path, 0644); err != nil {
			t.Fatal(err)
		}
		if _, err := ReadPrivate(path); err == nil {
			t.Fatal("accepted public action record")
		}
		data, err := ReadSettings(path)
		if err != nil || string(data) != "old" {
			t.Fatal(data, err)
		}
		if err := PublishPrivate(path, []byte("new")); err != nil {
			t.Fatal(err)
		}
		if _, err := ReadPrivate(path); err != nil {
			t.Fatal(err)
		}
	})
	t.Run("settings-link-readable-publication-refused", func(t *testing.T) {
		dir := t.TempDir()
		target := filepath.Join(dir, "real")
		link := filepath.Join(dir, "link")
		writeFixture(t, target, "original")
		if err := os.Symlink(target, link); err != nil {
			t.Fatal(err)
		}
		if _, err := ReadSettings(link); err != nil {
			t.Fatal(err)
		}
		if err := PublishPrivate(link, []byte("new")); err == nil {
			t.Fatal("replaced symlink")
		}
		data, _ := os.ReadFile(target)
		if string(data) != "original" {
			t.Fatal(data)
		}
	})
	t.Run("linked-output-preserved", func(t *testing.T) {
		dir := t.TempDir()
		path := filepath.Join(dir, "record")
		writeFixture(t, path, "original")
		if err := os.Link(path, filepath.Join(dir, "other")); err != nil {
			t.Fatal(err)
		}
		if err := PublishPrivate(path, []byte("new")); err == nil {
			t.Fatal("replaced linked file")
		}
		data, _ := os.ReadFile(path)
		if string(data) != "original" {
			t.Fatal(data)
		}
	})
	t.Run("directory-permissions", func(t *testing.T) {
		dir := t.TempDir()
		if err := os.Chmod(dir, 0777); err != nil {
			t.Fatal(err)
		}
		if err := PublishPrivate(filepath.Join(dir, "record"), []byte("new")); err == nil {
			t.Fatal("accepted writable directory")
		}
	})
	t.Run("writable-settings-refused", func(t *testing.T) {
		path := filepath.Join(t.TempDir(), "record")
		writeFixture(t, path, "original")
		if err := os.Chmod(path, 0666); err != nil {
			t.Fatal(err)
		}
		if _, err := ReadSettings(path); err == nil {
			t.Fatal("accepted writable settings")
		}
		if err := PublishPrivate(path, []byte("new")); err == nil {
			t.Fatal("accepted writable destination")
		}
	})
}

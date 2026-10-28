// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package pinboard

import (
	"errors"
	"io"
	"os"
	"path/filepath"
	"strings"
)

const MaxImageBytes int64 = 128 * 1024 * 1024

func imageGeneration(path, owner string) error {
	if !Hex(owner, 32) || !filepath.IsAbs(path) || filepath.Clean(path) != path || len(path) > 256 ||
		filepath.Base(filepath.Dir(path)) != "fvwm-pinboard-"+owner || !strings.HasPrefix(filepath.Base(path), "g-") {
		return errors.New("invalid owned pinboard image generation")
	}
	n, err := Decimal(strings.TrimPrefix(filepath.Base(path), "g-"), MaxGeneration)
	if err != nil || n == 0 {
		return errors.New("invalid image generation number")
	}
	return nil
}

func PrepareImages(path, owner string) error {
	if err := imageGeneration(path, owner); err != nil {
		return err
	}
	for _, dir := range []string{filepath.Dir(path), path} {
		if err := os.Mkdir(dir, 0700); err != nil && !os.IsExist(err) {
			return err
		}
		info, err := os.Lstat(dir)
		if err != nil {
			return err
		}
		if !private(info, true) {
			return errors.New("image generation directory must be private and owned")
		}
	}
	return CheckImages(path, owner)
}

func imageFiles() ([]string, int64, error) {
	dir, err := os.Open(".")
	if err != nil {
		return nil, 0, err
	}
	defer dir.Close()
	entries, err := dir.ReadDir(MaxPins + 1)
	if err != nil && err != io.EOF {
		return nil, 0, err
	}
	if len(entries) > MaxPins {
		return nil, 0, errors.New("image generation exceeds file count limit")
	}
	var names []string
	var bytes int64
	for _, entry := range entries {
		name := entry.Name()
		ext := filepath.Ext(name)
		digest := strings.TrimSuffix(strings.TrimPrefix(name, ".fvwm-xdg-"), ext)
		if !strings.HasPrefix(name, ".fvwm-xdg-") || !Hex(digest, 64) || (ext != ".png" && ext != ".svg" && ext != ".xpm") {
			return nil, 0, errors.New("unexpected file in owned image generation")
		}
		info, err := os.Lstat(name)
		if err != nil {
			return nil, 0, err
		}
		if !private(info, false) || info.Size() < 0 {
			return nil, 0, errors.New("image generation contains a non-private file")
		}
		// Saturate the accounting without making obsolete oversized files
		// impossible to retire after a failed acquisition.
		if info.Size() > MaxImageBytes-bytes {
			bytes = MaxImageBytes + 1
		} else {
			bytes += info.Size()
		}
		names = append(names, name)
	}
	return names, bytes, nil
}

func CheckImages(path, owner string) error {
	if err := imageGeneration(path, owner); err != nil {
		return err
	}
	return withStore(filepath.Join(path, "sentinel"), func(string) error {
		_, bytes, err := imageFiles()
		if err != nil {
			return err
		}
		if bytes > MaxImageBytes {
			return errors.New("image generation exceeds 128 MiB")
		}
		return nil
	})
}

// RetireImages is worker-only and must be requested only after the native
// replacement/retirement barrier acknowledged that this generation is obsolete.
// Validate the whole directory before unlinking any record; never walk children.
func RetireImages(path, owner string) error {
	if err := imageGeneration(path, owner); err != nil {
		return err
	}
	info, err := os.Lstat(path)
	if os.IsNotExist(err) {
		return nil
	}
	if err != nil {
		return err
	}
	if !private(info, true) {
		return errors.New("refusing foreign image directory")
	}
	if err := withStore(filepath.Join(path, "sentinel"), func(string) error {
		names, _, err := imageFiles()
		if err != nil {
			return err
		}
		for _, name := range names {
			if err := os.Remove(name); err != nil {
				return err
			}
		}
		return nil
	}); err != nil {
		return err
	}
	current, err := os.Lstat(path)
	if err != nil {
		return err
	}
	if !os.SameFile(info, current) {
		return errors.New("image generation directory was replaced")
	}
	if err := os.Remove(path); err != nil {
		return err
	}
	// Removing an empty owner root is safe; an active generation keeps its
	// directory nonempty and therefore prevents this optional final cleanup.
	if err := os.Remove(filepath.Dir(path)); err != nil && !os.IsExist(err) {
		return err
	}
	return nil
}

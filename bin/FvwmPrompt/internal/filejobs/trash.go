//go:build go1.25

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filejobs

import (
	"crypto/rand"
	"encoding/hex"
	"errors"
	"io"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"syscall"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

type trashRoots struct{ root, files, info *os.Root }

func (t *trashRoots) close() { t.info.Close(); t.files.Close(); t.root.Close() }

func privateDirectory(parent *os.Root, name string) (*os.Root, error) {
	if err := parent.Mkdir(name, 0700); err != nil && !errors.Is(err, os.ErrExist) {
		return nil, err
	}
	info, err := parent.Lstat(name)
	if err != nil {
		return nil, err
	}
	if !info.IsDir() || info.Mode().Perm() != 0700 || int(info.Sys().(*syscall.Stat_t).Uid) != os.Getuid() {
		return nil, errors.New("trash directory must be owned, private, and not a symlink")
	}
	r, err := parent.OpenRoot(name)
	if err != nil {
		return nil, err
	}
	f, err := r.Open(".")
	if err == nil {
		var actual os.FileInfo
		actual, err = f.Stat()
		f.Close()
		if err == nil && !sameObject(stamp(info), stamp(actual)) {
			err = errors.New("trash directory changed")
		}
	}
	if err != nil {
		r.Close()
		return nil, err
	}
	return r, nil
}

// This home-trash subset refuses cross-filesystem trash; it never falls back to
// deletion or creates mount-top trash directories. Data and info are private.
func openTrash() (*trashRoots, error) {
	data, err := dataHome()
	if err != nil {
		return nil, err
	}
	if err := os.MkdirAll(data, 0700); err != nil {
		return nil, err
	}
	base, err := os.OpenRoot(data)
	if err != nil {
		return nil, err
	}
	defer base.Close()
	root, err := privateDirectory(base, "Trash")
	if err != nil {
		return nil, err
	}
	files, err := privateDirectory(root, "files")
	if err != nil {
		root.Close()
		return nil, err
	}
	info, err := privateDirectory(root, "info")
	if err != nil {
		files.Close()
		root.Close()
		return nil, err
	}
	return &trashRoots{root: root, files: files, info: info}, nil
}

func rootStamp(root *os.Root) (filemodel.Identity, error) {
	f, err := root.Open(".")
	if err != nil {
		return filemodel.Identity{}, err
	}
	defer f.Close()
	i, err := f.Stat()
	if err != nil {
		return filemodel.Identity{}, err
	}
	return stamp(i), nil
}

func trashRecord(path []byte) []byte {
	return []byte("[Trash Info]\nPath=" + url.PathEscape(string(path)) + "\nDeletionDate=" + time.Now().Format("2006-01-02T15:04:05") + "\n")
}

func parseTrash(data []byte) ([]byte, error) {
	lines := strings.Split(string(data), "\n")
	if len(lines) != 4 || lines[0] != "[Trash Info]" || !strings.HasPrefix(lines[1], "Path=") || !strings.HasPrefix(lines[2], "DeletionDate=") || lines[3] != "" {
		return nil, errors.New("invalid trash metadata")
	}
	path, err := url.PathUnescape(strings.TrimPrefix(lines[1], "Path="))
	if err != nil || !validPath([]byte(path)) || !validName([]byte(filepath.Base(path))) {
		return nil, errors.New("invalid original trash path")
	}
	if _, err = time.Parse("2006-01-02T15:04:05", strings.TrimPrefix(lines[2], "DeletionDate=")); err != nil {
		return nil, errors.New("invalid trash deletion date")
	}
	return []byte(path), nil
}

func (r *runner) trash(source *os.Root, nodes []treeNode, item filemodel.Item) Outcome {
	out := Outcome{SourceRemaining: true}
	roots, err := openTrash()
	if err != nil {
		out.State = "failed"
		out.Detail = "Trash unavailable: " + err.Error()
		return out
	}
	defer roots.close()
	value, err := rootStamp(roots.files)
	if err != nil || value.Device != item.Identity.Device {
		out.State = "failed"
		out.Detail = "Trash unavailable on this filesystem; no deletion performed"
		return out
	}
	if below(roots.root.Name(), string(r.path)) {
		out.State = "failed"
		out.Detail = "Already inside trash; use restore or explicit delete"
		return out
	}
	var random [16]byte
	if _, err = rand.Read(random[:]); err != nil {
		out.State = "failed"
		out.Detail = err.Error()
		return out
	}
	name := hex.EncodeToString(random[:])
	metadata := name + ".trashinfo"
	reservation, err := roots.info.OpenFile(metadata, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if err != nil {
		out.State = "failed"
		out.Detail = err.Error()
		return out
	}
	reservedInfo, err := reservation.Stat()
	reservation.Close()
	if err != nil {
		out.State = "failed"
		out.Detail = err.Error()
		return out
	}
	r.destination = []byte(filepath.Join(roots.files.Name(), name))
	// The exclusive empty info reservation is not valid trash metadata. Only
	// complete content receives a valid atomic metadata publication below.
	out, err = r.copy(source, roots.files, nodes, name)
	published := false
	if err == nil {
		temporary := metadata + ".pending"
		var f *os.File
		f, err = roots.info.OpenFile(temporary, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
		if err == nil {
			data := trashRecord(r.path)
			var n int
			n, err = f.Write(data)
			if err == nil && n != len(data) {
				err = io.ErrShortWrite
			}
			if err == nil {
				err = f.Sync()
			}
			if closeErr := f.Close(); err == nil {
				err = closeErr
			}
		}
		if err == nil {
			var current os.FileInfo
			current, err = roots.info.Lstat(metadata)
			if err == nil && stamp(current) != stamp(reservedInfo) {
				err = errors.New("trash reservation changed")
			}
			// Replacement is limited to our exclusive private reservation;
			// arbitrary public destinations never use overwrite rename.
			if err == nil {
				err = roots.info.Rename(temporary, metadata)
			}
		}
		if err == nil {
			published = true
			_, err = r.remove(source, nodes)
			out.SourceRemaining = err != nil
		}
	}
	if err != nil {
		if !published {
			out.MetadataComplete = false
		}
		out.State = stateFor(err, out.DestinationPresent)
		out.Detail = "Trash incomplete; source/partial data retained: " + err.Error()
	} else {
		out.State = "completed"
		out.Detail = "Home trash; original path recorded, no undo guarantee"
	}
	return out
}

func readTrash(infoRoot *os.Root, name string) ([]byte, filemodel.Identity, error) {
	before, err := infoRoot.Lstat(name)
	if err != nil {
		return nil, filemodel.Identity{}, err
	}
	if !before.Mode().IsRegular() || before.Mode().Perm()&0077 != 0 || int(before.Sys().(*syscall.Stat_t).Uid) != os.Getuid() || before.Size() > MaxRecord {
		return nil, filemodel.Identity{}, errors.New("trash metadata is not a bounded private regular file")
	}
	f, err := infoRoot.Open(name)
	if err != nil {
		return nil, filemodel.Identity{}, err
	}
	defer f.Close()
	opened, err := f.Stat()
	if err != nil {
		return nil, filemodel.Identity{}, err
	}
	if stamp(opened) != stamp(before) {
		return nil, filemodel.Identity{}, errors.New("trash metadata changed")
	}
	data, err := io.ReadAll(io.LimitReader(f, MaxRecord+1))
	if err != nil || len(data) > MaxRecord {
		return nil, filemodel.Identity{}, errors.New("trash metadata exceeds limit")
	}
	after, err := f.Stat()
	if err != nil || stamp(after) != stamp(before) {
		return nil, filemodel.Identity{}, errors.New("trash metadata changed while reading")
	}
	path, err := parseTrash(data)
	return path, stamp(before), err
}

func (r *runner) restore(source *os.Root, nodes []treeNode, item filemodel.Item) Outcome {
	out := Outcome{SourceRemaining: true}
	roots, err := openTrash()
	if err != nil {
		out.State = "failed"
		out.Detail = err.Error()
		return out
	}
	defer roots.close()
	value, err := rootStamp(roots.files)
	if err != nil || !sameObject(value, r.req.Snapshot.Root) {
		out.State = "failed"
		out.Detail = "Restore requires selected entries in the private home trash files folder"
		return out
	}
	metadata := string(item.Name) + ".trashinfo"
	path, metadataIdentity, err := readTrash(roots.info, metadata)
	if err != nil {
		out.State = "failed"
		out.Detail = err.Error()
		return out
	}
	destination, err := os.OpenRoot(filepath.Dir(string(path)))
	if err != nil {
		out.State = "failed"
		out.Detail = "Original parent unavailable: " + err.Error()
		return out
	}
	defer destination.Close()
	name, err := r.choose(destination, filepath.Base(string(path)))
	if err == nil {
		r.destination = []byte(filepath.Join(destination.Name(), name))
		out, err = r.copy(source, destination, nodes, name)
		if err == nil {
			_, err = r.remove(source, nodes)
			out.SourceRemaining = err != nil
		}
		if err == nil {
			current, e := roots.info.Lstat(metadata)
			if e != nil {
				err = e
			} else if stamp(current) != metadataIdentity {
				err = errors.New("trash metadata changed before retirement")
			} else {
				err = roots.info.Remove(metadata)
			}
		}
	}
	if err != nil {
		out.MetadataComplete = false
		out.State = stateFor(err, out.DestinationPresent)
		out.Detail = "Restore incomplete: " + err.Error()
	} else {
		out.State = "completed"
		out.Detail = "Restored without overwrite"
	}
	return out
}

func dataHome() (string, error) {
	data := os.Getenv("XDG_DATA_HOME")
	home := ""
	if data == "" || !filepath.IsAbs(data) {
		var err error
		home, err = os.UserHomeDir()
		if err != nil {
			return "", err
		}
	}
	files, err := TrashFilesPath(home, data)
	if err != nil {
		return "", err
	}
	return filepath.Dir(filepath.Dir(files)), nil
}

// Erasing a selected home-trash entry also retires its associated private info.
// Ordinary deletion only probes the configured location; it never creates it.
func retireTrashMetadata(source *os.Root, item filemodel.Item) error {
	data, err := dataHome()
	if err != nil {
		return err
	}
	info, err := os.Stat(filepath.Join(data, "Trash", "files"))
	if errors.Is(err, os.ErrNotExist) {
		return nil
	}
	if err != nil {
		return err
	}
	actual, err := rootStamp(source)
	if err != nil {
		return err
	}
	if !sameObject(actual, stamp(info)) {
		return nil
	}
	roots, err := openTrash()
	if err != nil {
		return err
	}
	defer roots.close()
	name := string(item.Name) + ".trashinfo"
	before, err := roots.info.Lstat(name)
	if errors.Is(err, os.ErrNotExist) {
		return nil
	}
	if err != nil {
		return err
	}
	if !before.Mode().IsRegular() || before.Mode().Perm()&0077 != 0 || int(before.Sys().(*syscall.Stat_t).Uid) != os.Getuid() {
		return errors.New("trash metadata retirement refused")
	}
	return roots.info.Remove(name)
}

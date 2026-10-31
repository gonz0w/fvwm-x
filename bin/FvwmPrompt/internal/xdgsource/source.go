// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package xdgsource acquires one desktop-menu worker's filesystem inputs.
// It owns no X state. Cancellation checks bound cooperative work; the caller
// must enforce the worker deadline to cover a blocked filesystem operation.
package xdgsource

import (
	"context"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"syscall"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

const MaxJobBytes = 128 * 1024 * 1024

type Source struct {
	ctx                context.Context
	bytes, items       int
	files              map[string][]byte
	desktops           map[string]*xdgmenu.Desktop
	pools              map[xdgmenu.AppLocation]map[string]xdgmenu.PoolEntry
	Directories        map[string]*xdgmenu.Desktop
	KDEDirs            []string
	KDEDirectoryLookup func() ([]string, error)
	kdeResolved        bool
}

// The CLI supplies the optional KDE tool's bounded inventory. Acquisition never
// invokes programs while resolving a file or interpreting a desktop entry.
func (s *Source) KDELegacyDirectories() ([]string, error) {
	if err := s.ctx.Err(); err != nil {
		return nil, err
	}
	if !s.kdeResolved && s.KDEDirectoryLookup != nil {
		var err error
		s.KDEDirs, err = s.KDEDirectoryLookup()
		if err != nil {
			return nil, err
		}
		s.kdeResolved = true
	}
	return append([]string(nil), s.KDEDirs...), nil
}

func New(ctx context.Context) *Source {
	if ctx == nil {
		ctx = context.Background()
	}
	return &Source{ctx: ctx, bytes: MaxJobBytes, items: xdgmenu.MaxItems, files: make(map[string][]byte),
		desktops: make(map[string]*xdgmenu.Desktop), pools: make(map[xdgmenu.AppLocation]map[string]xdgmenu.PoolEntry), Directories: make(map[string]*xdgmenu.Desktop)}
}

// NewLimited gives an isolated consumer a smaller aggregate acquisition budget.
// The existing New defaults remain unchanged for menu/controller consumers.
func NewLimited(ctx context.Context, byteLimit, itemLimit int) (*Source, error) {
	if byteLimit < 1 || byteLimit > MaxJobBytes || itemLimit < 1 || itemLimit > xdgmenu.MaxItems {
		return nil, errors.New("invalid acquisition budget")
	}
	s := New(ctx)
	s.bytes, s.items = byteLimit, itemLimit
	return s, nil
}

func validPath(path string) bool {
	return filepath.IsAbs(path) && filepath.Clean(path) == path && len(path) <= xdgmenu.MaxValueBytes && utf8.ValidString(path) && !strings.ContainsRune(path, 0)
}

func (s *Source) ContextError() error { return s.ctx.Err() }

func (s *Source) charge(items, bytes int) error {
	if err := s.ctx.Err(); err != nil {
		return err
	}
	if items < 0 || bytes < 0 || items > s.items || bytes > s.bytes {
		return errors.New("desktop menu acquisition exceeds job limits")
	}
	s.items -= items
	s.bytes -= bytes
	return nil
}

func openFile(path string, directory bool) (*os.File, error) {
	return openLimitedFile(path, directory, xdgmenu.MaxSourceBytes)
}

var errInputFileLimit = errors.New("desktop menu input exceeds file limit")

func openLimitedFile(path string, directory bool, limit int) (*os.File, error) {
	flags := syscall.O_RDONLY | syscall.O_NONBLOCK | syscall.O_CLOEXEC
	if directory {
		flags |= syscall.O_DIRECTORY
	}
	fd, err := syscall.Open(path, flags, 0)
	if err != nil {
		return nil, &os.PathError{Op: "open", Path: path, Err: err}
	}
	file := os.NewFile(uintptr(fd), path)
	info, err := file.Stat()
	if err != nil {
		file.Close()
		return nil, err
	}
	if directory && !info.IsDir() || !directory && !info.Mode().IsRegular() {
		file.Close()
		return nil, fmt.Errorf("%s is not a regular input %s", path, map[bool]string{true: "directory", false: "file"}[directory])
	}
	if !directory && info.Size() > int64(limit) {
		file.Close()
		return nil, errInputFileLimit
	}
	return file, nil
}

func (s *Source) ReadMenu(path string) ([]byte, error) {
	return s.readLimited(path, xdgmenu.MaxSourceBytes)
}

func (s *Source) ReadImage(path string) ([]byte, error) {
	return s.readLimited(path, MaxJobBytes)
}

func (s *Source) readLimited(path string, limit int) ([]byte, error) {
	if !validPath(path) {
		return nil, errors.New("invalid input path")
	}
	if err := s.ctx.Err(); err != nil {
		return nil, err
	}
	if data, ok := s.files[path]; ok {
		if len(data) > limit {
			return nil, errors.New("cached input exceeds file limit")
		}
		return data, nil
	}
	if err := s.charge(1, len(path)); err != nil {
		return nil, err
	}
	// Refuse an oversized acquisition before growing a byte slice. Cached
	// snapshots above were already charged to this operation's budget.
	jobLimited := limit > s.bytes
	if jobLimited {
		limit = s.bytes
	}
	file, err := openLimitedFile(path, false, limit)
	if err != nil {
		if jobLimited && errors.Is(err, errInputFileLimit) {
			return nil, errors.New("desktop menu input exceeds job limits")
		}
		return nil, err
	}
	defer file.Close()
	var data []byte
	buffer := make([]byte, min(32768, limit+1))
	for {
		if err = s.ctx.Err(); err != nil {
			return nil, err
		}
		n, readErr := file.Read(buffer)
		if len(data) > limit-n {
			if jobLimited {
				return nil, errors.New("desktop menu input exceeds job limits")
			}
			return nil, errors.New("desktop menu input grew beyond file limit")
		}
		if err = s.charge(0, n); err != nil {
			return nil, err
		}
		data = append(data, buffer[:n]...)
		if readErr == io.EOF {
			break
		}
		if readErr != nil {
			return nil, readErr
		}
	}
	s.files[path] = data
	return data, nil
}

func (s *Source) entries(path string) ([]fs.DirEntry, error) {
	if !validPath(path) {
		return nil, errors.New("invalid directory path")
	}
	if err := s.charge(1, len(path)); err != nil {
		return nil, err
	}
	file, err := openFile(path, true)
	if err != nil {
		return nil, err
	}
	defer file.Close()
	var entries []fs.DirEntry
	for {
		if err = s.ctx.Err(); err != nil {
			return nil, err
		}
		batch, readErr := file.ReadDir(256)
		if err = s.charge(len(batch), 0); err != nil {
			return nil, err
		}
		for _, entry := range batch {
			if !utf8.ValidString(entry.Name()) || len(entry.Name()) > xdgmenu.MaxValueBytes {
				return nil, errors.New("invalid directory entry name")
			}
			if err = s.charge(0, len(entry.Name())); err != nil {
				return nil, err
			}
		}
		entries = append(entries, batch...)
		if readErr == io.EOF {
			break
		}
		if readErr != nil {
			return nil, readErr
		}
	}
	sort.Slice(entries, func(i, j int) bool { return entries[i].Name() < entries[j].Name() })
	return entries, nil
}

func (s *Source) MenuFiles(path string) ([]string, error) {
	entries, err := s.entries(path)
	if err != nil {
		return nil, err
	}
	var names []string
	for _, entry := range entries {
		if !entry.IsDir() && strings.HasSuffix(entry.Name(), ".menu") {
			names = append(names, entry.Name())
		}
	}
	return names, nil
}

func (s *Source) desktop(path string) (*xdgmenu.Desktop, error) {
	if d, ok := s.desktops[path]; ok {
		return d, nil
	}
	data, err := s.ReadMenu(path)
	if err != nil {
		return nil, err
	}
	d, err := xdgmenu.ParseDesktop(data)
	if err != nil {
		return nil, fmt.Errorf("desktop %s: %w", path, err)
	}
	s.desktops[path] = d
	return d, nil
}

func (s *Source) walk(root, path string, depth int, active map[string]bool, visit func(string) error) error {
	return s.walkLimited(root, path, depth, xdgmenu.MaxDepth, active, visit)
}

func (s *Source) walkLimited(root, path string, depth, maxDepth int, active map[string]bool, visit func(string) error) error {
	if depth >= maxDepth {
		return errors.New("application directory exceeds depth limit")
	}
	if err := s.ctx.Err(); err != nil {
		return err
	}
	canonical, err := filepath.EvalSymlinks(path)
	if err != nil {
		return err
	}
	if active[canonical] {
		return nil
	}
	active[canonical] = true
	defer delete(active, canonical)
	entries, err := s.entries(path)
	if err != nil {
		return err
	}
	for _, entry := range entries {
		filename := filepath.Join(path, entry.Name())
		if !validPath(filename) {
			return errors.New("application path exceeds limit")
		}
		info, err := os.Stat(filename)
		if errors.Is(err, fs.ErrNotExist) {
			continue
		}
		if err != nil {
			return err
		}
		if info.IsDir() {
			if err = s.walkLimited(root, filename, depth+1, maxDepth, active, visit); err != nil {
				return err
			}
		} else if strings.HasSuffix(entry.Name(), ".desktop") {
			if err = visit(filename); err != nil {
				return err
			}
		}
	}
	return nil
}

func (s *Source) pool(path string) (map[string]xdgmenu.PoolEntry, error) {
	if pool, ok := s.pools[xdgmenu.AppLocation{Path: path}]; ok {
		return pool, nil
	}
	var files []xdgmenu.DesktopFile
	err := s.walk(path, path, 0, make(map[string]bool), func(filename string) error {
		d, err := s.desktop(filename)
		if errors.Is(err, fs.ErrNotExist) {
			return nil
		}
		if err != nil {
			return err
		}
		relative, err := filepath.Rel(path, filename)
		if err != nil {
			return err
		}
		files = append(files, xdgmenu.DesktopFile{Relative: relative, Entry: d})
		return nil
	})
	if errors.Is(err, fs.ErrNotExist) {
		err = nil
	}
	if err != nil {
		return nil, err
	}
	pool, err := xdgmenu.BuildPool([]xdgmenu.AppDirectory{{Path: path, Entries: files}})
	if err != nil {
		return nil, err
	}
	s.pools[xdgmenu.AppLocation{Path: path}] = pool
	return pool, nil
}

// Prepare collects exactly the resolved menu's AppDirs and directory references.
// No default paths are scanned unless the XML requested them. Directory files
// are read on demand; unrelated files in their directories are not parsed.
func (s *Source) Prepare(root *xdgmenu.Node) (map[xdgmenu.AppLocation]map[string]xdgmenu.PoolEntry, error) {
	result := make(map[xdgmenu.AppLocation]map[string]xdgmenu.PoolEntry)
	var scan func(*xdgmenu.Node, []string, int) error
	scan = func(node *xdgmenu.Node, inherited []string, depth int) error {
		if node == nil || depth >= xdgmenu.MaxDepth {
			return errors.New("invalid acquisition tree")
		}
		if err := s.charge(1+len(node.Children), 0); err != nil {
			return err
		}
		dirs := append([]string(nil), inherited...)
		var references []string
		for _, child := range node.Children {
			if child == nil {
				return errors.New("nil acquisition element")
			}
			switch child.Tag {
			case "AppDir":
				location, err := xdgmenu.ApplicationLocation(child)
				if err != nil {
					return err
				}
				pool, err := s.locationPool(location)
				if err != nil {
					return err
				}
				result[location] = pool
			case "DirectoryDir":
				if !validPath(child.Text) {
					return errors.New("invalid directory metadata path")
				}
				dirs = append(dirs, child.Text)
			case "Directory":
				references = append(references, strings.Trim(child.Text, " \t\r\n"))
			}
		}
		for _, relative := range references {
			if filepath.IsAbs(relative) || filepath.Clean(relative) != relative || relative == ".." || strings.HasPrefix(relative, "../") || !strings.HasSuffix(relative, ".directory") {
				return errors.New("invalid directory entry reference")
			}
			for _, dir := range dirs {
				if err := s.charge(1, 0); err != nil {
					return err
				}
				path := filepath.Join(dir, relative)
				if !validPath(path) {
					return errors.New("directory entry path exceeds limit")
				}
				d, err := s.desktop(path)
				if errors.Is(err, fs.ErrNotExist) {
					continue
				}
				if err != nil {
					return err
				}
				s.Directories[path] = d
			}
		}
		for _, child := range node.Children {
			if child.Tag == "Menu" {
				if err := scan(child, dirs, depth+1); err != nil {
					return err
				}
			}
		}
		return nil
	}
	if root == nil {
		return result, nil
	}
	if err := scan(root, nil, 0); err != nil {
		return nil, err
	}
	return result, nil
}

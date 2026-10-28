//go:build go1.25

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filejobs

import (
	"errors"
	"fmt"
	"io"
	"math"
	"os"
	"path/filepath"
	"strings"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
)

type treeNode struct {
	path     string
	info     os.FileInfo
	identity filemodel.Identity
	link     string
}

func plan(root *os.Root, name string, r *runner) ([]treeNode, error) {
	nodes := []treeNode{}
	var walk func(string, int) error
	walk = func(path string, depth int) error {
		if err := r.poll(); err != nil {
			return err
		}
		if depth > MaxDepth || r.total >= MaxEntries || len(filepath.Join(root.Name(), path)) > filemodel.MaxPath {
			return errors.New("tree exceeds depth, entry or path budget")
		}
		info, err := root.Lstat(path)
		if err != nil {
			return err
		}
		if !info.Mode().IsRegular() && !info.IsDir() && info.Mode()&os.ModeSymlink == 0 {
			return errors.New("special entries are unavailable")
		}
		node := treeNode{path: path, info: info, identity: stamp(info)}
		if info.Mode()&os.ModeSymlink != 0 {
			node.link, err = root.Readlink(path)
			if err != nil {
				return err
			}
			if len(node.link) > filemodel.MaxPath || strings.ContainsRune(node.link, 0) {
				return errors.New("link target exceeds budget")
			}
			if err = checkNode(root, node, false); err != nil {
				return err
			}
		}
		nodes = append(nodes, node)
		r.total++
		if !info.IsDir() {
			return nil
		}
		dir, err := root.Open(path)
		if err != nil {
			return err
		}
		defer dir.Close()
		actual, err := dir.Stat()
		if err != nil {
			return err
		}
		if stamp(actual) != node.identity {
			return errors.New("folder changed during traversal")
		}
		entries, err := dir.ReadDir(MaxEntries - int(r.total) + 1)
		if err != nil && err != io.EOF {
			return err
		}
		if uint64(len(entries))+r.total > MaxEntries {
			return errors.New("tree exceeds entry budget")
		}
		for _, entry := range entries {
			if !validName([]byte(entry.Name())) {
				return errors.New("invalid tree component")
			}
			if err = walk(filepath.Join(path, entry.Name()), depth+1); err != nil {
				return err
			}
		}
		return checkNode(root, node, false)
	}
	if err := walk(name, 0); err != nil {
		return nil, err
	}
	return nodes, nil
}

func below(source, destination string) bool {
	a, err := filepath.EvalSymlinks(source)
	if err != nil {
		return true
	}
	b, err := filepath.EvalSymlinks(destination)
	if err != nil {
		return true
	}
	rel, err := filepath.Rel(a, b)
	return err != nil || rel == "." || rel != ".." && !strings.HasPrefix(rel, ".."+string(filepath.Separator))
}

// copy publishes each destination exclusively. Failed or cancelled work retains
// its partial destination for inspection and never removes a source. Ordinary
// permission bits and modification times are required; ACLs, owners, xattrs,
// sparse layout and hardlink topology are deliberately not preserved.
func (r *runner) copy(source, destination *os.Root, nodes []treeNode, name string) (Outcome, error) {
	out := Outcome{SourceRemaining: true}
	r.receipt = newReceipt(destination)
	if nodes[0].info.IsDir() && below(filepath.Join(source.Name(), nodes[0].path), destination.Name()) {
		return out, errors.New("destination is inside the source tree or cannot be resolved")
	}
	for _, node := range nodes {
		if err := r.poll(); err != nil {
			return out, err
		}
		if err := checkNode(source, node, false); err != nil {
			return out, err
		}
		rel, err := filepath.Rel(nodes[0].path, node.path)
		if err != nil {
			return out, err
		}
		target := name
		if rel != "." {
			target = filepath.Join(name, rel)
		}
		if len(filepath.Join(destination.Name(), target)) > filemodel.MaxPath {
			return out, errors.New("destination path exceeds budget")
		}
		if err = r.receipt.parents(target); err != nil {
			return out, err
		}
		switch {
		case node.info.IsDir():
			err = destination.Mkdir(target, 0700)
		case node.info.Mode()&os.ModeSymlink != 0:
			err = destination.Symlink(node.link, target)
		default:
			var created bool
			created, err = r.copyFile(source, destination, node, target)
			out.DestinationPresent = out.DestinationPresent || created
		}
		if err != nil {
			return out, err
		}
		if !node.info.Mode().IsRegular() {
			created, e := destination.Lstat(target)
			if e != nil {
				return out, e
			}
			if created.Mode().Type() != node.info.Mode().Type() {
				return out, errors.New("created destination type changed")
			}
			r.receipt.add(node.path, target, created)
		}
		out.DestinationPresent = true
		r.done++
		if err = r.progress(false); err != nil {
			return out, err
		}
	}
	for i := len(nodes) - 1; i >= 0; i-- {
		node := nodes[i]
		if err := checkNode(source, node, false); err != nil {
			return out, err
		}
		if !node.info.IsDir() {
			continue
		}
		rel, _ := filepath.Rel(nodes[0].path, node.path)
		target := name
		if rel != "." {
			target = filepath.Join(name, rel)
		}
		if err := r.receipt.parents(target); err != nil {
			return out, err
		}
		if err := r.receipt.check(target, false); err != nil {
			return out, err
		}
		owned, err := destination.Open(target)
		if err != nil {
			return out, err
		}
		opened, err := owned.Stat()
		if err == nil && !sameEntry(stamp(opened), r.receipt.entries[target]) {
			err = errors.New("created directory changed before metadata")
		}
		if err == nil {
			err = owned.Chmod(node.info.Mode().Perm())
		}
		if err == nil {
			err = r.receipt.check(target, false)
		}
		if err == nil {
			err = destination.Chtimes(target, node.info.ModTime(), node.info.ModTime())
		}
		if err == nil {
			err = r.receipt.check(target, false)
		}
		if err == nil {
			opened, err = owned.Stat()
		}
		owned.Close()
		if err != nil {
			return out, err
		}
		r.receipt.add(node.path, target, opened)
	}
	if err := r.receipt.verify(); err != nil {
		return out, err
	}
	out.ContentComplete = true
	out.MetadataComplete = true
	return out, nil
}

func (r *runner) copyFile(source, destination *os.Root, node treeNode, target string) (bool, error) {
	input, err := source.Open(node.path)
	if err != nil {
		return false, err
	}
	defer input.Close()
	info, err := input.Stat()
	if err != nil {
		return false, err
	}
	if stamp(info) != node.identity {
		return false, errors.New("source changed before opening")
	}
	output, err := destination.OpenFile(target, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if err != nil {
		return false, err
	}
	ownedInfo, err := output.Stat()
	if err != nil {
		output.Close()
		return true, err
	}
	r.receipt.add(node.path, target, ownedInfo)
	closed := false
	defer func() {
		if !closed {
			output.Close()
		}
	}()
	buffer := make([]byte, ChunkBytes)
	reader := io.LimitReader(input, node.identity.Size)
	var copied int64
	for {
		if err = r.poll(); err != nil {
			return true, err
		}
		n, readErr := reader.Read(buffer)
		if n > 0 {
			if uint64(n) > math.MaxUint64-r.bytes {
				return true, errors.New("byte progress exceeds limit")
			}
			written, writeErr := output.Write(buffer[:n])
			r.bytes += uint64(written)
			copied += int64(written)
			if writeErr != nil {
				return true, writeErr
			}
			if written != n {
				return true, io.ErrShortWrite
			}
			if err = r.progress(false); err != nil {
				return true, err
			}
		}
		if readErr == io.EOF {
			break
		}
		if readErr != nil {
			return true, readErr
		}
	}
	info, err = input.Stat()
	if err != nil {
		return true, err
	}
	if stamp(info) != node.identity || copied != node.identity.Size {
		return true, errors.New("source content changed during copy")
	}
	if err = output.Chmod(node.info.Mode().Perm()); err != nil {
		return true, err
	}
	if err = output.Sync(); err != nil {
		return true, err
	}
	if err = r.receipt.parents(target); err != nil {
		return true, err
	}
	if err = r.receipt.check(target, false); err != nil {
		return true, err
	}
	if err = destination.Chtimes(target, node.info.ModTime(), node.info.ModTime()); err != nil {
		return true, err
	}
	if err = r.receipt.check(target, false); err != nil {
		return true, err
	}
	ownedInfo, err = output.Stat()
	if err != nil {
		return true, err
	}
	if ownedInfo.Size() != node.identity.Size || ownedInfo.Mode().Perm() != node.info.Mode().Perm() || !ownedInfo.ModTime().Equal(node.info.ModTime()) {
		return true, errors.New("created file content or metadata changed")
	}
	r.receipt.add(node.path, target, ownedInfo)
	closed = true
	if err = output.Close(); err != nil {
		return true, err
	}
	if err = r.receipt.check(target, true); err != nil {
		return true, err
	}
	return true, nil
}

func (r *runner) remove(root *os.Root, nodes []treeNode) (int, error) {
	removed := 0
	for i := len(nodes) - 1; i >= 0; i-- {
		if err := r.poll(); err != nil {
			return removed, err
		}
		node := nodes[i]
		if err := checkNode(root, node, true); err != nil {
			return removed, err
		}
		if r.receipt != nil {
			if err := r.receipt.forSource(node.path); err != nil {
				return removed, err
			}
		}
		if err := root.Remove(node.path); err != nil {
			return removed, fmt.Errorf("remove %q: %w", node.path, err)
		}
		removed++
		if r.req.Operation == "delete" {
			r.done++
		}
		if err := r.progress(false); err != nil {
			return removed, err
		}
	}
	return removed, nil
}

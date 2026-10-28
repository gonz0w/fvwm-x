//go:build go1.25

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package filejobs

import (
	"errors"
	"os"

	"golang.org/x/sys/unix"
)

// A same-filesystem regular-file move publishes with no-clobber linkat and
// checks the linked inode before unlinking the source. It is not an atomic
// rename or identity-conditional unlink. EXDEV uses the explicit copy/remove
// fallback; public destinations never use overwrite rename.
func (r *runner) moveFile(source, destination *os.Root, node treeNode, name string) (Outcome, bool, error) {
	out := Outcome{SourceRemaining: true}
	if !node.info.Mode().IsRegular() {
		return out, false, nil
	}
	if err := checkNode(source, node, false); err != nil {
		return out, true, err
	}
	from, err := source.Open(".")
	if err != nil {
		return out, true, err
	}
	defer from.Close()
	to, err := destination.Open(".")
	if err != nil {
		return out, true, err
	}
	defer to.Close()
	err = unix.Linkat(int(from.Fd()), node.path, int(to.Fd()), name, 0)
	if errors.Is(err, unix.EXDEV) {
		return out, false, nil
	}
	if err != nil {
		return out, true, err
	}
	out.DestinationPresent = true
	linked, err := destination.Lstat(name)
	if err != nil {
		return out, true, err
	}
	if stamp(linked) != node.identity {
		return out, true, errors.New("source changed during exclusive publication; partial destination retained")
	}
	r.receipt = newReceipt(destination)
	r.receipt.add(node.path, name, linked)
	out.ContentComplete = true
	out.MetadataComplete = true
	_, err = r.remove(source, []treeNode{node})
	out.SourceRemaining = err != nil
	if err == nil {
		r.done++
	}
	out.Detail = "same-filesystem exclusive link/unlink; non-atomic; no overwrite"
	return out, true, err
}

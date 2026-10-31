// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bufio"
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"syscall"
	"time"
)

const fileManagerAdmissionWait = 1500 * time.Millisecond

// Reservations belong to the service loop. Only an actual Wait completion can
// retire a started child, including after a lost or late admission receipt.
type fileManagerChildren struct {
	live map[uint64]bool
	next uint64
	done chan uint64
}

func newFileManagerChildren() *fileManagerChildren {
	return &fileManagerChildren{live: make(map[uint64]bool), done: make(chan uint64, fileManagerMaxGroups)}
}

func (children *fileManagerChildren) drain() {
	for {
		select {
		case id := <-children.done:
			delete(children.live, id)
		default:
			return
		}
	}
}

func (children *fileManagerChildren) reserve(count int) ([]uint64, bool) {
	children.drain()
	if count < 1 || count > fileManagerMaxGroups-len(children.live) || uint64(count) > ^uint64(0)-children.next {
		return nil, false
	}
	ids := make([]uint64, count)
	for i := range ids {
		children.next++
		ids[i] = children.next
		children.live[ids[i]] = true
	}
	return ids, true
}

func fileManagerChildArgs(o options, socket string) []string {
	args := []string{"--_filemanager-child", "--reuse"}
	if socket != "" {
		args = append(args, "--socket", socket)
	}
	if len(o.targets) == 0 {
		return append(args, "--dir", o.dir)
	}
	flag := "--reveal"
	if o.properties {
		flag = "--properties"
	}
	for _, target := range o.targets {
		args = append(args, flag, string(target))
	}
	return args
}

func fileManagerChildEnvironment(startupID string) []string {
	env := os.Environ()
	if startupID == "" {
		return env
	}
	out := make([]string, 0, len(env)+1)
	for _, value := range env {
		if !strings.HasPrefix(value, "DESKTOP_STARTUP_ID=") {
			out = append(out, value)
		}
	}
	return append(out, "DESKTOP_STARTUP_ID="+startupID)
}

// start never attaches the browser to service cancellation. Setsid and private
// stdio make the view independent. Only the native bridge is killed on exit.
func (children *fileManagerChildren) start(id uint64, executable string, o options, socket, startupID string) (*os.File, string) {
	reader, writer, err := os.Pipe()
	if err != nil {
		delete(children.live, id)
		return nil, "refused"
	}
	cmd := exec.Command(executable, fileManagerChildArgs(o, socket)...)
	cmd.Env = fileManagerChildEnvironment(startupID)
	cmd.SysProcAttr = &syscall.SysProcAttr{Setsid: true}
	cmd.ExtraFiles = []*os.File{writer}
	cmd.Stderr = os.Stderr
	err = cmd.Start()
	writer.Close()
	if err != nil {
		reader.Close()
		delete(children.live, id)
		return nil, "refused"
	}
	// Exactly one waiter per successful Start. Capacity equals the maximum
	// number of children, so exit never blocks after the service loop retires.
	go func() {
		_ = cmd.Wait()
		children.done <- id
	}()
	return reader, ""
}

func (children *fileManagerChildren) admit(ctx context.Context, executable string, groups []options, socket, startupID string) string {
	ids, ok := children.reserve(len(groups))
	if !ok {
		return "refused"
	}
	started := 0
	defer func() {
		// Started children, including uncertain admissions, keep their slots
		// until actual Wait. Only never-submitted groups can be released here.
		for _, id := range ids[started:] {
			delete(children.live, id)
		}
	}()
	for i, group := range groups {
		if ctx.Err() != nil {
			return "unknown"
		}
		reader, status := children.start(ids[i], executable, group, socket, startupID)
		if status != "" {
			if started != 0 {
				return "unknown" // Prior submissions cannot be rolled back.
			}
			return status
		}
		started++
		// Sibling reuse clients must not overlap their provider queries: a
		// shared MFL producer slot can otherwise appear busy and trigger a
		// duplicate fallback view. Keep one shared request deadline while
		// admitting each group before starting the next independent child.
		result := make(chan string, 1)
		go func() { result <- readFileManagerReceipt(reader) }()
		select {
		case status := <-result:
			reader.Close()
			if status != "accepted" || ctx.Err() != nil {
				return "unknown"
			}
		case <-ctx.Done():
			reader.Close()
			return "unknown"
		}
	}
	return "accepted"
}

func openFileManagerReceipt() (*os.File, error) {
	file := os.NewFile(3, "filemanager-admission")
	if file == nil {
		return nil, errors.New("missing admission descriptor")
	}
	info, err := file.Stat()
	if err != nil || info.Mode()&os.ModeNamedPipe == 0 {
		file.Close()
		return nil, errors.New("admission descriptor must be a pipe")
	}
	// ExtraFiles clears CLOEXEC for this one exec; restore it before any MFL,
	// file worker or application spawn so descendants cannot retain the pipe.
	syscall.CloseOnExec(3)
	return file, nil
}

func writeFileManagerReceipt(file *os.File, status string) {
	if file == nil {
		return
	}
	if _, err := file.Stat(); err != nil {
		return // A previous successful admission already closed this receipt.
	}
	defer file.Close()
	_ = file.SetWriteDeadline(time.Now().Add(100 * time.Millisecond))
	data := []byte(`{"version":1,"status":` + strconv.Quote(status) + "}\n")
	if _, err := file.Write(data); err != nil {
		// Losing the private observer must not destroy an independent view.
		fmt.Fprintln(os.Stderr, "fvwm-file-browser: private admission receipt unavailable")
	}
}

func readFileManagerReceipt(reader io.Reader) string {
	line, err := bufio.NewReaderSize(reader, fileManagerMaxReply+1).ReadSlice('\n')
	if err != nil || len(line) > fileManagerMaxReply+1 {
		return "unknown"
	}
	var receipt struct {
		Version int    `json:"version"`
		Status  string `json:"status"`
	}
	if decodeFileManagerJSON(line, &receipt) != nil || receipt.Version != 1 || receipt.Status != "accepted" {
		return "unknown"
	}
	return "accepted"
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"math/rand"
	"os"
	"os/signal"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"
)

const commandLimit = 32000
const defaultTimeout = 5000
const maxTimeout = 3600000
const retryDelay = 20 * time.Millisecond

func communicationPID(name string) (int, error) {
	text := strings.TrimPrefix(name, "ComExample-")
	if text == "" || len(name) > 240 {
		return 0, errors.New("expected ComExample-PID")
	}
	for _, char := range text {
		if char < '0' || char > '9' {
			return 0, errors.New("expected ComExample-PID")
		}
	}
	pid, err := strconv.ParseInt(text, 10, 32)
	if err != nil || pid <= 1 {
		return 0, errors.New("PID must be between 2 and 2147483647")
	}
	return int(pid), nil
}

type protocol struct{ count uint64 }

func (p *protocol) answer(command string) (string, bool) {
	switch command {
	case "startup":
		return "fvwm-script-ComExample Go helper is up", false
	case "count":
		p.count++
		return fmt.Sprintf("Get %d count messages", p.count), false
	case "multilines":
		var answer strings.Builder
		for _, line := range []string{fmt.Sprintf("line1 (%d)", rand.Intn(20)),
			fmt.Sprintf("Second Line (%d)", rand.Intn(20)), fmt.Sprintf("An other Line (%d)", rand.Intn(20))} {
			fmt.Fprintf(&answer, "%04d%s", len(line), line)
		}
		return answer.String(), false
	case "exit":
		return "", true
	default:
		return "0", false
	}
}

func pause(ctx context.Context, until time.Time) error {
	if !until.IsZero() && !time.Now().Before(until) {
		return context.DeadlineExceeded
	}
	delay := retryDelay
	if !until.IsZero() && time.Until(until) < delay {
		delay = time.Until(until)
	}
	timer := time.NewTimer(delay)
	defer timer.Stop()
	select {
	case <-ctx.Done():
		return ctx.Err()
	case <-timer.C:
		return nil
	}
}

// Every pathname operation is relative to one pinned directory. This executable
// changes directory once before serving, and performs no concurrent file work.
func enterDirectory(path string) (*os.File, error) {
	fd, err := syscall.Open(path, syscall.O_RDONLY|syscall.O_DIRECTORY|syscall.O_NONBLOCK|syscall.O_CLOEXEC, 0)
	if err != nil {
		return nil, err
	}
	directory := os.NewFile(uintptr(fd), path)
	info, err := directory.Stat()
	if err == nil && info.Mode().Perm()&0022 != 0 && info.Mode()&os.ModeSticky == 0 {
		err = errors.New("user directory must not be group/world writable without the sticky bit")
	}
	if err == nil {
		err = syscall.Fchdir(fd)
	}
	if err != nil {
		directory.Close()
		return nil, err
	}
	return directory, nil
}

type endpoint struct {
	name string
	info os.FileInfo
}

func newEndpoint(name string) (*endpoint, error) {
	// Existing entries, including stale pipes, belong to another invocation.
	if err := syscall.Mkfifo(name, 0600); err != nil {
		return nil, err
	}
	info, err := os.Lstat(name)
	if err != nil {
		return nil, err
	}
	e := &endpoint{name, info}
	if !ownedPipe(info) {
		return nil, errors.New("created endpoint was replaced")
	}
	return e, nil
}

func ownedPipe(info os.FileInfo) bool {
	stat, ok := info.Sys().(*syscall.Stat_t)
	return ok && info.Mode()&os.ModeNamedPipe != 0 && int(stat.Uid) == os.Getuid()
}

func (e *endpoint) remove() error {
	info, err := os.Lstat(e.name)
	if err == nil && ownedPipe(info) && os.SameFile(info, e.info) {
		return os.Remove(e.name)
	}
	if err != nil && !os.IsNotExist(err) {
		return err
	}
	return nil
}

func (e *endpoint) open(mode int) (int, error) {
	info, err := os.Lstat(e.name)
	if err != nil {
		return -1, err
	}
	if !ownedPipe(info) || !os.SameFile(info, e.info) {
		return -1, errors.New("endpoint was replaced")
	}
	fd, err := syscall.Open(e.name, mode|syscall.O_NONBLOCK|syscall.O_NOFOLLOW|syscall.O_CLOEXEC, 0)
	if err != nil {
		return -1, err
	}
	var after syscall.Stat_t
	err = syscall.Fstat(fd, &after)
	before := e.info.Sys().(*syscall.Stat_t)
	if err == nil && (after.Mode&syscall.S_IFMT != syscall.S_IFIFO ||
		int(after.Uid) != os.Getuid() || after.Dev != before.Dev || after.Ino != before.Ino) {
		err = errors.New("opened endpoint was replaced")
	}
	if err != nil {
		syscall.Close(fd)
		return -1, err
	}
	return fd, nil
}

func alive(pid int) bool {
	err := syscall.Kill(pid, 0)
	return err == nil || err == syscall.EPERM
}

func receive(ctx context.Context, e *endpoint, pid int, timeout time.Duration) (string, error) {
	fd, err := e.open(syscall.O_RDONLY)
	if err != nil {
		return "", err
	}
	defer syscall.Close(fd)
	data := make([]byte, 0, 64)
	var buffer [1024]byte
	var deadline time.Time
	for {
		if err := ctx.Err(); err != nil {
			return "", err
		}
		if !alive(pid) {
			return "", io.EOF
		}
		if !deadline.IsZero() && !time.Now().Before(deadline) {
			return "", context.DeadlineExceeded
		}
		n, err := syscall.Read(fd, buffer[:])
		if n > 0 {
			if deadline.IsZero() {
				deadline = time.Now().Add(timeout)
			}
			for _, char := range buffer[:n] {
				if char == '\n' {
					return string(data), nil
				}
				if char == 0 || len(data) == commandLimit {
					return "", errors.New("command contains NUL or exceeds 32000 bytes")
				}
				data = append(data, char)
			}
			continue
		}
		if err == nil && len(data) != 0 {
			return string(data), nil
		}
		if err != nil && err != syscall.EINTR && err != syscall.EAGAIN && err != syscall.EWOULDBLOCK {
			return "", err
		}
		// EAGAIN means a writer has connected; an idle pipe returns zero.
		if (err == syscall.EAGAIN || err == syscall.EWOULDBLOCK) && deadline.IsZero() {
			deadline = time.Now().Add(timeout)
		}
		if err := pause(ctx, deadline); err != nil {
			return "", err
		}
	}
}

func reply(ctx context.Context, e *endpoint, pid int, text string, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	fd := -1
	for fd < 0 {
		if !time.Now().Before(deadline) {
			return context.DeadlineExceeded
		}
		if !alive(pid) {
			return io.EOF
		}
		if err := ctx.Err(); err != nil {
			return err
		}
		var err error
		fd, err = e.open(syscall.O_WRONLY)
		if err != nil && err != syscall.ENXIO && err != syscall.EINTR {
			return err
		}
		if fd < 0 {
			if err := pause(ctx, deadline); err != nil {
				return err
			}
		}
	}
	defer syscall.Close(fd)
	data := []byte(text)
	for len(data) != 0 {
		if err := ctx.Err(); err != nil {
			return err
		}
		if !time.Now().Before(deadline) {
			return context.DeadlineExceeded
		}
		n, err := syscall.Write(fd, data)
		if n > 0 {
			data = data[n:]
		} else if err == syscall.EINTR || err == syscall.EAGAIN || err == syscall.EWOULDBLOCK {
			if err := pause(ctx, deadline); err != nil {
				return err
			}
		} else if err != nil {
			return err
		} else {
			return io.ErrShortWrite
		}
	}
	return nil
}

func serve(ctx context.Context, name string, pid int, timeout time.Duration, diagnostics io.Writer) error {
	var p protocol
	for {
		if err := ctx.Err(); err != nil {
			return err
		}
		if !alive(pid) {
			return nil
		}
		input, err := newEndpoint(".tmp-com-in-" + name)
		if err != nil {
			return err
		}
		command, err := receive(ctx, input, pid, timeout)
		if cleanup := input.remove(); cleanup != nil {
			return cleanup
		}
		if errors.Is(err, io.EOF) {
			return nil
		}
		if err != nil {
			if ctx.Err() != nil {
				return ctx.Err()
			}
			if !errors.Is(err, context.DeadlineExceeded) {
				return err
			}
			fmt.Fprintln(diagnostics, "command timed out; waiting for next request")
			continue
		}
		answer, stop := p.answer(command)
		if stop {
			return nil
		}
		output, err := newEndpoint(".tmp-com-out-" + name)
		if err != nil {
			return err
		}
		err = reply(ctx, output, pid, answer, timeout)
		if cleanup := output.remove(); cleanup != nil {
			return cleanup
		}
		if err != nil {
			if errors.Is(err, io.EOF) {
				return nil
			}
			if ctx.Err() != nil {
				return ctx.Err()
			}
			if !errors.Is(err, context.DeadlineExceeded) && err != syscall.EPIPE {
				return err
			}
			fmt.Fprintln(diagnostics, "reply not read; waiting for next request")
		}
	}
}

func run(args []string, out, diagnostics io.Writer) int {
	flags := flag.NewFlagSet("fvwm-script-ComExample", flag.ContinueOnError)
	flags.SetOutput(diagnostics)
	name := flags.String("com-name", "", "communication identifier ComExample-PID")
	ms := flags.Int("timeout-ms", defaultTimeout, "connected command/reply deadline (1–3600000 ms)")
	flags.Usage = func() {
		fmt.Fprintln(out, "Usage: fvwm-script-ComExample --com-name ComExample-PID [--timeout-ms MS]")
	}
	if err := flags.Parse(args); err != nil {
		if err == flag.ErrHelp {
			return 0
		}
		return 1
	}
	pid, err := communicationPID(*name)
	if err != nil || flags.NArg() != 0 || *ms < 1 || *ms > maxTimeout {
		flags.Usage()
		return 1
	}
	path := os.Getenv("FVWM_USERDIR")
	if path == "" {
		home := os.Getenv("HOME")
		if home == "" {
			home = "."
		}
		path = filepath.Join(home, ".fvwm")
	}
	directory, err := enterDirectory(path)
	if err != nil {
		fmt.Fprintln(diagnostics, err)
		return 1
	}
	defer directory.Close()
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM, syscall.SIGHUP)
	defer stop()
	if err := serve(ctx, *name, pid, time.Duration(*ms)*time.Millisecond, diagnostics); err != nil && !errors.Is(err, context.Canceled) {
		fmt.Fprintln(diagnostics, err)
		return 1
	}
	return 0
}

func main() { os.Exit(run(os.Args[1:], os.Stdout, os.Stderr)) }

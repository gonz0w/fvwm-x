// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"context"
	"crypto/rand"
	"crypto/subtle"
	"encoding/hex"
	"errors"
	"net"
	"os"
	"path/filepath"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
	"golang.org/x/sys/unix"
)

type controlRequest struct {
	ctx     context.Context
	message sessionservice.RequestMessage
	reply   chan sessionservice.Reply
}
type ownerControl struct {
	listener               *net.UnixListener
	directory              *os.File
	name, path, capability string
	inode                  unix.Stat_t
	cancel                 context.CancelFunc
	requests               chan controlRequest
}

func randomIdentity(bytes int) (string, error) {
	data := make([]byte, bytes)
	if _, err := rand.Read(data); err != nil {
		return "", err
	}
	return hex.EncodeToString(data), nil
}

func controlDirectory(environment []string) (*os.File, string, error) {
	env := environmentMap(environment)
	if path := env["XDG_RUNTIME_DIR"]; path != "" {
		if directory, err := openDirectory(path, true); err == nil {
			return directory, filepath.Clean(path), nil
		}
	}
	directory, err := cacheDirectory(env)
	if err != nil {
		return nil, "", err
	}
	cache := env["XDG_CACHE_HOME"]
	if !filepath.IsAbs(cache) {
		cache = filepath.Join(env["HOME"], ".cache")
	}
	return directory, filepath.Join(cache, "fvwm3", "session-owner"), nil
}

func startControl(environment []string) (*ownerControl, error) {
	directory, path, err := controlDirectory(environment)
	if err != nil {
		return nil, err
	}
	name, err := randomIdentity(8)
	if err != nil {
		directory.Close()
		return nil, err
	}
	name = ".s" + name
	path = filepath.Join(path, name)
	if len(path) > 103 {
		directory.Close()
		return nil, errors.New("private session control path exceeds Unix socket limit")
	}
	var held, current unix.Stat_t
	if err = unix.Fstat(int(directory.Fd()), &held); err == nil {
		err = unix.Lstat(filepath.Dir(path), &current)
	}
	if err != nil || held.Dev != current.Dev || held.Ino != current.Ino || current.Uid != uint32(os.Geteuid()) || current.Mode&0077 != 0 {
		directory.Close()
		return nil, errors.New("session control directory changed")
	}
	listener, err := net.ListenUnix("unix", &net.UnixAddr{Name: path, Net: "unix"})
	if err != nil {
		directory.Close()
		return nil, err
	}
	listener.SetUnlinkOnClose(false)
	control := &ownerControl{listener: listener, directory: directory, name: name, path: path, requests: make(chan controlRequest, 8)}
	if err = unix.Fstatat(int(directory.Fd()), name, &control.inode, unix.AT_SYMLINK_NOFOLLOW); err != nil {
		listener.Close()
		directory.Close()
		return nil, err
	}
	fail := func(err error) (*ownerControl, error) { control.close(); return nil, err }
	if control.inode.Mode&unix.S_IFMT != unix.S_IFSOCK || control.inode.Uid != uint32(os.Geteuid()) {
		return fail(errors.New("session endpoint is not an owned socket"))
	}
	if err = unix.Fchmodat(int(directory.Fd()), name, 0600, 0); err != nil {
		return fail(err)
	}
	if err = unix.Lstat(filepath.Dir(path), &current); err != nil || held.Dev != current.Dev || held.Ino != current.Ino || current.Uid != uint32(os.Geteuid()) || current.Mode&0077 != 0 {
		return fail(errors.New("session control directory changed during bind"))
	}
	if err = unix.Fstatat(int(directory.Fd()), name, &current, unix.AT_SYMLINK_NOFOLLOW); err != nil || current.Dev != control.inode.Dev || current.Ino != control.inode.Ino || current.Mode&0077 != 0 {
		return fail(errors.New("session control socket changed during bind"))
	}
	control.capability, err = randomIdentity(32)
	if err != nil {
		return fail(err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	control.cancel = cancel
	go control.accept(ctx)
	return control, nil
}

func (control *ownerControl) close() {
	if control.cancel != nil {
		control.cancel()
	}
	control.listener.Close()
	var current unix.Stat_t
	if unix.Fstatat(int(control.directory.Fd()), control.name, &current, unix.AT_SYMLINK_NOFOLLOW) == nil && current.Dev == control.inode.Dev && current.Ino == control.inode.Ino && current.Mode&unix.S_IFMT == unix.S_IFSOCK && current.Uid == uint32(os.Geteuid()) {
		_ = unix.Unlinkat(int(control.directory.Fd()), control.name, 0)
	}
	control.directory.Close()
}

func (control *ownerControl) accept(ctx context.Context) {
	slots := make(chan struct{}, 8)
	for {
		connection, err := control.listener.AcceptUnix()
		if err != nil {
			return
		}
		select {
		case slots <- struct{}{}:
			go func() { defer func() { <-slots }(); control.serve(ctx, connection) }()
		default:
			connection.Close()
		}
	}
}

func (control *ownerControl) serve(parent context.Context, connection *net.UnixConn) {
	defer connection.Close()
	ctx, cancel := context.WithTimeout(parent, 5*time.Second)
	defer cancel()
	deadline, _ := ctx.Deadline()
	connection.SetDeadline(deadline)
	stop := context.AfterFunc(ctx, func() { connection.Close() })
	defer stop()
	envelope, err := sessionservice.ReadRequest(connection)
	if err != nil || subtle.ConstantTimeCompare([]byte(envelope.Capability), []byte(control.capability)) != 1 {
		return
	}
	// At most eight admitted connections have one disconnect monitor each.
	// Client disconnect/extra input cancels only this request, never the WM.
	go func() { var extra [1]byte; connection.Read(extra[:]); cancel() }()
	request := controlRequest{ctx: ctx, message: envelope.Request, reply: make(chan sessionservice.Reply, 1)}
	select {
	case control.requests <- request:
	case <-ctx.Done():
		return
	default:
		_ = sessionservice.WriteReply(connection, sessionservice.Reply{Version: sessionservice.Version, Outcome: "busy", Error: "session control queue is full"})
		return
	}
	select {
	case reply := <-request.reply:
		if ctx.Err() == nil {
			_ = sessionservice.WriteReply(connection, reply)
		}
	case <-ctx.Done():
	}
}

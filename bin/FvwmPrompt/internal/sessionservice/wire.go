// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionservice

import (
	"bytes"
	"context"
	"encoding/binary"
	"encoding/json"
	"errors"
	"io"
	"net"
	"os"
	"path/filepath"
	"time"

	"golang.org/x/sys/unix"
)

type Envelope struct {
	Capability string         `json:"capability"`
	Request    RequestMessage `json:"request"`
}

func readFrame(reader io.Reader, value any) error {
	var header [4]byte
	if _, err := io.ReadFull(reader, header[:]); err != nil {
		return err
	}
	size := binary.BigEndian.Uint32(header[:])
	if size == 0 || size > MaxFrame {
		return errors.New("session frame exceeds limit")
	}
	data := make([]byte, int(size))
	if _, err := io.ReadFull(reader, data); err != nil {
		return err
	}
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(value); err != nil {
		return err
	}
	var extra any
	if err := decoder.Decode(&extra); err != io.EOF {
		return errors.New("extra session frame data")
	}
	return nil
}

func writeFrame(writer io.Writer, value any) error {
	data, err := json.Marshal(value)
	if err != nil {
		return err
	}
	if len(data) == 0 || len(data) > MaxFrame {
		return errors.New("session frame exceeds limit")
	}
	var header [4]byte
	binary.BigEndian.PutUint32(header[:], uint32(len(data)))
	if _, err = io.Copy(writer, bytes.NewReader(header[:])); err != nil {
		return err
	}
	_, err = io.Copy(writer, bytes.NewReader(data))
	return err
}

func ReadRequest(reader io.Reader) (Envelope, error) {
	var value Envelope
	err := readFrame(reader, &value)
	if err == nil {
		err = value.Request.Validate()
	}
	return value, err
}
func WriteReply(writer io.Writer, value Reply) error { return writeFrame(writer, value) }

func socketIdentity(path string) (unix.Stat_t, error) {
	var socket, parent unix.Stat_t
	if !filepath.IsAbs(path) || len(path) > 103 {
		return socket, errors.New("invalid session endpoint")
	}
	if err := unix.Lstat(filepath.Dir(path), &parent); err != nil {
		return socket, err
	}
	if parent.Mode&unix.S_IFMT != unix.S_IFDIR || parent.Uid != uint32(os.Geteuid()) || parent.Mode&0077 != 0 {
		return socket, errors.New("session endpoint parent must be private and owned")
	}
	if err := unix.Lstat(path, &socket); err != nil {
		return socket, err
	}
	if socket.Mode&unix.S_IFMT != unix.S_IFSOCK || socket.Uid != uint32(os.Geteuid()) || socket.Mode&0077 != 0 {
		return socket, errors.New("session endpoint must be a private owned socket")
	}
	return socket, nil
}

// Request uses only the parent's inherited private endpoint and capability.
// Same-UID processes are trusted session peers, not a confidentiality boundary.
func Request(ctx context.Context, message RequestMessage) (Reply, error) {
	return RequestTo(ctx, os.Getenv(SocketEnvironment), os.Getenv(CapabilityEnvironment), message)
}

func RequestTo(ctx context.Context, path, capability string, message RequestMessage) (Reply, error) {
	var reply Reply
	if err := message.Validate(); err != nil {
		return reply, err
	}
	if len(capability) != 64 {
		return reply, errors.New("session owner is unavailable")
	}
	before, err := socketIdentity(path)
	if err != nil {
		return reply, err
	}
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	connection, err := (&net.Dialer{}).DialContext(ctx, "unix", path)
	if err != nil {
		return reply, err
	}
	defer connection.Close()
	deadline, _ := ctx.Deadline()
	if err = connection.SetDeadline(deadline); err != nil {
		return reply, err
	}
	stop := context.AfterFunc(ctx, func() { connection.Close() })
	defer stop()
	after, err := socketIdentity(path)
	if err != nil || before.Dev != after.Dev || before.Ino != after.Ino {
		return reply, errors.New("session endpoint changed while connecting")
	}
	if err = writeFrame(connection, Envelope{capability, message}); err != nil {
		return reply, err
	}
	if err = readFrame(connection, &reply); err != nil {
		return reply, err
	}
	if reply.Version != Version || !Plain(reply.Error, 512) {
		return Reply{}, errors.New("invalid session reply")
	}
	if reply.Error != "" {
		return reply, errors.New(reply.Error)
	}
	if err = reply.Status.Validate(); err != nil {
		return Reply{}, err
	}
	if reply.Generation != reply.Status.Generation || reply.Operation != reply.Status.Operation {
		return Reply{}, errors.New("inconsistent session reply identity")
	}
	return reply, nil
}

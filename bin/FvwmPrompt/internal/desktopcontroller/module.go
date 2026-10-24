// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopcontroller

import (
	"context"
	"encoding/binary"
	"errors"
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"golang.org/x/sys/unix"
)

const (
	newDesk       = 1 << 1
	configInfo    = 1 << 18
	endConfigInfo = 1 << 19
	stringMessage = 1 << 22
	coreReply     = (1 << 31) | (1 << 10)
	queueLimit    = 1024 * 1024
)

type frame struct {
	kind uint64
	body []byte
}

func nextFrame(buffer []byte) (frame, int, error) {
	if len(buffer) < 32 {
		return frame{}, 0, nil
	}
	flag, kind, size := binary.NativeEndian.Uint64(buffer), binary.NativeEndian.Uint64(buffer[8:]), binary.NativeEndian.Uint64(buffer[16:])
	if flag != 0xffffffff || size < 4 || size > 4096 {
		return frame{}, 0, errors.New("Invalid FVWM packet")
	}
	length := int(size) * 8
	if len(buffer) < length {
		return frame{}, 0, nil
	}
	return frame{kind, buffer[32:length]}, length, nil
}
func appendCommand(queue []byte, message string) ([]byte, error) {
	if len(message) > 4096 || strings.ContainsAny(message, "\x00\r\n") {
		return queue, errors.New("Invalid module command")
	}
	length := 24 + len(message)
	if length > queueLimit-len(queue) {
		return queue, errors.New("FVWM command queue exceeded its limit")
	}
	queue = binary.NativeEndian.AppendUint64(queue, 0)
	queue = binary.NativeEndian.AppendUint64(queue, uint64(len(message)))
	queue = append(queue, message...)
	queue = binary.NativeEndian.AppendUint64(queue, 1)
	return queue, nil
}

type Options struct {
	Context                                  context.Context
	Root, StatePath, System, Worker, Capture string
	PNG                                      bool
	Input, Output                            int
}

func Run(options Options) error {
	if options.Input < 0 || options.Output < 0 || options.Input == options.Output || options.Input > 2147483647 || options.Output > 2147483647 {
		return errors.New("Invalid module descriptors")
	}
	for _, fd := range []int{options.Input, options.Output} {
		unix.CloseOnExec(fd)
		if err := unix.SetNonblock(fd, true); err != nil {
			return err
		}
	}
	pending := []byte{}
	d, err := New(options.Root, options.StatePath, options.System, options.Worker, options.Capture, options.PNG, func(message string) error { var err error; pending, err = appendCommand(pending, message); return err })
	if err != nil {
		return err
	}
	defer d.Close()
	d.emit(fmt.Sprintf("SET_MASK %d", stringMessage|newDesk|configInfo|endConfigInfo))
	d.emit(fmt.Sprintf("SET_MASK %d", coreReply))
	d.emit("Send_ConfigInfo *FvwmDesktop")
	buffer := []byte{}
	started := false
	lastStatus := time.Time{}
	chunk := make([]byte, 65536)
	for {
		if options.Context != nil && options.Context.Err() != nil {
			return nil
		}
		if d.fatal != nil {
			return d.fatal
		}
		inputEvents := int16(0)
		if len(buffer) <= queueLimit-len(chunk) {
			inputEvents = unix.POLLIN
		}
		outputEvents := int16(0)
		if len(pending) > 0 {
			outputEvents = unix.POLLOUT
		}
		fds := []unix.PollFd{{Fd: int32(options.Input), Events: inputEvents}, {Fd: int32(options.Output), Events: outputEvents}}
		timeout := 250
		if _, n, err := nextFrame(buffer); n > 0 || err != nil {
			timeout = 0
		}
		if _, err := unix.Poll(fds, timeout); err != nil && err != unix.EINTR {
			return err
		}
		if fds[0].Revents&unix.POLLNVAL != 0 || fds[1].Revents&unix.POLLNVAL != 0 {
			return errors.New("Module descriptor closed")
		}
		if outputEvents != 0 && fds[1].Revents&(unix.POLLOUT|unix.POLLHUP|unix.POLLERR) != 0 {
			n, err := unix.Write(options.Output, pending[:min(8192, len(pending))])
			if err == unix.EPIPE {
				return nil
			}
			if err != nil && err != unix.EAGAIN && err != unix.EINTR {
				return err
			}
			if n > 0 {
				pending = pending[n:]
			}
		}
		if inputEvents != 0 && fds[0].Revents&(unix.POLLIN|unix.POLLHUP|unix.POLLERR) != 0 {
			n, err := unix.Read(options.Input, chunk)
			if err != nil && err != unix.EAGAIN && err != unix.EINTR {
				return err
			}
			if n == 0 && err == nil {
				return nil
			}
			if n > 0 {
				buffer = append(buffer, chunk[:n]...)
			}
		}
		for budget := 0; budget < 64; budget++ {
			packet, n, err := nextFrame(buffer)
			if err != nil {
				return err
			}
			if n == 0 {
				break
			}
			switch packet.kind {
			case stringMessage, configInfo, coreReply:
				if len(packet.body) < 24 {
					return errors.New("Truncated FVWM string packet")
				}
				text, _, _ := strings.Cut(string(packet.body[24:]), "\x00")
				text = strings.ToValidUTF8(text, "\uFFFD")
				if packet.kind == stringMessage {
					d.Handle(text)
				} else if packet.kind == coreReply {
					d.preview.reply(text)
				} else if key, value, ok := desktopConfiguration(text); ok {
					if _, known := desktopsettings.Registry()[key]; known {
						if err := d.configure(key, value); err != nil {
							fmt.Fprintln(os.Stderr, "FvwmDesktop: "+err.Error())
						}
					} else if key == "terminal" {
						d.terminal = detectTerminal(value)
					}
				}
			case newDesk:
				if len(packet.body) >= 8 {
					d.desk = int(binary.NativeEndian.Uint64(packet.body))
					d.highlightDesk()
				}
			case endConfigInfo:
				if !started {
					started = true
					if err := d.finishConfiguration(); err != nil {
						return err
					}
					if err := d.theme(false); err != nil {
						return err
					}
					d.systemMenu()
					d.workerJob("apps", "apps", values{})
					d.emit("NOP FINISHED STARTUP")
				}
			}
			buffer = buffer[n:]
		}
		if started {
			d.tick()
			if time.Since(lastStatus) >= 5*time.Second {
				d.workerJob("status", "status", values{"system": d.system})
				lastStatus = time.Now()
			}
		}
	}
}

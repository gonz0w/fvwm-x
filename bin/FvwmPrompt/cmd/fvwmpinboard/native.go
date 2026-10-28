// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"errors"
	"fmt"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/internal/pinboard"
)

// Exactly one owner query borrows a fresh bounded mailbox. Reader admission
// and retirement share the lock so a retired reply cannot cross into a later
// query, including while the owner is between send and wait.
type nativeReplyGate struct {
	mu      sync.Mutex
	tag     string
	mailbox chan string
}

func (g *nativeReplyGate) begin(tag string) (<-chan string, error) {
	g.mu.Lock()
	defer g.mu.Unlock()
	if tag == "" || g.tag != "" {
		return nil, errors.New("native query already active or missing tag")
	}
	g.tag = tag
	g.mailbox = make(chan string, 16)
	return g.mailbox, nil
}
func (g *nativeReplyGate) retire(tag string) {
	g.mu.Lock()
	defer g.mu.Unlock()
	if g.tag == tag {
		g.tag = ""
		g.mailbox = nil
	}
}
func (g *nativeReplyGate) clear() {
	g.mu.Lock()
	defer g.mu.Unlock()
	g.tag = ""
	g.mailbox = nil
}
func (g *nativeReplyGate) offer(tag, message string) error {
	g.mu.Lock()
	defer g.mu.Unlock()
	if tag == "" || g.tag != tag || g.mailbox == nil {
		return nil
	}
	select {
	case g.mailbox <- message:
		return nil
	default:
		return errors.New("active native reply queue is full")
	}
}

func (b *board) exchange(tag string, send func() error, receive func(string) (bool, error)) error {
	mailbox, err := b.nativeReplies.begin(tag)
	if err != nil {
		return err
	}
	defer b.nativeReplies.retire(tag)
	if err := send(); err != nil {
		return err
	}
	return b.wait(tag, mailbox, receive)
}

func (b *board) send(verb string, args ...string) error {
	command, err := b.protocol.Command(verb, args...)
	if err != nil {
		return err
	}
	return b.session.Send(command)
}
func (b *board) tag(prefix string) (string, error) {
	if b.tagSerial == ^uint64(0) {
		return "", errors.New("native tag generation exhausted")
	}
	b.tagSerial++
	return fmt.Sprintf("%s%x", prefix, b.tagSerial), nil
}
func (b *board) wait(tag string, mailbox <-chan string, receive func(string) (bool, error)) error {
	timer := time.NewTimer(5 * time.Second)
	defer timer.Stop()
	for {
		select {
		case message := <-mailbox:
			fields := strings.Fields(message)
			if len(fields) < 3 || fields[2] != tag {
				continue
			}
			done, err := receive(message)
			if err != nil {
				return err
			}
			if done {
				return nil
			}
		case <-b.renew.C:
			if err := b.send("Renew"); err != nil {
				return err
			}
		case <-b.ctx.Done():
			return b.ctx.Err()
		case <-b.session.Done():
			return errors.New("FvwmMFL disconnected during native acknowledgement")
		case <-timer.C:
			if b.ctx.Err() == nil {
				b.nativeLost = true
			}
			return errors.New("native pinboard acknowledgement timed out")
		}
	}
}

func (b *board) barrier() (status pinboard.Status, err error) {
	tag, err := b.tag("s")
	if err != nil {
		return status, err
	}
	err = b.exchange(tag, func() error { return b.send("Status", tag) }, func(message string) (bool, error) {
		var e error
		status, e = pinboard.ParseStatus(message, b.protocol.Token)
		return true, e
	})
	if err != nil && b.ctx.Err() == nil {
		b.nativeLost = true
	}
	return
}

func (b *board) configure(commands []string) error {
	for offset := 0; offset < len(commands); offset += 32 {
		end := offset + 32
		if end > len(commands) {
			end = len(commands)
		}
		for _, command := range commands[offset:end] {
			if err := b.session.Send(command); err != nil {
				return err
			}
		}
		tag, err := b.tag("f")
		if err != nil {
			return err
		}
		if err := b.exchange(tag, func() error { return b.session.Send("Echo FVWM_PINBOARD_SYNC " + b.protocol.Token + " " + tag) }, func(message string) (bool, error) {
			return message == "FVWM_PINBOARD_SYNC "+b.protocol.Token+" "+tag, nil
		}); err != nil {
			return err
		}
	}
	return nil
}

// publish returns true whenever native acknowledged a committed scene, even
// if a concurrent topology replacement made its acquired pictures obsolete.
// Callers must retain that picture generation until a replacement barrier.
func (b *board) publish(state pinboard.State, observed map[string]filemodel.Observation, pictures map[string]string, expectedEpoch string) (bool, error) {
	if b.nextView >= pinboard.MaxGeneration {
		return false, errors.New("native view generation exhausted")
	}
	b.nextView++
	next := b.nextView
	missing := map[string]bool{}
	for id, o := range observed {
		missing[id] = o.Status != "present"
	}
	commands, err := b.protocol.Scene(b.view, next, state, missing, pictures)
	if err != nil {
		return false, err
	}
	committed := false
	defer func() {
		if b.ctx.Err() != nil {
			return
		}
		if !committed && !b.nativeLost {
			if err := b.send("Abort", strconv.FormatUint(uint64(next), 10)); err != nil {
				b.nativeLost = true
				return
			}
			status, err := b.barrier()
			if err != nil || status.Current != b.view || status.Pending != 0 || status.State != "ready" {
				b.nativeLost = true
				return
			}
			b.epoch, b.window = status.Epoch, status.Window
		}
	}()
	for offset := 0; offset < len(commands); offset += 32 {
		end := offset + 32
		if end > len(commands) {
			end = len(commands)
		}
		for _, command := range commands[offset:end] {
			if err := b.session.Send(command); err != nil {
				return false, err
			}
		}
		status, err := b.barrier()
		if err != nil {
			return false, err
		}
		if end == len(commands) {
			previous := b.view
			until := time.Now().Add(5 * time.Second)
			for {
				if status.Current == next && status.Pending == 0 {
					committed = true
					b.view, b.epoch, b.window = next, status.Epoch, status.Window
					if status.State == "ready" {
						break
					}
				}
				startup := previous == 0 && status.Current == 0 && status.Pending == next && status.State == "unavailable" && status.Window != "0" && status.Epoch == "0000000000000000"
				staging := status.Current == previous && status.Pending == next && status.State == "pending"
				mapping := committed && status.Current == next && status.Pending == 0 && status.State == "pending"
				if !startup && !staging && !mapping {
					// A poisoned transaction can be cleared by Abort while the
					// prior committed scene remains usable. Persistent desktop
					// refusal is distinguished by the subsequent actual barrier.
					if status.State != "refused" {
						b.nativeLost = true
					}
					return committed, fmt.Errorf("native commit/readiness refused: %s", status.State)
				}
				if !time.Now().Before(until) {
					b.nativeLost = true
					return committed, errors.New("native commit/readiness timed out")
				}
				timer := time.NewTimer(10 * time.Millisecond)
				select {
				case <-timer.C:
				case <-b.ctx.Done():
					timer.Stop()
					return committed, b.ctx.Err()
				case <-b.session.Done():
					timer.Stop()
					return committed, errors.New("native readiness disconnected")
				}
				status, err = b.barrier()
				if err != nil {
					return committed, err
				}
			}
			if b.epoch == "0000000000000000" {
				return true, errors.New("committed native scene has unavailable monitor epoch")
			}
			if expectedEpoch != "" && expectedEpoch != b.epoch {
				return true, errors.New("topology changed during image publication; labels retained")
			}
			return true, nil
		}
		if status.State == "refused" || status.State == "unavailable" || status.State == "retired" {
			if status.State != "refused" {
				b.nativeLost = true
			}
			return false, fmt.Errorf("native canvas %s", status.State)
		}
		if status.Current != b.view || status.Pending != next || status.State != "pending" {
			return false, errors.New("native staging refused")
		}
		if expectedEpoch != "" && status.Epoch != expectedEpoch {
			return false, errors.New("topology changed during image staging")
		}
	}
	return false, errors.New("incomplete scene")
}

func (b *board) cells(offset int) (page pinboard.CellPage, err error) {
	if b.icons == nil || b.icons.view != b.view || b.icons.epoch != b.epoch {
		return page, errors.New("icon projection was retired")
	}
	tag, err := b.tag("c")
	if err != nil {
		return page, err
	}
	cells := []pinboard.Cell{}
	err = b.exchange(tag, func() error { return b.send("Cells", tag, strconv.Itoa(offset), "8") }, func(message string) (bool, error) {
		if strings.HasPrefix(message, "FVWM_PINBOARD_CELL ") {
			if len(cells) >= 8 {
				return false, errors.New("native cell page exceeds eight records")
			}
			cell, err := pinboard.ParseCell(message, b.protocol.Token, tag, b.view, b.epoch)
			if err != nil {
				return false, err
			}
			cells = append(cells, cell)
			return false, nil
		}
		var err error
		page, err = pinboard.ParseCellsEnd(message, b.protocol.Token, tag, b.view, b.epoch)
		if err != nil {
			return true, err
		}
		page.Cells = cells
		return true, page.Validate(b.state, offset)
	})
	return
}

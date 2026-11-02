// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package clipboard carries only numeric metadata for one isolated native owner.
// No selection bytes, preview strings, hashes or source titles enter Go.
package clipboard

import (
	"errors"
	"fmt"
	"path/filepath"
	"strconv"
	"strings"
)

const (
	Status uint64 = iota + 1
	List
	Pin
	Remove
	Clear
	State
	Quit
)
const (
	Unknown uint64 = iota
	Unlocked
	Locked
)
const (
	OK uint64 = iota
	Blocked
	Refused
	Stale
	Invalid
	Full
	Fatal
	Empty
)
const replyLimit = 384

type Ref struct {
	Epoch      uint64 `json:"epoch,string"`
	Item       uint64 `json:"item,string"`
	Generation uint64 `json:"generation,string"`
}

type Request struct {
	Verb  uint64 `json:"verb"`
	Ref   Ref    `json:"ref"`
	Value uint64 `json:"value,string"`
}

type Reply struct {
	Sequence       uint64 `json:"sequence,string"`
	Result         uint64 `json:"result"`
	Ref            Ref    `json:"ref"`
	Representation uint64 `json:"representation"`
	Length         uint64 `json:"length,string"`
	Deadline       uint64 `json:"deadline,string"`
	Pinned         uint64 `json:"pinned"`
	Mode           uint64 `json:"mode"`
	Items          uint64 `json:"items,string"`
	Mapped         uint64 `json:"mapped,string"`
	Locked         uint64 `json:"locked,string"`
}

// Config has no installed defaults. Its explicit limits and a qualified lock
// observer are required before installed capture can be enabled. The initial
// native state is always Unknown. State is an internal trusted-parent seam;
// it is never accepted by the general session-control socket.
type Config struct {
	Path                                 string
	Epoch, Items, Bytes, Total           uint64
	Mapped, Locked, LifetimeMilliseconds uint64
}

func (c Config) Validate() error {
	if !filepath.IsAbs(c.Path) || filepath.Clean(c.Path) != c.Path || c.Epoch == 0 || c.Items == 0 || c.Items > 4096 || c.Bytes == 0 || c.Bytes > 32768 || c.Total < c.Bytes || c.Total > c.Items*c.Bytes || c.Mapped == 0 || c.Locked == 0 || c.Locked > c.Mapped || c.LifetimeMilliseconds == 0 {
		return errors.New("clipboard needs an absolute helper and explicit bounded limits")
	}
	return nil
}

func (c Config) arguments() []string {
	result := []string{c.Path, "--serve"}
	for _, value := range []uint64{c.Epoch, c.Items, c.Bytes, c.Total, c.Mapped, c.Locked, c.LifetimeMilliseconds} {
		result = append(result, strconv.FormatUint(value, 10))
	}
	return result
}

func (r Request) Validate(epoch uint64) error {
	if r.Ref.Epoch != epoch || epoch == 0 || r.Verb < Status || r.Verb > Quit {
		return errors.New("invalid clipboard control")
	}
	reference := r.Verb == Pin || r.Verb == Remove
	if reference && (r.Ref.Item == 0 || r.Ref.Generation == 0) || !reference && (r.Ref.Item != 0 || r.Ref.Generation != 0) {
		return errors.New("clipboard control needs its exact reference")
	}
	if r.Verb == Pin && r.Value > 1 || r.Verb == State && r.Value > Locked || r.Verb != Pin && r.Verb != State && r.Verb != List && r.Value != 0 {
		return errors.New("invalid clipboard control value")
	}
	return nil
}

func (r Request) line(sequence uint64) string {
	return fmt.Sprintf("1 %d %d %d %d %d %d\n", sequence, r.Verb, r.Ref.Epoch, r.Ref.Item, r.Ref.Generation, r.Value)
}

func parseReply(line []byte) (Reply, error) {
	var result Reply
	if len(line) == 0 || len(line) > replyLimit || line[len(line)-1] != '\n' {
		return result, errors.New("invalid clipboard frame")
	}
	fields := strings.Split(string(line[:len(line)-1]), " ")
	if len(fields) != 14 {
		return result, errors.New("invalid clipboard field count")
	}
	var values [14]uint64
	for i, field := range fields {
		if field == "" || len(field) > 1 && field[0] == '0' {
			return result, errors.New("invalid clipboard number")
		}
		for _, c := range field {
			if c < '0' || c > '9' {
				return result, errors.New("invalid clipboard number")
			}
		}
		value, err := strconv.ParseUint(field, 10, 64)
		if err != nil {
			return result, errors.New("invalid clipboard number")
		}
		values[i] = value
	}
	if values[0] != 1 || values[1] == 0 || values[2] > Empty || values[3] == 0 || values[6] > 1 || values[7] > 32768 || values[9] > 1 || values[10] > Locked || values[11] > 4096 || values[13] > values[12] {
		return result, errors.New("invalid clipboard metadata")
	}
	result = Reply{Sequence: values[1], Result: values[2], Ref: Ref{values[3], values[4], values[5]}, Representation: values[6], Length: values[7], Deadline: values[8], Pinned: values[9], Mode: values[10], Items: values[11], Mapped: values[12], Locked: values[13]}
	if (result.Ref.Item == 0) != (result.Ref.Generation == 0) || result.Ref.Item == 0 && (result.Representation != 0 || result.Length != 0 || result.Deadline != 0 || result.Pinned != 0) || result.Ref.Item != 0 && (result.Representation != 1 || result.Deadline == 0 || result.Result != OK || result.Mode != Unlocked) || result.Mode != Unlocked && (result.Ref.Item != 0 || result.Items != 0 || result.Mapped != 0 || result.Locked != 0) {
		return Reply{}, errors.New("inconsistent clipboard projection")
	}
	return result, nil
}

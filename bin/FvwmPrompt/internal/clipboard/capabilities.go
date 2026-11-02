// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package clipboard

import (
	"context"
	"errors"
)

const MetadataVersion = 1
const MaxPage = 16
const (
	ActionList uint64 = 1 << iota
	ActionPin
	ActionRemove
	ActionClear
	MetadataActions = ActionList | ActionPin | ActionRemove | ActionClear
)

type Limits struct {
	Items    uint64 `json:"items,string"`
	PerItem  uint64 `json:"per_item_bytes,string"`
	Total    uint64 `json:"total_bytes,string"`
	Mapped   uint64 `json:"mapped_bytes,string"`
	Locked   uint64 `json:"locked_bytes,string"`
	Lifetime uint64 `json:"window_lifetime_ms,string"`
}

// DefaultTextLimits is the owner's selected payload/count/age policy. Physical
// budgets use the same one-rounded-region plus two-guard-page model as the
// native owner. This chooses no process, lock source or enabled configuration.
func DefaultTextLimits(page uint64) (Limits, error) {
	const items, bytes uint64 = 32, 32768
	if page == 0 {
		return Limits{}, errors.New("native page size is unavailable")
	}
	pages := bytes / page
	if bytes%page != 0 {
		pages++
	}
	maximum := ^uint64(0) / page / items
	if maximum < 2 || pages > maximum-2 {
		return Limits{}, errors.New("text page charges overflow")
	}
	result := Limits{Items: items, PerItem: bytes, Total: 1048576, Mapped: (pages + 2) * page * items, Locked: pages * page * items, Lifetime: 600000}
	return result, result.Validate()
}

func (l Limits) Validate() error {
	if l.Items == 0 || l.Items > 4096 || l.PerItem == 0 || l.PerItem > 32768 || l.Total < l.PerItem || l.Total > l.Items*l.PerItem || l.Mapped == 0 || l.Locked == 0 || l.Locked > l.Mapped || l.Lifetime == 0 {
		return errors.New("invalid clipboard limits")
	}
	return nil
}

func (c Config) limits() Limits {
	return Limits{c.Items, c.Bytes, c.Total, c.Mapped, c.Locked, c.LifetimeMilliseconds}
}

// Capabilities describes the configured contract and one native observation.
// Limits are admission ceilings, not an entitlement to future OS locking.
// ProcessPrepared attests completed startup preflight only; every later native
// allocation/access still validates its own protection and exact reference.
type Capabilities struct {
	Version               int     `json:"version"`
	State                 string  `json:"state"`
	Reason                string  `json:"reason"`
	Epoch                 uint64  `json:"epoch,string"`
	Mode                  uint64  `json:"mode"`
	Limits                *Limits `json:"limits,omitempty"`
	SupportedActions      uint64  `json:"supported_actions"`
	ProcessPrepared       bool    `json:"process_prepared"`
	RequiredStorage       string  `json:"required_storage,omitempty"`
	Format                string  `json:"format,omitempty"`
	XlibReplyMax          uint64  `json:"xlib_reply_max,string"`
	XlibReplySwapExcluded bool    `json:"xlib_reply_swap_excluded"`
}

func DisabledCapabilities() Capabilities {
	return Capabilities{Version: MetadataVersion, State: "disabled", Reason: "not-configured"}
}

func (c Capabilities) Validate() error {
	if c.Version != MetadataVersion || c.Mode > Locked || c.XlibReplySwapExcluded {
		return errors.New("invalid clipboard capability version or protection claim")
	}
	if c.State == "disabled" {
		if c.Reason != "not-configured" || c.Epoch != 0 || c.Mode != Unknown || c.Limits != nil || c.SupportedActions != 0 || c.ProcessPrepared || c.RequiredStorage != "" || c.Format != "" || c.XlibReplyMax != 0 {
			return errors.New("invalid disabled clipboard capability")
		}
		return nil
	}
	if c.Epoch == 0 || c.Limits == nil || c.Limits.Validate() != nil || c.SupportedActions != MetadataActions || c.RequiredStorage != "native-locked-closed" || c.Format != "UTF8_STRING" || c.XlibReplyMax != 32768 {
		return errors.New("invalid configured clipboard capability")
	}
	switch c.State {
	case "ready":
		if c.Reason != "ready" || c.Mode != Unlocked || !c.ProcessPrepared {
			return errors.New("inconsistent ready clipboard capability")
		}
	case "blocked":
		if !c.ProcessPrepared || c.Mode != Unknown && c.Mode != Locked || c.Mode == Unknown && c.Reason != "unknown-lock" || c.Mode == Locked && c.Reason != "locked" {
			return errors.New("inconsistent blocked clipboard capability")
		}
	case "unavailable":
		if (c.Reason != "native-status-unconfirmed" && c.Reason != "native-owner-exited" && c.Reason != "native-owner-closed" && c.Reason != "native-reap-unconfirmed") || c.Mode != Unknown || c.ProcessPrepared {
			return errors.New("inconsistent unavailable clipboard capability")
		}
	default:
		return errors.New("unknown clipboard capability state")
	}
	return nil
}

func (c *Client) status(ctx context.Context) (Reply, error) {
	reply, err := c.Call(ctx, Request{Verb: Status, Ref: Ref{Epoch: c.epoch}})
	if err == nil && (reply.Result != OK || reply.Ref.Item != 0) {
		c.Close()
		return Reply{}, errors.New("invalid native clipboard status")
	}
	return reply, err
}

func (c *Client) Capabilities(ctx context.Context) Capabilities {
	limits := c.config.limits()
	result := Capabilities{Version: MetadataVersion, Epoch: c.epoch, Limits: &limits, SupportedActions: MetadataActions, RequiredStorage: "native-locked-closed", Format: "UTF8_STRING", XlibReplyMax: 32768, State: "unavailable", Reason: "native-status-unconfirmed"}
	reply, err := c.status(ctx)
	if err != nil || reply.Result != OK {
		select {
		case <-c.done:
			switch {
			case !c.terminal.Started:
				result.Reason = "native-owner-closed"
			case c.terminal.Reaped:
				result.Reason = "native-owner-exited"
			default:
				result.Reason = "native-reap-unconfirmed"
			}
		default:
		}
		return result
	}
	result.ProcessPrepared = true
	result.Mode = reply.Mode
	switch reply.Mode {
	case Unlocked:
		result.State, result.Reason = "ready", "ready"
	case Locked:
		result.State, result.Reason = "blocked", "locked"
	default:
		result.State, result.Reason = "blocked", "unknown-lock"
	}
	return result
}

type PageRequest struct {
	Epoch  uint64 `json:"epoch,string"`
	Offset uint64 `json:"offset,string"`
	Limit  uint64 `json:"limit,string"`
}

func (r PageRequest) Validate() error {
	if r.Epoch == 0 || r.Offset > 4096 || r.Limit == 0 || r.Limit > MaxPage {
		return errors.New("invalid clipboard page")
	}
	return nil
}

type Descriptor struct {
	Slot           uint64 `json:"slot,string"`
	Ref            Ref    `json:"ref"`
	Representation uint64 `json:"representation"`
	Length         uint64 `json:"length,string"`
	Deadline       uint64 `json:"deadline,string"`
	Pinned         bool   `json:"pinned"`
}
type Page struct {
	Version  int          `json:"version"`
	Epoch    uint64       `json:"epoch,string"`
	Offset   uint64       `json:"offset,string"`
	Next     uint64       `json:"next,string"`
	Slots    uint64       `json:"configured_slots,string"`
	Complete bool         `json:"complete"`
	Result   uint64       `json:"result"`
	Items    []Descriptor `json:"items"`
}

func (p Page) Validate(request PageRequest) error {
	if request.Validate() != nil || p.Version != MetadataVersion || p.Epoch != request.Epoch || p.Offset != request.Offset || p.Slots == 0 || p.Slots > 4096 || p.Offset > p.Slots || p.Next < p.Offset || p.Next > p.Slots || p.Next-p.Offset > request.Limit || p.Complete != (p.Next == p.Slots) || len(p.Items) > MaxPage || uint64(len(p.Items)) > request.Limit || p.Result > Empty {
		return errors.New("invalid clipboard page result")
	}
	if p.Result == OK {
		expected := p.Offset + request.Limit
		if expected > p.Slots {
			expected = p.Slots
		}
		if p.Next != expected {
			return errors.New("clipboard page did not advance through its configured slots")
		}
	}
	if p.Result != OK && (len(p.Items) != 0 || p.Next != p.Offset) {
		return errors.New("refused clipboard page has descriptors")
	}
	var previous uint64
	for i, item := range p.Items {
		if item.Slot < p.Offset || item.Slot >= p.Next || i > 0 && item.Slot <= previous || item.Ref.Epoch != p.Epoch || item.Ref.Item == 0 || item.Ref.Generation == 0 || item.Representation != 1 || item.Length > 32768 || item.Deadline == 0 {
			return errors.New("invalid clipboard descriptor")
		}
		previous = item.Slot
	}
	return nil
}

// Page observes at most MaxPage configured slots; it never grants a payload
// lease or promises a transaction across concurrent capture/expiry. Mutations
// still use each exact reference. Any blocked observation discards this page.
func (c *Client) Page(ctx context.Context, request PageRequest) (Page, error) {
	result := Page{Version: MetadataVersion, Epoch: request.Epoch, Offset: request.Offset, Next: request.Offset, Slots: c.config.Items, Items: make([]Descriptor, 0)}
	if err := request.Validate(); err != nil {
		return result, err
	}
	if request.Offset > c.config.Items {
		return result, errors.New("clipboard page exceeds configured slots")
	}
	result.Complete = request.Offset == c.config.Items
	if request.Epoch != c.epoch {
		result.Result = Stale
		return result, nil
	}
	blocked := func() (bool, error) {
		reply, err := c.status(ctx)
		if err != nil {
			return false, err
		}
		return reply.Result != OK || reply.Mode != Unlocked, nil
	}
	if denied, err := blocked(); denied || err != nil {
		result.Result = Blocked
		return result, err
	}
	end := request.Offset + request.Limit
	if end > c.config.Items {
		end = c.config.Items
	}
	for slot := request.Offset; slot < end; slot++ {
		reply, err := c.Call(ctx, Request{Verb: List, Ref: Ref{Epoch: c.epoch}, Value: slot})
		if err != nil {
			return Page{}, err
		}
		if reply.Mode != Unlocked || reply.Result == Blocked {
			result.Result = Blocked
			result.Items = result.Items[:0]
			return result, nil
		}
		if reply.Result == Empty {
			continue
		}
		if reply.Result != OK || reply.Ref.Item == 0 {
			c.Close()
			return Page{}, errors.New("native clipboard returned invalid list metadata")
		}
		result.Items = append(result.Items, Descriptor{slot, reply.Ref, reply.Representation, reply.Length, reply.Deadline, reply.Pinned != 0})
	}
	if denied, err := blocked(); denied || err != nil {
		result.Result = Blocked
		result.Items = result.Items[:0]
		return result, err
	}
	result.Next = end
	result.Complete = end == c.config.Items
	return result, nil
}

// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

// Package sessionservice defines the private session-owner control contract.
// It carries fixed lifecycle verbs, never FVWM commands or executable arguments.
package sessionservice

import (
	"encoding/hex"
	"errors"
	"strings"
	"unicode"
	"unicode/utf8"
)

const (
	Version               = 1
	MaxFrame              = 16 * 1024
	SocketEnvironment     = "FVWM_SESSION_CONTROL_SOCKET"
	CapabilityEnvironment = "FVWM_SESSION_CONTROL_CAPABILITY"
)

type Config struct {
	Bus           string `json:"session_bus"`
	Notifications string `json:"session_notifications"`
	DND           bool   `json:"notification_dnd"`
}

func Defaults() Config { return Config{Bus: "off", Notifications: "off"} }
func (c Config) Validate() error {
	if c.Bus != "off" && c.Bus != "reuse" && c.Bus != "start" || c.Notifications != "off" && c.Notifications != "external" {
		return errors.New("invalid session configuration")
	}
	return nil
}

type Item struct {
	ID        string `json:"id"`
	State     string `json:"state"`
	Ownership string `json:"ownership"`
	Detail    string `json:"detail,omitempty"`
	Operation string `json:"operation,omitempty"`
	Attempts  uint64 `json:"attempts"`
	Started   int    `json:"started,omitempty"`
	Exited    int    `json:"exited,omitempty"`
	Failed    int    `json:"failed,omitempty"`
}

type Snapshot struct {
	Version         int    `json:"version"`
	Generation      string `json:"generation"`
	Operation       string `json:"operation"`
	Config          Config `json:"config"`
	ActiveBusPolicy string `json:"active_bus_policy"`
	Items           []Item `json:"items"`
}

type RequestMessage struct {
	Version        int     `json:"version"`
	Generation     string  `json:"generation,omitempty"`
	Operation      string  `json:"operation,omitempty"`
	Verb           string  `json:"verb"`
	Config         *Config `json:"config,omitempty"`
	Summary        string  `json:"summary,omitempty"`
	Body           string  `json:"body,omitempty"`
	NotificationID uint32  `json:"notification_id,omitempty"`
}

type Reply struct {
	Version        int      `json:"version"`
	Generation     string   `json:"generation,omitempty"`
	Operation      string   `json:"operation,omitempty"`
	Outcome        string   `json:"outcome"`
	Error          string   `json:"error,omitempty"`
	NotificationID uint32   `json:"notification_id,omitempty"`
	Status         Snapshot `json:"status"`
}

func Identity(value string) bool {
	if len(value) != 32 {
		return false
	}
	_, err := hex.DecodeString(value)
	return err == nil
}

func Plain(value string, max int) bool {
	if len(value) > max || !utf8.ValidString(value) {
		return false
	}
	for _, r := range value {
		if unicode.IsControl(r) {
			return false
		}
	}
	return true
}

func (r RequestMessage) Validate() error {
	if r.Version != Version {
		return errors.New("unsupported session request version")
	}
	if r.Verb != "status" && (!Identity(r.Generation) || !Identity(r.Operation)) {
		return errors.New("session controls need current generation and operation")
	}
	if r.Verb == "status" && (r.Generation != "" && !Identity(r.Generation) || r.Operation != "" && !Identity(r.Operation)) {
		return errors.New("invalid status identity")
	}
	if r.Verb != "configure" && r.Config != nil || r.Verb != "notify" && (r.Summary != "" || r.Body != "") || r.Verb != "close" && r.NotificationID != 0 {
		return errors.New("fields do not belong to session verb")
	}
	switch r.Verb {
	case "status", "reprobe":
	case "configure":
		if r.Config == nil {
			return errors.New("missing session configuration")
		}
		return r.Config.Validate()
	case "notify":
		if strings.TrimSpace(r.Summary) == "" || !Plain(r.Summary, 160) || !Plain(r.Body, 1024) {
			return errors.New("notification needs bounded plain text")
		}
	case "close":
		if r.NotificationID == 0 {
			return errors.New("missing owned notification ID")
		}
	default:
		return errors.New("unknown session lifecycle verb")
	}
	return nil
}

func (s Snapshot) Validate() error {
	if s.Version != Version || !Identity(s.Generation) || !Identity(s.Operation) || len(s.Items) != 5 {
		return errors.New("invalid session inventory")
	}
	if err := s.Config.Validate(); err != nil {
		return err
	}
	if s.ActiveBusPolicy != "off" && s.ActiveBusPolicy != "reuse" && s.ActiveBusPolicy != "start" {
		return errors.New("invalid active bus policy")
	}
	for i, id := range []string{"core", "scan", "bus", "notifications", "applications"} {
		item := s.Items[i]
		if item.ID != id || !Plain(item.Detail, 512) || item.Operation != "" && !Identity(item.Operation) || item.Started < 0 || item.Exited < 0 || item.Failed < 0 || item.Started > 256 || item.Exited > 256 || item.Failed > 256 {
			return errors.New("invalid session inventory item")
		}
		switch item.State {
		case "installed", "enabled", "starting", "ready", "unavailable", "failed", "stopping", "stopped":
		default:
			return errors.New("invalid session state")
		}
		switch item.Ownership {
		case "owned", "borrowed", "independent", "none":
		default:
			return errors.New("invalid session ownership")
		}
	}
	return nil
}

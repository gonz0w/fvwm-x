// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"
	"unicode"
	"unicode/utf8"

	"github.com/fvwmorg/FvwmPrompt/internal/desktopsettings"
	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
	"github.com/fvwmorg/FvwmPrompt/internal/xdgsource"
	"golang.org/x/sys/unix"
)

type busReply struct {
	Version        int      `json:"version"`
	State          string   `json:"state"`
	Available      bool     `json:"available"`
	BusID          string   `json:"bus_id"`
	ServerOwner    string   `json:"server_owner"`
	Capabilities   []string `json:"capabilities"`
	NotificationID uint32   `json:"notification_id"`
	Detail         string   `json:"detail"`
}

type limitedServiceOutput struct {
	data     [sessionservice.MaxFrame]byte
	size     int
	overflow bool
}

func (b *limitedServiceOutput) Write(data []byte) (int, error) {
	n := copy(b.data[b.size:], data)
	b.size += n
	b.overflow = b.overflow || n != len(data)
	return len(data), nil
}

func detail(value string) string {
	value = strings.Map(func(r rune) rune {
		if unicode.IsControl(r) {
			return ' '
		}
		return r
	}, value)
	if len(value) > 512 {
		value = value[:512]
		for !utf8.ValidString(value) {
			value = value[:len(value)-1]
		}
	}
	return value
}

func readSessionConfig(environment []string) (sessionservice.Config, error) {
	config := sessionservice.Defaults()
	env := environmentMap(environment)
	userdir := env["FVWM_USERDIR"]
	if userdir == "" {
		userdir = filepath.Join(env["HOME"], ".fvwm")
	}
	if !filepath.IsAbs(userdir) {
		return config, errors.New("session settings directory is not absolute")
	}
	data, err := xdgsource.ReadSettings(filepath.Join(userdir, "desktop-state.json"))
	if errors.Is(err, os.ErrNotExist) {
		return config, nil
	}
	if err != nil {
		return config, err
	}
	values, err := desktopsettings.DecodeState(data)
	if err != nil {
		return config, err
	}
	if value, ok := values["session_bus"].(string); ok {
		config.Bus = value
	}
	if value, ok := values["session_notifications"].(string); ok {
		config.Notifications = value
	}
	if value, ok := values["session_power"].(string); ok {
		config.Power = value
	}
	if value, ok := values["notification_dnd"].(bool); ok {
		config.DND = value
	}
	return config, config.Validate()
}

func serviceHelper(parent context.Context, cfg Config, environment []string, args ...string) (busReply, error) {
	if err := parent.Err(); err != nil {
		return busReply{}, err
	}
	cfg = defaults(cfg)
	var reply busReply
	if !filepath.IsAbs(cfg.BusHelperPath) || filepath.Clean(cfg.BusHelperPath) != cfg.BusHelperPath {
		return reply, errors.New("optional session bus helper is not installed")
	}
	address := environmentMap(environment)["DBUS_SESSION_BUS_ADDRESS"]
	if address != "" && (!sessionservice.Plain(address, 4096) || !strings.HasPrefix(address, "unix:") || strings.Contains(address, ";")) {
		return reply, errors.New("session adapter requires one explicit Unix bus address; no autolaunch")
	}
	if address == "" && (len(args) != 1 || args[0] != "probe") {
		return reply, errors.New("missing explicit session bus address")
	}
	ctx, cancel := context.WithTimeout(parent, 2*time.Second)
	defer cancel()
	select {
	case cfg.serviceSlot <- struct{}{}:
	default:
		return reply, errors.New("previous session bus helper is still being reaped")
	}
	if err := ctx.Err(); err != nil {
		<-cfg.serviceSlot
		return reply, err
	}
	command := cfg.Command(cfg.BusHelperPath, args...)
	command.Env = environment
	var output limitedServiceOutput
	command.Stdout = &output
	command.Stderr = io.Discard
	command.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := ctx.Err(); err != nil {
		<-cfg.serviceSlot
		return reply, err
	}
	if err := command.Start(); err != nil {
		<-cfg.serviceSlot
		return reply, err
	}
	done := make(chan error, 1)
	go func() { err := command.Wait(); <-cfg.serviceSlot; done <- err }()
	select {
	case err := <-done:
		if err != nil {
			return reply, fmt.Errorf("session bus helper failed: %w", err)
		}
	case <-ctx.Done():
		_ = command.Process.Kill()
		return reply, ctx.Err()
	}
	if output.overflow {
		return reply, errors.New("session bus helper output exceeds limit")
	}
	decoder := json.NewDecoder(bytes.NewReader(output.data[:output.size]))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(&reply); err != nil {
		return busReply{}, err
	}
	var extra any
	if err := decoder.Decode(&extra); err != io.EOF {
		return busReply{}, errors.New("extra bus helper output")
	}
	if reply.Version != 1 || !sessionservice.Plain(reply.BusID, 128) || !sessionservice.Plain(reply.ServerOwner, 255) || !sessionservice.Plain(reply.Detail, 512) || len(reply.Capabilities) > 32 {
		return busReply{}, errors.New("invalid session bus helper reply")
	}
	for _, capability := range reply.Capabilities {
		if !sessionservice.Plain(capability, 128) {
			return busReply{}, errors.New("invalid notification capability")
		}
	}
	switch reply.State {
	case "ready", "unavailable", "failed", "sent", "closed":
	default:
		return busReply{}, errors.New("invalid session bus helper state")
	}
	if !reply.Available || reply.State == "unavailable" || reply.State == "failed" {
		return reply, errors.New("optional bus adapter: " + reply.Detail)
	}
	if reply.BusID == "" || reply.ServerOwner != "" && !strings.HasPrefix(reply.ServerOwner, ":") {
		return busReply{}, errors.New("missing authenticated bus/server identity")
	}
	return reply, nil
}

// finishServiceWork is the owner's final barrier, before CLI exit or owned-bus
// retirement. Cancellation reaches the active helper first; its actual Wait
// releases the admission lease. A kernel-delayed exit is reported explicitly.
func finishServiceWork(cfg Config, cancel context.CancelFunc, job <-chan serviceJob, report *reporter) {
	cancel()
	timer := time.NewTimer(3 * time.Second)
	defer timer.Stop()
	if job != nil {
		select {
		case <-job:
		case <-timer.C:
			report.write("owned session adapter shutdown/reaping is unconfirmed")
			return
		}
	}
	select {
	case cfg.serviceSlot <- struct{}{}:
		<-cfg.serviceSlot
	case <-timer.C:
		report.write("owned session adapter shutdown/reaping is unconfirmed")
	}
}

func daemonPath(environment []string) (string, error) {
	path := environmentMap(environment)["PATH"]
	if len(path) > 64*1024 {
		return "", errors.New("PATH exceeds session lookup limit")
	}
	roots := strings.Split(path, ":")
	if len(roots) > 128 {
		return "", errors.New("too many session executable search roots")
	}
	for _, root := range roots {
		if !filepath.IsAbs(root) || len(root) > 4096 {
			continue
		}
		candidate := filepath.Join(root, "dbus-daemon")
		info, err := os.Stat(candidate)
		if len(candidate) <= 4096 && err == nil && info.Mode().IsRegular() && info.Mode().Perm()&0111 != 0 && unix.Access(candidate, unix.X_OK) == nil {
			return candidate, nil
		}
	}
	return "", errors.New("dbus-daemon is not installed in an absolute PATH directory")
}

func busEnvironment(environment []string, address string) []string {
	result := make([]string, 0, len(environment)+1)
	for _, entry := range environment {
		if key, _, _ := strings.Cut(entry, "="); key != "DBUS_SESSION_BUS_ADDRESS" {
			result = append(result, entry)
		}
	}
	return append(result, "DBUS_SESSION_BUS_ADDRESS="+address)
}

type serviceState struct {
	snapshot sessionservice.Snapshot
	bus      *ownedBus
	busLost  bool
	identity busReply
	ids      map[uint32]bool
	plans    providerPlans
	locker   *ownedLocker
}

func newServices(cfg *Config, report *reporter) (*serviceState, error) {
	generation, err := randomIdentity(16)
	if err != nil {
		return nil, err
	}
	operation, err := randomIdentity(16)
	if err != nil {
		return nil, err
	}
	config, configErr := readSessionConfig(cfg.Environment)
	state := &serviceState{snapshot: sessionservice.Snapshot{Version: sessionservice.Version, Generation: generation, Operation: operation, Config: config, ActiveBusPolicy: config.Bus, Items: []sessionservice.Item{
		{ID: "core", State: "starting", Ownership: "owned"}, {ID: "scan", State: "enabled", Ownership: "owned"}, {ID: "bus", State: "stopped", Ownership: "none"}, {ID: "notifications", State: "stopped", Ownership: "borrowed"}, {ID: "applications", State: "enabled", Ownership: "independent"},
	}}, ids: make(map[uint32]bool)}
	state.snapshot.Power = unavailablePower(config.Power, generation, "provider probe pending after core startup")
	state.snapshot.Locker = sessionservice.LockerObservation{Generation: generation, State: "unavailable", Detail: "locker discovery pending; secure readiness unconfirmed"}
	if cfg.NoAutostart {
		state.snapshot.Items[1].State = "stopped"
		state.snapshot.Items[4].State = "stopped"
		state.snapshot.Items[4].Detail = "autostart disabled for this login"
	}
	if configErr != nil {
		report.write("session settings unavailable: %v", configErr)
	}
	if config.Notifications == "external" {
		state.snapshot.Items[3].State = "enabled"
	}
	if config.Bus == "off" {
		state.snapshot.Items[2].Detail = "bus policy off; inherited environment preserved"
		return state, nil
	}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	state.snapshot.Items[2].Attempts = 1
	var probe busReply
	probeErr := errors.New("no inherited session bus address")
	if environmentMap(cfg.Environment)["DBUS_SESSION_BUS_ADDRESS"] != "" {
		probe, probeErr = serviceHelper(ctx, *cfg, cfg.Environment, "probe")
	}
	if probeErr == nil {
		state.adoptProbe(probe, "borrowed")
		return state, nil
	}
	if config.Bus == "reuse" {
		state.snapshot.Items[2].State = "unavailable"
		state.snapshot.Items[2].Detail = detail(probeErr.Error())
		return state, nil
	}
	// Feature discovery never connects: native probe with an empty address
	// distinguishes compiled support from the default disabled worker stub.
	feature, featureErr := serviceHelper(ctx, *cfg, busEnvironment(cfg.Environment, ""), "probe")
	if !feature.Available || feature.State != "unavailable" {
		state.snapshot.Items[2].State = "unavailable"
		message := "optional session bus feature is disabled"
		if featureErr != nil {
			message = featureErr.Error()
		}
		state.snapshot.Items[2].Detail = detail(message)
		return state, nil
	}
	bus, address, err := startBus(ctx, *cfg)
	state.bus = bus // Retain actual ownership even when startup cannot be confirmed.
	if bus != nil {
		state.snapshot.Items[2].Ownership = "owned"
	}
	if err != nil {
		state.snapshot.Items[2].State = "failed"
		state.snapshot.Items[2].Detail = detail(err.Error())
		return state, nil
	}
	environment := busEnvironment(cfg.Environment, address)
	probe, err = serviceHelper(ctx, *cfg, environment, "probe")
	if err == nil && bus.ended() {
		err = errors.New("owned session bus ended during authentication")
	}
	if err != nil {
		bus.stop()
		state.snapshot.Items[2].State = "failed"
		state.snapshot.Items[2].Detail = detail(err.Error())
		return state, nil
	}
	cfg.Environment = environment
	state.adoptProbe(probe, "owned")
	return state, nil
}

func (state *serviceState) adoptProbe(probe busReply, ownership string) {
	if state.bus != nil && state.bus.ended() {
		state.fenceBusLoss()
		return
	}
	if state.identity.BusID != probe.BusID || state.identity.ServerOwner != probe.ServerOwner {
		state.ids = make(map[uint32]bool)
	}
	state.identity = probe
	state.snapshot.Items[2].State = "ready"
	state.snapshot.Items[2].Ownership = ownership
	state.snapshot.Items[2].Detail = "authenticated session bus"
	identity := sha256.Sum256([]byte(probe.BusID + "\x00" + probe.ServerOwner))
	state.snapshot.Items[3].Operation = hex.EncodeToString(identity[:16])
	state.refreshNotifications()
}

func (state *serviceState) refreshNotifications() {
	probe := state.identity
	state.snapshot.Items[3].Ownership = "borrowed"
	if state.snapshot.Config.Notifications == "off" {
		state.snapshot.Items[3].State = "stopped"
		state.snapshot.Items[3].Detail = "notifications disabled"
	} else if state.snapshot.Items[2].State != "ready" {
		state.snapshot.Items[3].State = "unavailable"
		state.snapshot.Items[3].Detail = "session bus has not been authenticated"
	} else if probe.ServerOwner == "" {
		state.snapshot.Items[3].State = "unavailable"
		state.snapshot.Items[3].Detail = "no existing notification server"
	} else {
		state.snapshot.Items[3].State = "ready"
		state.snapshot.Items[3].Detail = "existing notification server; outbound plain text only"
	}
}

type serviceJob struct {
	request controlRequest
	probe   busReply
	result  busReply
	err     error
	outcome string
	power   sessionservice.PowerObservation
	plans   providerPlans
	locker  *ownedLocker
}

func runServiceJob(request controlRequest, cfg Config, identity busReply, ownedIDs int) serviceJob {
	job := serviceJob{request: request, outcome: "failed"}
	address := environmentMap(cfg.Environment)["DBUS_SESSION_BUS_ADDRESS"]
	if address == "" {
		job.err = errors.New("no session bus address; no automatic launch")
		return job
	}
	probe, err := serviceHelper(request.ctx, cfg, cfg.Environment, "probe")
	job.probe = probe
	if err != nil {
		job.err = err
		return job
	}
	if err := request.ctx.Err(); err != nil {
		job.err = err
		return job
	}
	if identity.BusID != "" && identity.BusID != probe.BusID {
		job.err = errors.New("session bus identity changed; existing apps retain the original address")
		return job
	}
	if request.message.Verb == "reprobe" {
		job.outcome = "ready"
		return job
	}
	if probe.ServerOwner == "" {
		job.err = errors.New("no existing notification server")
		job.outcome = "unavailable"
		return job
	}
	if request.message.Verb == "notify" && identity.ServerOwner == probe.ServerOwner && identity.BusID == probe.BusID && ownedIDs >= 32 {
		job.err = errors.New("session owns its maximum 32 notification IDs")
		return job
	}
	args := []string{"notify", request.message.Summary, request.message.Body, probe.ServerOwner, probe.BusID}
	if request.message.Verb == "close" {
		if identity.ServerOwner != probe.ServerOwner || identity.BusID != probe.BusID {
			job.err = errors.New("notification server owner changed; old IDs are unavailable")
			return job
		}
		args = []string{"close", strconv.FormatUint(uint64(request.message.NotificationID), 10), probe.ServerOwner, probe.BusID}
	}
	job.result, job.err = serviceHelper(request.ctx, cfg, cfg.Environment, args...)
	if job.err == nil && (job.result.BusID != probe.BusID || job.result.ServerOwner != probe.ServerOwner) {
		job.err = errors.New("notification reply changed server identity")
	}
	if job.err == nil && request.message.Verb == "notify" && (job.result.State != "sent" || job.result.NotificationID == 0) {
		job.err = errors.New("notification server did not return a sent notification ID")
	}
	if job.err == nil && request.message.Verb == "close" && job.result.State != "closed" {
		job.err = errors.New("notification close was not acknowledged")
	}
	if job.err == nil {
		job.outcome = job.result.State
	}
	return job
}

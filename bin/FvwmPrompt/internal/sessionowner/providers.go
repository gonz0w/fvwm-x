// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package sessionowner

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"syscall"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/sessionprovider"
	"github.com/fvwmorg/FvwmPrompt/internal/sessionservice"
	"golang.org/x/sys/unix"
)

const ProviderWorkerArgument = "--_session-providers"

type providerPlans struct {
	Poweroff []string `json:"poweroff"`
	Reboot   []string `json:"reboot"`
	Locker   []string `json:"locker"`
}

// ProviderDiscovery only resolves fixed plans. It never launches a command,
// so killing this direct self-worker cannot strand a helper grandchild.
func ProviderDiscovery(environment []string, output io.Writer) error {
	path := environmentMap(environment)["PATH"]
	if len(path) > 64*1024 || len(strings.Split(path, ":")) > 128 {
		return errors.New("provider PATH exceeds limit")
	}
	lookup := func(name string) (string, error) {
		for _, root := range strings.Split(path, ":") {
			if !filepath.IsAbs(root) || len(root) > 4096 || filepath.Clean(root) != root {
				continue
			}
			candidate := filepath.Join(root, name)
			info, err := os.Stat(candidate)
			if err == nil && info.Mode().IsRegular() && info.Mode().Perm()&0111 != 0 && unix.Access(candidate, unix.X_OK) == nil {
				return candidate, nil
			}
		}
		return "", errors.New("fixed provider executable unavailable")
	}
	system := map[string]string{"linux": "Linux", "freebsd": "FreeBSD", "openbsd": "OpenBSD"}[runtime.GOOS]
	plans := providerPlans{}
	plans.Poweroff, _ = sessionprovider.NativePlan(system, "poweroff", lookup)
	plans.Reboot, _ = sessionprovider.NativePlan(system, "reboot", lookup)
	plans.Locker, _ = sessionprovider.LockerPlan(lookup)
	return json.NewEncoder(output).Encode(plans)
}

func validPlan(plan []string) bool {
	if len(plan) == 0 {
		return true
	}
	if len(plan) > 4 || !filepath.IsAbs(plan[0]) || filepath.Clean(plan[0]) != plan[0] {
		return false
	}
	for _, arg := range plan {
		if !sessionservice.Plain(arg, 4096) {
			return false
		}
	}
	return true
}

// boundedProviderChild retains the transient lease until actual Wait, including
// after cancellation. started distinguishes refusal from uncertain submission.
func boundedProviderChild(parent context.Context, cfg Config, path string, args ...string) (data []byte, started bool, err error) {
	cfg = defaults(cfg)
	ctx, cancel := context.WithTimeout(parent, 4*time.Second)
	defer cancel()
	if err = ctx.Err(); err != nil {
		return
	}
	select {
	case cfg.serviceSlot <- struct{}{}:
	default:
		err = errors.New("previous helper is still being reaped")
		return
	}
	command := cfg.Command(path, args...)
	command.Env = cfg.Environment
	command.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	var output limitedServiceOutput
	command.Stdout = &output
	command.Stderr = io.Discard
	if err = ctx.Err(); err != nil {
		<-cfg.serviceSlot
		return
	}
	if err = command.Start(); err != nil {
		<-cfg.serviceSlot
		return
	}
	started = true
	done := make(chan error, 1)
	go func() { err := command.Wait(); <-cfg.serviceSlot; done <- err }()
	select {
	case err = <-done:
	case <-ctx.Done():
		_ = command.Process.Kill()
		err = ctx.Err()
		return
	}
	if output.overflow {
		err = errors.New("provider output exceeds limit")
		return
	}
	data = append([]byte(nil), output.data[:output.size]...)
	return
}

func decodeProvider(data []byte, value any) error {
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(value); err != nil {
		return err
	}
	var extra any
	if decoder.Decode(&extra) != io.EOF {
		return errors.New("extra provider output")
	}
	return nil
}

func discoverProviders(ctx context.Context, cfg Config) (providerPlans, error) {
	var plans providerPlans
	data, _, err := boundedProviderChild(ctx, cfg, cfg.SelfPath, ProviderWorkerArgument)
	if err == nil {
		err = decodeProvider(data, &plans)
	}
	if err == nil && (!validPlan(plans.Poweroff) || !validPlan(plans.Reboot) || !validPlan(plans.Locker)) {
		err = errors.New("invalid fixed provider plans")
	}
	return plans, err
}

func unavailablePower(provider, generation, reason string) sessionservice.PowerObservation {
	return sessionservice.PowerObservation{Provider: provider, Generation: generation, State: "unavailable", Detail: detail(reason), Capabilities: []sessionservice.PowerCapability{
		{Action: "poweroff", Permission: "na"}, {Action: "reboot", Permission: "na"}, {Action: "suspend", Permission: "na", Detail: "secure locker readiness is unconfirmed"},
	}}
}

type powerHelperReply struct {
	Version      int                              `json:"version"`
	Kind         string                           `json:"kind"`
	Available    bool                             `json:"available"`
	State        string                           `json:"state"`
	BusID        string                           `json:"bus_id"`
	ServerOwner  string                           `json:"server_owner"`
	Detail       string                           `json:"detail"`
	Capabilities []sessionservice.PowerCapability `json:"capabilities"`
}

func loginPower(ctx context.Context, cfg Config, observation sessionservice.PowerObservation, action string) (sessionservice.PowerObservation, bool, error) {
	if !filepath.IsAbs(cfg.BusHelperPath) || filepath.Clean(cfg.BusHelperPath) != cfg.BusHelperPath {
		return observation, false, errors.New("optional power helper is not installed")
	}
	args := []string{"power-probe"}
	if action != "" {
		args = []string{"power-request", action, observation.ServerOwner, observation.BusID}
	}
	data, started, err := boundedProviderChild(ctx, cfg, cfg.BusHelperPath, args...)
	if err != nil {
		return observation, started, err
	}
	var reply powerHelperReply
	if err = decodeProvider(data, &reply); err != nil {
		return observation, started, err
	}
	result := observation
	result.State, result.Detail, result.BusID, result.ServerOwner, result.Capabilities = reply.State, reply.Detail, reply.BusID, reply.ServerOwner, reply.Capabilities
	if reply.Version != 1 || reply.Kind != "power" {
		return observation, started, errors.New("invalid power helper version/kind")
	}
	if err = result.Validate(); err != nil {
		return observation, started, err
	}
	if reply.Available && reply.State == "ready" && (reply.BusID == "" || reply.ServerOwner == "") {
		return observation, started, errors.New("missing power provider identity")
	}
	if action != "" && reply.State == "requested" && (reply.BusID != observation.BusID || reply.ServerOwner != observation.ServerOwner) {
		return observation, started, errors.New("power acknowledgment changed the pinned provider identity")
	}
	if !reply.Available && reply.State != "unavailable" && reply.State != "failed" {
		return observation, started, errors.New("inconsistent power provider availability")
	}
	return result, started, nil
}

type ownedLocker struct {
	command *exec.Cmd
	done    <-chan error
}

func startLocker(ctx context.Context, cfg Config, plan []string) (*ownedLocker, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	if len(plan) == 0 {
		return nil, errors.New("xlock/slock is unavailable")
	}
	command := cfg.Command(plan[0], plan[1:]...)
	command.Env = cfg.Environment
	command.Stdin, command.Stdout, command.Stderr = nil, io.Discard, io.Discard
	command.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	if err := command.Start(); err != nil {
		return nil, err
	}
	// From Start onward this record is delivered regardless of requester context.
	// Exactly one waiter owns the child; no lifecycle path signals the locker.
	done := make(chan error, 1)
	go func() { done <- command.Wait() }()
	return &ownedLocker{command: command, done: done}, nil
}

func runProviderJob(request controlRequest, cfg Config, power sessionservice.PowerObservation, plans providerPlans) serviceJob {
	job := serviceJob{request: request, outcome: "failed", power: power, plans: plans}
	verb := request.message.Verb
	if verb == "lock" {
		job.locker, job.err = startLocker(request.ctx, cfg, plans.Locker)
		if job.locker != nil {
			job.outcome = "started"
		} else if errors.Is(job.err, context.Canceled) {
			job.outcome = "cancelled"
		}
		return job
	}
	if verb == "power-probe" {
		job.plans, job.err = discoverProviders(request.ctx, cfg)
		if job.err != nil {
			job.power.State = "failed"
			job.power.Detail = detail(job.err.Error())
			return job
		}
		if power.Provider == "login1" {
			job.power, _, job.err = loginPower(request.ctx, cfg, power, "")
		} else {
			job.power = unavailablePower(power.Provider, power.Generation, "native commands unavailable")
			for i, plan := range [][]string{job.plans.Poweroff, job.plans.Reboot} {
				if len(plan) > 0 {
					job.power.Capabilities[i].Permission = "unknown"
					job.power.Capabilities[i].Detail = "installed; authorization unknown; request capable"
					job.power.State = "ready"
					job.power.Detail = "fixed native commands; authorization unknown"
				}
			}
		}
		if job.err == nil {
			job.outcome = job.power.State
		}
		return job
	}
	if verb == "suspend" {
		job.outcome = "unavailable"
		job.err = errors.New("coordinated suspend refused: secure locker readiness is unconfirmed")
		return job
	}
	var started bool
	if power.Provider == "login1" {
		job.power, started, job.err = loginPower(request.ctx, cfg, power, verb)
	} else {
		plan := plans.Poweroff
		if verb == "reboot" {
			plan = plans.Reboot
		}
		if len(plan) == 0 {
			job.outcome = "unavailable"
			job.err = errors.New("fixed native power command unavailable")
			return job
		}
		_, started, job.err = boundedProviderChild(request.ctx, cfg, plan[0], plan[1:]...)
		if job.err == nil {
			job.power.State = "requested"
			job.power.Detail = "native request acknowledged; completion is not observed"
		}
	}
	if job.err != nil {
		job.power.State = "failed"
		if started {
			job.power.State = "outcome-unconfirmed"
		}
		job.power.Detail = detail(fmt.Sprintf("power request: %v; no replay or fallback", job.err))
	}
	job.outcome = job.power.State
	return job
}

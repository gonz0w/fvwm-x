// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"path/filepath"
	"slices"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/filemodel"
	"github.com/fvwmorg/FvwmPrompt/transport"
)

type reuseReply struct {
	Status string          `json:"status"`
	Result json.RawMessage `json:"result"`
	Error  struct {
		Code string `json:"code"`
	} `json:"error"`
}

type reuseClient struct {
	endpoint string
	serial   int
}

func (c *reuseClient) request(ctx context.Context, fields map[string]any) (reuseReply, error) {
	c.serial++
	fields["version"], fields["id"], fields["resource"] = 1, fmt.Sprintf("reuse-%d", c.serial), "files"
	data, err := json.Marshal(fields)
	if err != nil {
		return reuseReply{}, err
	}
	raw, err := transport.Request(ctx, c.endpoint, string(data))
	if err != nil {
		return reuseReply{}, err
	}
	var envelope struct {
		Reply reuseReply `json:"reply"`
	}
	if json.Unmarshal(raw, &envelope) != nil {
		return reuseReply{}, errors.New("invalid browser reply")
	}
	return envelope.Reply, nil
}

// No path lookup runs here. A matching existing browser resolves target bytes
// in its worker. Larger-than-wire requests retain the independent launch path.
func (o options) reusePlan() (string, string, map[string]any, bool, error) {
	if len(o.targets) == 0 {
		return filepath.Clean(o.dir), "refresh", map[string]any{}, true, nil
	}
	parent, err := filemodel.TargetParent(o.targets)
	if err != nil {
		return "", "", nil, false, err
	}
	if len(o.targets) > providerTargetMax {
		return "", "", nil, false, nil
	}
	for _, path := range o.targets {
		if len(path) > providerPathMax {
			return "", "", nil, false, nil
		}
	}
	operation := "reveal"
	if o.properties {
		operation = "inspect"
	}
	values := map[string]any{"paths_bytes": o.targets}
	raw, _ := json.Marshal(values)
	if len(raw) > providerValuesMax {
		return "", "", nil, false, nil
	}
	return parent, operation, values, true, nil
}

// Reuse only an idle browser already observing the requested lexical folder.
// Before submission, absence/busy/nonmatching state can safely fall back. After
// submission, an unknown outcome must return an error, never launch a duplicate.
func reuseBrowser(parent context.Context, endpoint string, o options) (json.RawMessage, bool, error) {
	folder, operation, values, eligible, err := o.reusePlan()
	if err != nil || !eligible {
		return nil, false, err
	}
	ctx, cancel := context.WithTimeout(parent, 10*time.Second)
	defer cancel()
	client := reuseClient{endpoint: endpoint}
	discovery, err := client.request(ctx, map[string]any{"method": "query"})
	if err != nil {
		return nil, false, err
	}
	if discovery.Status != "ok" {
		return nil, false, errors.New("browser discovery refused")
	}
	var providers struct {
		Providers []struct {
			Provider string `json:"provider"`
			Busy     bool   `json:"busy"`
		} `json:"providers"`
	}
	if json.Unmarshal(discovery.Result, &providers) != nil || len(providers.Providers) > 8 {
		return nil, false, errors.New("invalid browser discovery")
	}
	seen := make(map[string]bool)
	for _, provider := range providers.Providers {
		if !hexToken(provider.Provider, 32) || seen[provider.Provider] {
			return nil, false, errors.New("invalid browser identity")
		}
		seen[provider.Provider] = true
		if provider.Busy {
			continue
		}
		for attempt := 0; attempt < 2; attempt++ {
			response, err := client.request(ctx, map[string]any{"method": "query", "provider": provider.Provider, "scope": "state"})
			if err != nil {
				return nil, false, err
			}
			if response.Status != "ok" {
				break
			}
			var state struct {
				Path       []byte   `json:"path_bytes"`
				Generation string   `json:"generation"`
				Busy       bool     `json:"busy"`
				Operations []string `json:"action_operations"`
			}
			if json.Unmarshal(response.Result, &state) != nil || !hexToken(state.Generation, 32) {
				return nil, false, errors.New("invalid browser state")
			}
			if state.Busy || !bytes.Equal(state.Path, []byte(folder)) || !slices.Contains(state.Operations, operation) || !slices.Contains(state.Operations, "present") {
				break
			}
			response, err = client.request(ctx, map[string]any{"method": "action", "provider": provider.Provider, "generation": state.Generation, "operation": operation, "values": values})
			if err != nil {
				return nil, false, errors.New("browser handoff outcome is unknown; inspect the existing browser")
			}
			if response.Status != "ok" {
				if response.Error.Code == "stale_generation" {
					continue
				}
				return nil, false, fmt.Errorf("browser handoff refused: %s", response.Error.Code)
			}
			var accepted struct {
				Applied    bool          `json:"applied"`
				Provider   string        `json:"provider"`
				Generation string        `json:"generation"`
				Work       *providerWork `json:"work"`
			}
			if json.Unmarshal(response.Result, &accepted) != nil || !accepted.Applied || accepted.Provider != provider.Provider || !hexToken(accepted.Generation, 32) || accepted.Work == nil || accepted.Work.Operation != operation || accepted.Work.Status != "accepted" || (!hexToken(accepted.Work.ID, 16) || accepted.Work.ID == "0000000000000000") {
				return nil, false, errors.New("browser handoff outcome is unknown; invalid admission reply")
			}
			if err := client.present(ctx, provider.Provider); err != nil {
				return nil, false, err
			}

			receipt, _ := json.Marshal(map[string]any{"version": 1, "status": "accepted", "provider": provider.Provider, "work": accepted.Work})
			return receipt, true, nil
		}
	}
	return nil, false, nil
}

func (c *reuseClient) present(ctx context.Context, provider string) error {
	for attempt := 0; attempt < 2; attempt++ {
		current, err := c.request(ctx, map[string]any{"method": "query", "provider": provider, "scope": "state"})
		if err != nil || current.Status != "ok" {
			return errors.New("handoff accepted; browser presentation unavailable")
		}
		var state struct {
			Generation string `json:"generation"`
		}
		if json.Unmarshal(current.Result, &state) != nil || !hexToken(state.Generation, 32) {
			return errors.New("handoff accepted; invalid browser state")
		}
		presented, err := c.request(ctx, map[string]any{"method": "action", "provider": provider, "generation": state.Generation, "operation": "present", "values": map[string]any{}})
		if err != nil {
			return errors.New("handoff accepted; browser presentation outcome is unknown")
		}
		if presented.Status == "ok" {
			return nil
		}
		if presented.Error.Code != "stale_generation" {
			break
		}
	}
	return errors.New("handoff accepted; browser presentation refused")
}

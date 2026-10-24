// Copyright (c) 2026 FVWM-X contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package desktopworker

import (
	"context"
	"math"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

type Status struct {
	CPU     *[2]int64 `json:"cpu"`
	Memory  *int64    `json:"memory"`
	Network string    `json:"network"`
	Battery *int      `json:"battery"`
}

func number(value string) (int64, bool) {
	if value == "" {
		return 0, false
	}
	for _, c := range value {
		if c < '0' || c > '9' {
			return 0, false
		}
	}
	n, err := strconv.ParseInt(value, 10, 64)
	return n, err == nil
}
func cpu(values []string, idleIndex int) *[2]int64 {
	if idleIndex < 0 || idleIndex >= len(values) {
		return nil
	}
	var total, idle int64
	for i, value := range values {
		n, ok := number(value)
		if !ok || n > math.MaxInt64-total {
			return nil
		}
		total += n
		if i == idleIndex {
			idle = n
		}
	}
	return &[2]int64{total, idle}
}
func usedMemory(total, free, unit string) *int64 {
	t, ok := number(total)
	if !ok {
		return nil
	}
	f, ok := number(free)
	if !ok {
		return nil
	}
	p, ok := number(unit)
	if !ok || p == 0 || f > math.MaxInt64/p {
		return nil
	}
	used := t - f*p
	if used < 0 {
		used = 0
	}
	return &used
}
func battery(value string) *int {
	n, ok := number(value)
	if !ok || n > 100 {
		return nil
	}
	result := int(n)
	return &result
}

var freePagesPattern = regexp.MustCompile(`(?m)^\s*([0-9]+) pages free$`)

func collectStatus(ctx context.Context, system string, env Environment) Status {
	status := Status{Network: "Unavailable"}
	read := func(path string) string {
		data, err := env.Read(path)
		if err != nil {
			return ""
		}
		return strings.TrimSpace(string(data))
	}
	if system == "Linux" {
		line, _, _ := strings.Cut(read("/proc/stat"), "\n")
		values := strings.Fields(line)
		if len(values) >= 6 && values[0] == "cpu" {
			if len(values) > 9 {
				values = values[:9]
			}
			status.CPU = cpu(values[1:], 3)
			if status.CPU != nil {
				n, ok := number(values[5])
				if !ok || n > math.MaxInt64-status.CPU[1] {
					status.CPU = nil
				} else {
					status.CPU[1] += n
				}
			}
		}
		memory := map[string]string{}
		for _, line := range strings.Split(read("/proc/meminfo"), "\n") {
			key, value, ok := strings.Cut(line, ":")
			fields := strings.Fields(value)
			if ok && len(fields) > 0 {
				memory[key] = fields[0]
			}
		}
		total, valid := number(memory["MemTotal"])
		available := memory["MemAvailable"]
		if available == "" {
			available = memory["MemFree"]
		}
		free, ok := number(available)
		if valid && ok && total <= math.MaxInt64/1024 && free <= math.MaxInt64/1024 {
			status.Memory = usedMemory(strconv.FormatInt(total*1024, 10), available, "1024")
		}
		if names, err := env.Entries("/sys/class/net"); err == nil {
			up := []string{}
			for _, name := range names {
				if len(up) >= 128 {
					break
				}
				if name != "lo" && read(filepath.Join("/sys/class/net", name, "operstate")) == "up" {
					up = append(up, name)
				}
			}
			status.Network = "Disconnected"
			if len(up) > 0 {
				status.Network = strings.Join(up, ", ")
			}
		}
		if names, err := env.Entries("/sys/class/power_supply"); err == nil {
			sum, count := 0, 0
			for _, name := range names {
				if read(filepath.Join("/sys/class/power_supply", name, "type")) == "Battery" {
					if value := battery(read(filepath.Join("/sys/class/power_supply", name, "capacity"))); value != nil {
						sum += *value
						count++
					}
				}
			}
			if count > 0 {
				value := int(math.RoundToEven(float64(sum) / float64(count)))
				status.Battery = &value
			}
		}
	} else if system == "FreeBSD" || system == "OpenBSD" {
		query := func(key string) string { return output(ctx, env, "sysctl", "-n", key) }
		values := strings.FieldsFunc(query("kern.cp_time"), func(c rune) bool { return strings.ContainsRune(", \t\r\n", c) })
		status.CPU = cpu(values, len(values)-1)
		total, pagesize := query("hw.physmem"), query("hw.pagesize")
		var free, level string
		if system == "FreeBSD" {
			free = query("vm.stats.vm.v_free_count")
			level = query("hw.acpi.battery.life")
		} else {
			match := freePagesPattern.FindStringSubmatch(output(ctx, env, "vmstat", "-s"))
			if len(match) == 2 {
				free = match[1]
			}
			state := output(ctx, env, "apm", "-b")
			if state == "0" || state == "1" || state == "2" || state == "3" {
				level = output(ctx, env, "apm", "-l")
			}
		}
		status.Memory = usedMemory(total, free, pagesize)
		status.Battery = battery(level)
		var current string
		active := []string{}
		for _, line := range strings.Split(output(ctx, env, "ifconfig", "-a"), "\n") {
			if line != "" && line[0] != ' ' && line[0] != '\t' {
				current, _, _ = strings.Cut(line, ":")
			} else if strings.Contains(line, "status: active") && !strings.HasPrefix(current, "lo") && current != "" && len(active) < 128 {
				active = append(active, current)
			}
		}
		status.Network = "Disconnected"
		if len(active) > 0 {
			status.Network = strings.Join(active, ", ")
		}
	}
	return status
}

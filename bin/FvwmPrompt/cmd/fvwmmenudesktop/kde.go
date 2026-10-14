// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.
package main

import (
	"context"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/fvwmorg/FvwmPrompt/internal/xdgmenu"
)

// Only KDELegacyDirs requests this optional base-tool inventory. The worker
// owns its deadline and process group; desktop application commands never run
// during acquisition. Missing optional tools leave the legacy inventory empty.
func kdeDirectories(ctx context.Context) ([]string, error) {
	for _, name := range []string{"kde-config", "kde4-config", "kf5-config", "kf6-config"} {
		program, err := exec.LookPath(name)
		if err != nil {
			continue
		}
		command := exec.CommandContext(ctx, program, "--path", "apps")
		command.WaitDelay = 100 * time.Millisecond
		out, diagnostics := &boundedOutput{limit: xdgmenu.MaxSourceBytes}, &boundedOutput{limit: 1024 * 1024}
		command.Stdout = out
		command.Stderr = diagnostics
		if err = command.Run(); err != nil {
			return nil, err
		}
		var dirs []string
		for _, path := range strings.Split(strings.TrimSpace(out.String()), ":") {
			if filepath.IsAbs(path) {
				dirs = append(dirs, filepath.Clean(path))
			}
		}
		return dirs, nil
	}
	return nil, nil
}

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package transport

import (
	"os"
	"path/filepath"
	"strconv"
)

// SocketPath follows the current private MFL endpoint producer.
func SocketPath(explicit string, getenv func(string) string, uid int) string {
	if explicit != "" {
		return explicit
	}
	if name := getenv("FVWMMFL_SOCKET"); name != "" {
		return name
	}
	base := getenv("FVWMMFL_SOCKET_PATH")
	if base != "" {
		base = os.Expand(base, getenv)
		if !filepath.IsAbs(base) {
			base = filepath.Join(getenv("FVWM_USERDIR"), base)
		}
	} else if runtime := getenv("XDG_RUNTIME_DIR"); runtime != "" {
		base = filepath.Join(runtime, "fvwmmfl")
	} else {
		temporary := getenv("TMPDIR")
		if temporary == "" {
			temporary = "/tmp"
		}
		base = filepath.Join(temporary, "fvwmmfl-"+strconv.Itoa(uid))
	}
	display := getenv("DISPLAY")
	if display == "" {
		display = "default"
	}
	name := []byte(display)
	for i, c := range name {
		if !(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' ||
			c >= '0' && c <= '9' || c == '-' || c == '_') {
			name[i] = '_'
		}
	}
	return filepath.Join(base, "fvwm_mfl_"+string(name)+".sock")
}

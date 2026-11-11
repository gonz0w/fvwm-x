// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
package desktopcontroller

import (
	"encoding/hex"
	"errors"
	"strconv"
)

// coreFragment consumes the existing directed core framing. The owning request
// checks its cookie, serial and deadline before sharing this bounded assembler.
func coreFragment(fields []string, data *[]byte, parts *int, limit int) (bool, error) {
	if len(fields) == 3 && fields[0] == "part" && fields[1] == strconv.Itoa(*parts) && len(fields[2]) >= 2 && len(fields[2]) <= 512 && len(fields[2])%2 == 0 && identity(fields[2], len(fields[2])) && len(*data)+len(fields[2])/2 <= limit {
		decoded, _ := hex.DecodeString(fields[2])
		*data = append(*data, decoded...)
		*parts = *parts + 1
		return false, nil
	}
	if len(fields) == 2 && fields[0] == "end" && fields[1] == strconv.Itoa(*parts) && *parts > 0 {
		return true, nil
	}
	return false, errors.New("Malformed or oversized core reply")
}

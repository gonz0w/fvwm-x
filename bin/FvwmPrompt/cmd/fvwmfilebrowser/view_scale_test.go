// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or any later version.

package main

import (
	"strings"
	"testing"
)

// The browser only opts into Form's native policy. Scale observations and
// reflow belong to Form and must not add a second Go-side generation protocol.
func TestBrowserRenderOptsIntoMonitorScaleBeforeControls(t *testing.T) {
	b, peer := layoutBrowser(t, "list")
	b.renderPath = "/old-folder"
	scaleCommands, notify := 0, ""
	_, err := layoutScript(t, b, peer, b.show, func(command string) string {
		if strings.Contains(command, ": UIScale ") {
			if !strings.HasSuffix(command, ": UIScale monitor") {
				t.Fatal("unexpected native scale policy", command)
			}
			scaleCommands++
		}
		for _, control := range []string{": Title ", ": Label ", ": Input ", ": Button ", ": List "} {
			if strings.Contains(command, control) && scaleCommands != 1 {
				t.Fatal("scale policy must precede the rendered controls", command)
			}
		}
		if strings.Contains(command, ": ListNotify ") {
			notify = strings.Fields(command)[2]
		}
		if strings.HasPrefix(command, "Module FvwmForm ") {
			if notify == "" {
				t.Fatal("missing existing collection observation")
			}
			return layoutReply("SendToModule unused ListStatus picks "+notify+" selection", 1, 0, []byte{0, 1, 1})
		}
		return ""
	})
	if err != nil || scaleCommands != 1 {
		t.Fatal("browser did not emit one native monitor-scale opt-in", err, scaleCommands)
	}
}

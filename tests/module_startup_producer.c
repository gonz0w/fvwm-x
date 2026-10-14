/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include "libs/Module.h"

int main(int argc, char **argv)
{
	assert(argc == 7);
	int descriptors[2] = { atoi(argv[1]), atoi(argv[2]) };

	if (strcmp(argv[6], "escape") == 0)
	{
		Display *display = XOpenDisplay(NULL);
		assert(display != NULL);
		assert(poll(NULL, 0, 150) == 0);
		XEvent event = { 0 };
		event.xkey.type = KeyPress;
		event.xkey.display = display;
		event.xkey.window = DefaultRootWindow(display);
		event.xkey.root = event.xkey.window;
		event.xkey.keycode = XKeysymToKeycode(display, XK_Escape);
		event.xkey.same_screen = True;
		assert(XSendEvent(display, event.xkey.window, False,
			KeyPressMask, &event) != 0);
		XFlush(display);
		assert(poll(NULL, 0, 500) == 0);
		XCloseDisplay(display);
		return 0;
	}
	assert(strcmp(argv[6], "activity") == 0);
	for (unsigned i = 0; i < 8; i++)
	{
		SendText(descriptors, "NOP", None);
		assert(poll(NULL, 0, 250) == 0);
	}
	SendText(descriptors, ModuleFinishedStartupResponse, None);
	return 0;
}

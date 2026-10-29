/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 *
 * Independent eventual application for xdg_installed_live.py. All observations
 * are byte strings; this application knows no launcher or observer protocol.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define ARGUMENT_LIMIT 128
#define VALUE_LIMIT 16384
#define TOTAL_LIMIT 32768

static void print_hex(const char *value)
{
	const unsigned char *cursor = (const unsigned char *)value;

	putchar('"');
	while (*cursor != 0)
		printf("%02x", *cursor++);
	putchar('"');
}

int main(int argc, char **argv)
{
	Display *display = NULL;
	Window window = None;
	const char *startup = getenv("DESKTOP_STARTUP_ID");
	const char *display_name = getenv("DISPLAY");
	char cwd[VALUE_LIMIT], command[16];
	size_t total = 0, length;
	int i, result = 0;
	int no_x = argc > 1 && strcmp(argv[1], "--no-x") == 0;

	if (argc > ARGUMENT_LIMIT || getcwd(cwd, sizeof(cwd)) == NULL)
	{
		fputs("installed fixture argument/cwd limit\n", stderr);
		return 2;
	}
	for (i = 1; i < argc; i++)
	{
		length = strlen(argv[i]);
		if (length > VALUE_LIMIT || length > TOTAL_LIMIT - total)
		{
			fputs("installed fixture argument byte limit\n", stderr);
			return 2;
		}
		total += length;
	}
	if (startup != NULL && strlen(startup) > 4096)
	{
		fputs("installed fixture startup ID limit\n", stderr);
		return 2;
	}
	if (!no_x && display_name != NULL && *display_name != 0)
	{
		display = XOpenDisplay(NULL);
		if (display == NULL)
		{
			fputs("installed fixture cannot open display\n", stderr);
			return 2;
		}
		window = XCreateSimpleWindow(display, DefaultRootWindow(display),
			40, 40, 240, 100, 0, 0, 0xdddddd);
		XStoreName(display, window, "Installed XDG fixture application");
		if (startup != NULL)
			XChangeProperty(display, window,
				XInternAtom(display, "_NET_STARTUP_ID", False),
				XInternAtom(display, "UTF8_STRING", False), 8,
				PropModeReplace, (const unsigned char *)startup,
				(int)strlen(startup));
		XMapWindow(display, window);
		XSync(display, False);
	}
	printf("{\"pid\":%ld,\"window\":%lu,\"argv_hex\":[", (long)getpid(),
		(unsigned long)window);
	for (i = 1; i < argc; i++)
	{
		if (i != 1)
			putchar(',');
		print_hex(argv[i]);
	}
	fputs("],\"cwd_hex\":", stdout);
	print_hex(cwd);
	fputs(",\"startup_id_hex\":", stdout);
	if (startup != NULL)
		print_hex(startup);
	else
		fputs("null", stdout);
	fputs("}\n", stdout);
	if (fflush(stdout) != 0)
		result = 2;
	/* EOF and an explicit quit both release this directly owned application. */
	if (result == 0 && fgets(command, sizeof(command), stdin) != NULL &&
	    strcmp(command, "quit\n") != 0)
	{
		fputs("installed fixture invalid quit command\n", stderr);
		result = 2;
	}
	if (ferror(stdin))
		result = 2;
	if (display != NULL)
	{
		XDestroyWindow(display, window);
		XCloseDisplay(display);
	}
	return result;
}

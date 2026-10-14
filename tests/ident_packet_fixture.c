/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "fvwm/fvwm.h"
#include "libs/Module.h"
#include "libs/vpacket.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	unsigned long storage[FvwmPacketMaxSize] = { 0 };
	ConfigWinPacket configuration = { 0 };
	char *end;
	unsigned long window;
	size_t words = (sizeof(configuration) + sizeof(unsigned long) - 1) /
	    sizeof(unsigned long);

	if (argc != 2)
	{
		return 1;
	}
	errno = 0;
	window = strtoul(argv[1], &end, 0);
	if (errno || end == argv[1] || *end || !window ||
	    words > FvwmPacketBodyMaxSize)
	{
		return 1;
	}
	configuration.w = configuration.frame = window;
	configuration.frame_width = 320;
	configuration.frame_height = 240;
	configuration.hints_width_inc = configuration.hints_height_inc = 1;
	configuration.orig_hints_width_inc = 1;
	configuration.orig_hints_height_inc = 1;
	configuration.hints_min_width = configuration.hints_min_height = 1;
	configuration.hints_max_width = configuration.hints_max_height = 65535;
	configuration.hints_win_gravity = NorthWestGravity;
	storage[0] = START_FLAG;
	storage[1] = M_CONFIGURE_WINDOW;
	storage[2] = FvwmPacketHeaderSize + words;
	memcpy(storage + FvwmPacketHeaderSize, &configuration,
	    sizeof(configuration));
	return fwrite(storage, sizeof(unsigned long), storage[2], stdout) ==
	    storage[2] ? 0 : 1;
}
